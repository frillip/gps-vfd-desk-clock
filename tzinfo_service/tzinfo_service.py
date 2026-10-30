#!/usr/bin/env python3
"""Timezone information service for Smol Clock v2+.

Public endpoint:
    GET /smol_clock/api/v1/tzinfo
    GET /smol_clock/api/v1/tzinfo?lat=<latitude>&lon=<longitude>
    GET /smol_clock/api/v1/special
    GET /smol_clock/api/v1/special?id=<special_id>
    GET /smol_clock/api/v1/special?lat=<latitude>&lon=<longitude>

The service converts either supplied coordinates or GeoIP-derived coordinates
into an IANA timezone, then converts IANA/tzdata rules into the deliberately
small embedded protocol used by Smol Clock.

Protocol semantics:
    effective UTC offset = offset + (dst_offset if dst else 0)

``offset`` and ``dst_offset`` therefore remain distinct.  ``current`` contains
the state currently in force and the UTC epoch at which that state most
recently became effective.  ``next`` contains future state transitions.
"""

from __future__ import annotations

import ipaddress
import logging
import math
import os
import re
import shutil
import subprocess
import time as monotonic_time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

import maxminddb
from flask import Flask, jsonify, request
from timezonefinder import TimezoneFinder
from werkzeug.exceptions import HTTPException


API_PATH = "/smol_clock/api/v1/tzinfo"
SPECIAL_PATH = "/smol_clock/api/v1/special"
HEALTH_PATH = "/smol_clock/api/v1/health"
READY_PATH = "/smol_clock/api/v1/ready"

PROTOCOL_VERSION = 1
UNIX_EPOCH = 0
DEFAULT_DB_PATH = "GeoLite2-City.mmdb"
DEFAULT_TRANSITION_YEARS = 5
DEFAULT_MAX_TRANSITIONS = 8
DEFAULT_VALIDITY_SECONDS = 7 * 24 * 60 * 60
DEFAULT_MMDB_CHECK_INTERVAL = 60
DEFAULT_LEAP_SECONDS_PATH = "/usr/share/zoneinfo/leap-seconds.list"
NTP_UNIX_EPOCH_DELTA = 2208988800
EARTH_RADIUS_M = 6371008.8
MAX_TRANSITION_YEARS = 20
MAX_TRANSITIONS = 40

# zdump -V output produced by tzcode/glibc. utc_date is explicitly UTC.
ZDUMP_RE = re.compile(
    r"^(?P<zone>\S+)\s+"
    r"(?P<utc_date>\w{3} \w{3}\s+\d{1,2} \d{2}:\d{2}:\d{2} \d{4}) UT = "
    r"(?P<local_date>\w{3} \w{3}\s+\d{1,2} \d{2}:\d{2}:\d{2} \d{4}) "
    r"(?P<abbr>\S+) isdst=(?P<isdst>[01]) gmtoff=(?P<gmtoff>-?\d+)"
)


def make_logger() -> logging.Logger:
    """Create the journald-facing application logger."""
    log = logging.getLogger("smol_clock.tzinfo")
    if log.handlers:
        return log

    level_name = os.getenv("TZINFO_LOG_LEVEL", "INFO").upper()
    level = getattr(logging, level_name, logging.INFO)
    formatter = logging.Formatter(
        "%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%dT%H:%M:%S%z",
    )

    stream = logging.StreamHandler()
    stream.setFormatter(formatter)
    log.addHandler(stream)
    log.setLevel(level)
    return log


logger = make_logger()


class ServiceError(Exception):
    """Expected client/service error that can be rendered as JSON."""

    def __init__(self, status: int, code: str, message: str):
        super().__init__(message)
        self.status = status
        self.code = code
        self.message = message


@dataclass(frozen=True)
class TimezoneState:
    """Timezone state currently in force.

    transition_utc is the UTC epoch at which this state most recently became
    effective.  A value of 0 means no transition at or after the Unix epoch
    could be established (for example Etc/UTC or a zone whose last change was
    before 1970).
    """

    transition_utc: int
    offset: int
    dst_offset: int
    dst: bool

    def as_dict(self) -> dict[str, Any]:
        return {
            "dst": self.dst,
            "dst_offset": self.dst_offset,
            "transition_utc": self.transition_utc,
            "offset": self.offset,
        }


@dataclass(frozen=True)
class Transition:
    """Timezone state that becomes effective at transition_utc."""

    transition_utc: int
    offset: int
    dst_offset: int
    dst: bool

    def as_dict(self) -> dict[str, Any]:
        return {
            "dst": self.dst,
            "dst_offset": self.dst_offset,
            "transition_utc": self.transition_utc,
            "offset": self.offset,
        }


@dataclass(frozen=True)
class SpecialLocation:
    lat: float
    lon: float
    radius_m: float


@dataclass(frozen=True)
class SpecialTimebase:
    id: str
    name: str
    offset_s: int
    offset_ns: int
    locations: tuple[SpecialLocation, ...]

    def as_state_dict(self) -> dict[str, Any]:
        return {
            "dst": False,
            "dst_offset": 0,
            "offset_s": self.offset_s,
            "offset_ns": self.offset_ns,
            "transition_utc": 0,
        }


SPECIAL_TIMEBASES: dict[str, SpecialTimebase] = {
    "gmt": SpecialTimebase(
        id="gmt",
        name="Greenwich Mean Time",
        offset_s=0,
        offset_ns=0,
        locations=(SpecialLocation(51.477890, -0.001468, 300.0),),
    ),
    "dublin_mean": SpecialTimebase(
        id="dublin_mean",
        name="Dublin Mean Time",
        offset_s=-1521,
        offset_ns=0,
        locations=(SpecialLocation(53.38708, -6.33756, 300.0),),
    ),
    "paris_meridian": SpecialTimebase(
        id="paris_meridian",
        name="Paris Meridian Time",
        offset_s=561,
        offset_ns=0,
        locations=(
            SpecialLocation(48.8364389, 2.3365056, 300.0),
            SpecialLocation(48.861027, 2.335708, 500.0),
        ),
    ),
    "mecca_time": SpecialTimebase(
        id="mecca_time",
        name="Mecca Time",
        offset_s=9558,
        offset_ns=300000000,
        locations=(SpecialLocation(21.4224779, 39.8251832, 1000.0),),
    ),
}

SPECIAL_ZONE_NAMES: dict[str, str] = {
    "gmt": "Special/GMT",
    "dublin_mean": "Special/DMT",
    "paris_meridian": "Special/Paris_Meridian",
    "mecca_time": "Special/Mecca",
    "tai": "Special/TAI",
}

SPECIAL_NONE: dict[str, Any] = {
    "dst": False,
    "dst_offset": 0,
    "offset_s": 0,
    "offset_ns": 0,
    "transition_utc": 0,
}


class LeapSecondTable:
    """Read UTC/TAI data from the NTP leap-seconds.list distributed by tzdata.

    The file's DTAI column is TAI-UTC.  state() returns the opposite sign,
    UTC-TAI (-37 at present); the /special TAI record carries TAI-UTC (+37)
    as its offset, since special offsets are added to UTC.
    """

    def __init__(self, path: str):
        self.path = Path(path)

    def _read(self) -> tuple[list[tuple[int, int]], int | None]:
        try:
            text = self.path.read_text(encoding="ascii")
        except (FileNotFoundError, PermissionError, OSError) as exc:
            raise ServiceError(
                503,
                "leap_seconds_unavailable",
                f"Leap-second data is unavailable at {self.path}",
            ) from exc

        entries: list[tuple[int, int]] = []
        expires_unix: int | None = None
        for raw in text.splitlines():
            line = raw.strip()
            if not line:
                continue
            if line.startswith("#@"):
                parts = line[2:].strip().split()
                if parts:
                    try:
                        expires_unix = int(parts[0]) - NTP_UNIX_EPOCH_DELTA
                    except ValueError:
                        pass
                continue
            if line.startswith("#"):
                continue
            match = re.match(r"^(\d+)\s+(\d+)(?:\s+#.*)?$", line)
            if not match:
                continue
            ntp_epoch = int(match.group(1))
            dtai = int(match.group(2))
            entries.append((ntp_epoch - NTP_UNIX_EPOCH_DELTA, dtai))

        if not entries:
            raise ServiceError(
                503,
                "leap_seconds_invalid",
                f"No leap-second entries were found in {self.path}",
            )
        entries.sort()
        return entries, expires_unix

    def state(self, epoch: int) -> tuple[int, int | None, int | None]:
        """Return (UTC-TAI, next DTAI change epoch, file expiration epoch)."""
        entries, expires_unix = self._read()
        current_dtai: int | None = None
        next_change: int | None = None
        for effective_utc, dtai in entries:
            if effective_utc <= epoch:
                current_dtai = dtai
            else:
                next_change = effective_utc
                break
        if current_dtai is None:
            raise ServiceError(
                422,
                "tai_undefined",
                "TAI offset is not defined by the leap-second table at the requested epoch",
            )
        if expires_unix is not None and epoch >= expires_unix:
            raise ServiceError(
                503,
                "leap_seconds_expired",
                "Installed leap-second data has expired",
            )
        return -current_dtai, next_change, expires_unix

    def ready(self, epoch: int) -> bool:
        try:
            self.state(epoch)
        except ServiceError:
            return False
        return True


class TzInfoService:
    def __init__(
        self,
        db_path: str,
        transition_years: int,
        max_transitions: int,
        mmdb_check_interval: int = DEFAULT_MMDB_CHECK_INTERVAL,
        leap_seconds_path: str = DEFAULT_LEAP_SECONDS_PATH,
    ):
        self.db_path = Path(db_path)
        self.transition_years = transition_years
        self.max_transitions = max_transitions
        self.mmdb_check_interval = max(0, mmdb_check_interval)
        self.timezone_finder = TimezoneFinder(in_memory=True)
        self.leap_seconds = LeapSecondTable(leap_seconds_path)

        self.geoip_reader: maxminddb.Reader | None = None
        self._geoip_signature: tuple[int, int, int] | None = None
        self._last_mmdb_check = 0.0

        if not shutil.which("zdump"):
            raise RuntimeError("zdump executable was not found in PATH")

        self._reload_geoip_if_changed(force=True)

    def close(self) -> None:
        if self.geoip_reader is not None:
            self.geoip_reader.close()
            self.geoip_reader = None
        self._geoip_signature = None

    def _mmdb_signature(self) -> tuple[int, int, int] | None:
        try:
            stat = self.db_path.stat()
        except FileNotFoundError:
            return None
        except OSError as exc:
            logger.warning("Unable to stat GeoIP database %s: %s", self.db_path, exc)
            return None
        return stat.st_ino, stat.st_mtime_ns, stat.st_size

    def _reload_geoip_if_changed(self, force: bool = False) -> None:
        """Open a newly installed MMDB without disrupting a known-good reader."""
        now = monotonic_time.monotonic()
        if not force and now - self._last_mmdb_check < self.mmdb_check_interval:
            return
        self._last_mmdb_check = now

        signature = self._mmdb_signature()
        if signature == self._geoip_signature:
            return

        if signature is None:
            if self.geoip_reader is None:
                logger.warning(
                    "GeoIP database not found at %s; coordinate queries remain available",
                    self.db_path,
                )
            else:
                logger.warning(
                    "GeoIP database %s disappeared; retaining currently open database",
                    self.db_path,
                )
            return

        try:
            new_reader = maxminddb.open_database(str(self.db_path))
        except Exception as exc:
            logger.error(
                "Unable to open replacement GeoIP database %s; retaining current database: %s",
                self.db_path,
                exc,
            )
            return

        old_reader = self.geoip_reader
        self.geoip_reader = new_reader
        self._geoip_signature = signature
        if old_reader is not None:
            old_reader.close()
        logger.info("Loaded GeoIP database: %s", self.db_path)

    def geoip_ready(self) -> bool:
        self._reload_geoip_if_changed()
        return self.geoip_reader is not None

    def timezone_from_coordinates(self, lat: float, lon: float) -> str:
        zone = self.timezone_finder.timezone_at(lat=lat, lng=lon)
        if zone is None:
            raise ServiceError(
                422,
                "timezone_not_found",
                "No IANA timezone could be resolved for the supplied coordinates",
            )
        return zone

    def coordinates_from_ip(self, ip_text: str) -> tuple[float, float]:
        self._reload_geoip_if_changed()
        if self.geoip_reader is None:
            raise ServiceError(
                503,
                "geoip_unavailable",
                "GeoIP lookup is unavailable because the GeoLite2 City database is not loaded",
            )

        try:
            ip = ipaddress.ip_address(ip_text)
        except ValueError as exc:
            raise ServiceError(400, "invalid_client_ip", "Client IP address is invalid") from exc

        if ip.is_loopback or ip.is_private or ip.is_link_local or ip.is_unspecified:
            raise ServiceError(
                422,
                "geoip_unresolvable",
                "Client IP address is not publicly geolocatable",
            )

        data = self.geoip_reader.get(str(ip))
        if not data or "location" not in data:
            raise ServiceError(404, "geoip_not_found", "No GeoIP location data was found for the client IP")

        location = data["location"]
        lat = location.get("latitude")
        lon = location.get("longitude")
        if lat is None or lon is None:
            raise ServiceError(404, "geoip_not_found", "GeoIP record does not contain coordinates")

        return float(lat), float(lon)

    @staticmethod
    def _zoneinfo_state(zone_name: str, epoch: int) -> tuple[int, int, bool]:
        """Return (effective_offset, active_dst_delta, dst_active)."""
        try:
            tz = ZoneInfo(zone_name)
        except ZoneInfoNotFoundError as exc:
            raise ServiceError(500, "timezone_database_error", f"IANA timezone is unavailable: {zone_name}") from exc

        local = datetime.fromtimestamp(epoch, tz=timezone.utc).astimezone(tz)
        effective_delta = local.utcoffset()
        dst_delta = local.dst()
        if effective_delta is None:
            raise ServiceError(500, "timezone_database_error", "Timezone returned no UTC offset")

        effective = int(effective_delta.total_seconds())
        active_dst_delta = int(dst_delta.total_seconds()) if dst_delta is not None else 0
        return effective, active_dst_delta, active_dst_delta != 0

    @staticmethod
    def _parse_zdump(output: str) -> list[dict[str, Any]]:
        """Parse zdump records without host-local-time dependence."""
        records: list[dict[str, Any]] = []
        for raw_line in output.splitlines():
            match = ZDUMP_RE.match(raw_line.strip())
            if not match:
                continue
            utc_dt = datetime.strptime(
                match.group("utc_date"), "%a %b %d %H:%M:%S %Y"
            ).replace(tzinfo=timezone.utc)
            records.append(
                {
                    "transition_utc": int(utc_dt.timestamp()),
                    "isdst": bool(int(match.group("isdst"))),
                    "gmtoff": int(match.group("gmtoff")),
                }
            )
        return records

    def _zdump_records(self, zone_name: str, start_epoch: int, end_epoch: int) -> list[dict[str, Any]]:
        try:
            result = subprocess.run(
                ["zdump", "-V", "-t", f"{start_epoch},{end_epoch}", zone_name],
                capture_output=True,
                text=True,
                check=True,
                timeout=5,
            )
        except subprocess.TimeoutExpired as exc:
            raise ServiceError(504, "zdump_timeout", "Timezone transition lookup timed out") from exc
        except subprocess.CalledProcessError as exc:
            detail = (exc.stderr or "").strip()
            logger.error("zdump failed for %s: %s", zone_name, detail)
            raise ServiceError(500, "zdump_failed", "Timezone transition lookup failed") from exc
        except OSError as exc:
            logger.exception("Unable to execute zdump")
            raise ServiceError(500, "zdump_failed", "Unable to execute timezone transition lookup") from exc
        return self._parse_zdump(result.stdout)

    @staticmethod
    def _boundary_pairs(records: list[dict[str, Any]]) -> list[tuple[dict[str, Any], dict[str, Any]]]:
        boundaries: list[tuple[dict[str, Any], dict[str, Any]]] = []
        for before, after in zip(records, records[1:]):
            if after["transition_utc"] != before["transition_utc"] + 1:
                continue
            if before["gmtoff"] == after["gmtoff"] and before["isdst"] == after["isdst"]:
                continue
            boundaries.append((before, after))
        return boundaries

    @staticmethod
    def _dst_adjustment_from_pair(before: dict[str, Any], after: dict[str, Any]) -> int | None:
        """Return the seasonal adjustment represented by a DST toggle pair."""
        if before["isdst"] == after["isdst"]:
            return None
        if after["isdst"]:
            return after["gmtoff"] - before["gmtoff"]
        return before["gmtoff"] - after["gmtoff"]

    def _state_components(
        self,
        zone_name: str,
        epoch: int,
        boundary_pairs: list[tuple[dict[str, Any], dict[str, Any]]],
    ) -> tuple[int, int, bool]:
        """Return protocol (base offset, DST adjustment, DST active) at epoch."""
        effective, active_delta, dst_active = self._zoneinfo_state(zone_name, epoch)
        if dst_active:
            return effective - active_delta, active_delta, True

        # When DST is inactive, zoneinfo.dst() is zero. Preserve the protocol's
        # seasonal-adjustment field by recovering the nearest DST toggle rule.
        candidates: list[tuple[int, int]] = []
        for before, after in boundary_pairs:
            adjustment = self._dst_adjustment_from_pair(before, after)
            if adjustment is None:
                continue
            candidates.append((abs(after["transition_utc"] - epoch), adjustment))

        dst_adjustment = min(candidates, default=(0, 0), key=lambda item: item[0])[1]
        return effective, dst_adjustment, False

    def snapshot(self, zone_name: str, now_epoch: int) -> tuple[TimezoneState, list[Transition]]:
        """Build current and future timezone states from one zdump invocation.

        zdump history starts at the Unix epoch so current.transition_utc can be
        the actual most recent representable transition.  If there has been no
        transition since 1970, transition_utc is the defined sentinel 0.
        """
        end_epoch = now_epoch + int(self.transition_years * 365.2425 * 86400)
        records = self._zdump_records(zone_name, UNIX_EPOCH, end_epoch)
        boundaries = self._boundary_pairs(records)

        current_transition_utc = UNIX_EPOCH
        for _before, after in boundaries:
            if after["transition_utc"] <= now_epoch:
                current_transition_utc = after["transition_utc"]
            else:
                break

        current_offset, current_dst_offset, current_dst = self._state_components(
            zone_name, now_epoch, boundaries
        )
        current = TimezoneState(
            transition_utc=current_transition_utc,
            offset=current_offset,
            dst_offset=current_dst_offset,
            dst=current_dst,
        )

        future: list[Transition] = []
        for _before, after in boundaries:
            transition_utc = after["transition_utc"]
            if transition_utc <= now_epoch:
                continue
            offset, dst_offset, dst = self._state_components(
                zone_name, transition_utc, boundaries
            )
            future.append(
                Transition(
                    transition_utc=transition_utc,
                    offset=offset,
                    dst_offset=dst_offset,
                    dst=dst,
                )
            )
            if len(future) >= self.max_transitions:
                break

        return current, future

    @staticmethod
    def _distance_m(lat_a: float, lon_a: float, lat_b: float, lon_b: float) -> float:
        """Great-circle distance between two WGS84 latitude/longitude points."""
        phi1 = math.radians(lat_a)
        phi2 = math.radians(lat_b)
        d_phi = math.radians(lat_b - lat_a)
        d_lambda = math.radians(lon_b - lon_a)
        a = (
            math.sin(d_phi / 2.0) ** 2
            + math.cos(phi1) * math.cos(phi2) * math.sin(d_lambda / 2.0) ** 2
        )
        return 2.0 * EARTH_RADIUS_M * math.atan2(math.sqrt(a), math.sqrt(1.0 - a))

    def special_for_coordinates(self, lat: float, lon: float) -> SpecialTimebase | None:
        """Return the first configured special timebase whose geofence contains the point."""
        for special in SPECIAL_TIMEBASES.values():
            for location in special.locations:
                if self._distance_m(lat, lon, location.lat, location.lon) <= location.radius_m:
                    return special
        return None

    @staticmethod
    def _state_as_special(state: TimezoneState | Transition) -> dict[str, Any]:
        return {
            "dst": state.dst,
            "dst_offset": state.dst_offset,
            "offset_s": state.offset,
            "offset_ns": 0,
            "transition_utc": state.transition_utc,
        }

    def build_special_response(
        self,
        now_epoch: int,
        special_id: str | None = None,
        lat: float | None = None,
        lon: float | None = None,
    ) -> dict[str, Any]:
        """Build the special-timebase response.

        Explicit id selection overrides geofencing. Coordinates are used only
        to discover special geofences; ordinary IANA timezone resolution is
        handled independently by /tzinfo. Without either, nothing can match.

        When no special geofence matches, the response is Special/None with an
        all-zero special state. TAI is returned as Special/TAI with a positive
        offset because special offsets are added to UTC to obtain displayed
        time: TAI = UTC + (TAI-UTC). Only TAI needs the leap-second table, so
        only TAI fails without it, and only TAI's validity is bounded by its
        next change or its expiry.
        """
        valid_until = now_epoch + DEFAULT_VALIDITY_SECONDS
        selected: SpecialTimebase | None = None

        if special_id == "tai":
            utc_minus_tai, next_tai_change, leap_expiry = self.leap_seconds.state(now_epoch)
            if next_tai_change is not None:
                valid_until = min(valid_until, next_tai_change)
            if leap_expiry is not None:
                valid_until = min(valid_until, leap_expiry)
            special_records: list[dict[str, Any]] = [{
                "dst": False,
                "dst_offset": 0,
                "offset_s": -utc_minus_tai,
                "offset_ns": 0,
                "transition_utc": 0,
            }]
            zone = SPECIAL_ZONE_NAMES["tai"]
        else:
            if special_id is not None:
                selected = SPECIAL_TIMEBASES.get(special_id)
                if selected is None:
                    raise ServiceError(
                        400,
                        "invalid_special",
                        "Unknown special timebase; expected gmt, dublin_mean, paris_meridian, mecca_time, or tai",
                    )
            elif lat is not None and lon is not None:
                selected = self.special_for_coordinates(lat, lon)

            if selected is not None:
                special_records = [selected.as_state_dict()]
                zone = SPECIAL_ZONE_NAMES[selected.id]
            else:
                special_records = [dict(SPECIAL_NONE)]
                zone = "Special/None"

        return {
            "special": special_records,
            "generated": now_epoch,
            "next": [],
            "valid_until": valid_until,
            "version": PROTOCOL_VERSION,
            "zone": zone,
        }

    def build_response(self, zone_name: str, source: str, now_epoch: int) -> dict[str, Any]:
        current, next_transitions = self.snapshot(zone_name, now_epoch)
        return {
            "generated": now_epoch,
            "current": [current.as_dict()],
            "next": [item.as_dict() for item in next_transitions],
            "source": source,
            "valid_until": now_epoch + DEFAULT_VALIDITY_SECONDS,
            "version": PROTOCOL_VERSION,
            "zone": zone_name,
        }


def parse_float_arg(name: str) -> float | None:
    value = request.args.get(name)
    if value is None:
        return None
    try:
        return float(value)
    except ValueError as exc:
        raise ServiceError(400, f"invalid_{name}", f"{name} must be a decimal number") from exc


def validate_coordinates(lat: float, lon: float) -> None:
    if not -90.0 <= lat <= 90.0:
        raise ServiceError(400, "invalid_lat", "lat must be between -90 and 90 degrees")
    if not -180.0 <= lon <= 180.0:
        raise ServiceError(400, "invalid_lon", "lon must be between -180 and 180 degrees")


def get_client_ip(trust_proxy: bool) -> str:
    """Return client IP; forwarded headers are trusted only when configured.

    The last X-Forwarded-For address is the one the trusted proxy added.
    Earlier ones came from the client and could say anything: Apache appends
    to a header the client sent, nginx (as configured here) replaces it, and
    the last address is right for both.
    """
    remote = request.remote_addr or ""
    if not trust_proxy:
        return remote

    forwarded = request.headers.get("X-Forwarded-For", "")
    if not forwarded:
        return remote

    candidate = forwarded.rsplit(",", 1)[-1].strip()
    try:
        ipaddress.ip_address(candidate)
    except ValueError:
        logger.warning("Ignoring invalid X-Forwarded-For value")
        return remote
    return candidate


def _env_int(name: str, default: int, minimum: int, maximum: int) -> int:
    raw = os.getenv(name)
    if raw is None:
        return default
    try:
        value = int(raw)
    except ValueError as exc:
        raise RuntimeError(f"{name} must be an integer") from exc
    if not minimum <= value <= maximum:
        raise RuntimeError(f"{name} must be between {minimum} and {maximum}")
    return value


def _env_bool(name: str, default: bool = False) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


def create_app() -> Flask:
    """Application factory used by Gunicorn and tests."""
    service = TzInfoService(
        db_path=os.getenv("TZINFO_GEOIP_DB", DEFAULT_DB_PATH),
        transition_years=_env_int(
            "TZINFO_TRANSITION_YEARS", DEFAULT_TRANSITION_YEARS, 1, MAX_TRANSITION_YEARS
        ),
        max_transitions=_env_int(
            "TZINFO_MAX_TRANSITIONS", DEFAULT_MAX_TRANSITIONS, 1, MAX_TRANSITIONS
        ),
        mmdb_check_interval=_env_int(
            "TZINFO_MMDB_CHECK_INTERVAL", DEFAULT_MMDB_CHECK_INTERVAL, 0, 86400
        ),
        leap_seconds_path=os.getenv("TZINFO_LEAP_SECONDS", DEFAULT_LEAP_SECONDS_PATH),
    )
    trust_proxy = _env_bool("TZINFO_TRUST_PROXY", False)

    app = Flask(__name__)
    # Response keys are sorted (Flask's default), as the examples in API.md.
    app.extensions["tzinfo_service"] = service

    @app.errorhandler(ServiceError)
    def handle_service_error(exc: ServiceError):
        return jsonify({"error": {"code": exc.code, "message": exc.message}}), exc.status

    @app.errorhandler(404)
    def handle_404(_exc):
        return jsonify({"error": {"code": "not_found", "message": "Endpoint not found"}}), 404

    @app.errorhandler(Exception)
    def handle_unexpected_error(exc: Exception):
        # Flask hands its own HTTP errors (405, 400, ...) to this handler too:
        # they are the client's, not ours.
        if isinstance(exc, HTTPException):
            code = (exc.name or "error").lower().replace(" ", "_")
            return jsonify({"error": {"code": code, "message": exc.description}}), exc.code
        logger.exception("Unhandled request error: %s", exc)
        return jsonify({"error": {"code": "internal_error", "message": "Internal server error"}}), 500

    @app.get(API_PATH)
    def tzinfo_endpoint():
        lat = parse_float_arg("lat")
        lon = parse_float_arg("lon")

        if (lat is None) != (lon is None):
            raise ServiceError(400, "incomplete_coordinates", "lat and lon must be supplied together")

        if lat is not None and lon is not None:
            validate_coordinates(lat, lon)
            source = "gnss"
        else:
            client_ip = get_client_ip(trust_proxy)
            lat, lon = service.coordinates_from_ip(client_ip)
            validate_coordinates(lat, lon)
            source = "geoip"

        zone_name = service.timezone_from_coordinates(lat, lon)
        now_epoch = int(datetime.now(timezone.utc).timestamp())
        response = service.build_response(zone_name=zone_name, source=source, now_epoch=now_epoch)

        logger.info("tzinfo source=%s zone=%s", source, zone_name)
        logger.debug("tzinfo coordinates lat=%.6f lon=%.6f", lat, lon)
        return jsonify(response)

    @app.get(SPECIAL_PATH)
    def special_endpoint():
        special_id = request.args.get("id")
        lat = parse_float_arg("lat")
        lon = parse_float_arg("lon")

        if (lat is None) != (lon is None):
            raise ServiceError(400, "incomplete_coordinates", "lat and lon must be supplied together")

        # No GeoIP here: city-level locations never fall inside geofences of
        # a few hundred metres, so without coordinates or an id nothing can
        # match and the answer is Special/None.
        if lat is not None and lon is not None:
            validate_coordinates(lat, lon)

        now_epoch = int(datetime.now(timezone.utc).timestamp())
        response = service.build_special_response(
            now_epoch=now_epoch,
            special_id=special_id,
            lat=lat,
            lon=lon,
        )
        logger.info(
            "special id=%s zone=%s",
            special_id or "auto",
            response["zone"],
        )
        if lat is not None and lon is not None:
            logger.debug("special coordinates lat=%.6f lon=%.6f", lat, lon)
        return jsonify(response)

    @app.get(HEALTH_PATH)
    def health_endpoint():
        return jsonify({"status": "ok"})

    @app.get(READY_PATH)
    def ready_endpoint():
        checks = {
            "zdump": shutil.which("zdump") is not None,
            "timezonefinder": service.timezone_finder is not None,
            "geoip": service.geoip_ready(),
            "leap_seconds": service.leap_seconds.ready(int(datetime.now(timezone.utc).timestamp())),
        }
        # Leap-second data serves only /special?id=tai: reported, but stale
        # tzdata does not make the clock-facing service unready.
        ready = all(value for name, value in checks.items() if name != "leap_seconds")
        return jsonify({"status": "ready" if ready else "not_ready", **checks}), 200 if ready else 503

    return app


if __name__ == "__main__":
    # Development only. Production uses Gunicorn behind Apache/nginx.
    application = create_app()
    host = os.getenv("TZINFO_BIND", "127.0.0.1")
    port = _env_int("TZINFO_PORT", 8088, 1, 65535)
    application.run(host=host, port=port, debug=False)
