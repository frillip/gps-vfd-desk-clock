# Smol Clock tzinfo service

The server the clock asks how UTC maps to local time where it is. Given a position from the clock's GNSS receiver, or failing that the caller's IP address, it finds the IANA timezone and returns the state in force and the transitions to come, in a form small enough for the ESP to pass to the PIC. It is what answers `https://rubidium.darksky.io/smol_clock/api/v1/tzinfo`, and anyone can run their own: the clock's `tz server` and `tz path` point it elsewhere.

The service does not provide time. The clock keeps UTC from GNSS, NTP and its RTC, and asks this only about civil time. Timezone rules stay with the host's IANA tzdata, so keeping the host's `tzdata` package up to date keeps the clocks right.

- [API.md](API.md): the protocol, v1, every endpoint and field
- [INSTALL.md](INSTALL.md): deploying it, and updating a deployment

## How it works

```
supplied coordinates ──┐
                       ├─> TimezoneFinder ─> IANA timezone
client IP ─> MaxMind ──┘

IANA timezone ─┬─> zoneinfo: the state at an instant
               └─> zdump: the transitions, one run per request
                     └─> current state and the transitions to come
```

`tzinfo_service.py` holds the service and `create_app()`, the Flask application factory. Importing it starts nothing, so the tests can. `wsgi.py` is the production entry point (`wsgi:app`), run by Gunicorn as `www-data` on a Unix socket behind Apache or nginx, which terminate HTTPS. No process of it runs as root or listens on a TCP port.

The MaxMind GeoLite2 City database is kept up to date by `geoipupdate` on a timer, and the service picks up a new one without a restart, keeping the one it has if the new one will not open. Logs go to the journal (`journalctl -u smol-clock-tzinfo.service`). At INFO level they never hold a caller's coordinates or address.

## Settings

Environment variables, set in the systemd unit:

| Variable | Default | |
| --- | --- | --- |
| `TZINFO_GEOIP_DB` | `GeoLite2-City.mmdb` | The GeoLite2 City database |
| `TZINFO_TRANSITION_YEARS` | 5 | How far ahead to look for transitions, 1–20 |
| `TZINFO_MAX_TRANSITIONS` | 8 | How many to return, 1–40. The ESP expects 8 |
| `TZINFO_MMDB_CHECK_INTERVAL` | 60 | Seconds between checks for a new database |
| `TZINFO_TRUST_PROXY` | 0 | 1 behind a reverse proxy: the caller's address from `X-Forwarded-For` |
| `TZINFO_LEAP_SECONDS` | `/usr/share/zoneinfo/leap-seconds.list` | The leap-second table, for TAI |
| `TZINFO_LOG_LEVEL` | `INFO` | |
| `TZINFO_BIND`, `TZINFO_PORT` | `127.0.0.1`, 8088 | Running it directly (`python3 tzinfo_service.py`), for development only |

## Tests

From this folder, with the requirements installed:

```
python3 -m unittest discover -s tests -t .
```

- `tests/test_timezone_semantics.py`: representative and awkward zones (London, Berlin, New York, Lord Howe, Kolkata, Kathmandu, Chatham, Casablanca, Troll, UTC). The protocol's invariants, the current transition, the zero sentinel, field names and the single `zdump` run.
- `tests/test_special_time.py`: the TAI sign, the leap-second file, every geofence, explicit selection, `Special/None`.
- `tests/test_app.py`: over HTTP. Errors (a `POST` is 405, not 500), `/special` without coordinates or leap-second data, readiness, and which forwarded address is trusted.

The timezone tests need the host's `zdump` and tzdata, and the special-time tests make their own leap-second table. None needs the GeoIP database.

## Files

| Path | |
| --- | --- |
| `tzinfo_service.py` | The service and the Flask application factory |
| `wsgi.py` | The production entry point |
| `requirements.txt` | Python dependencies |
| `tests/` | The tests above |
| `deploy/systemd/` | `smol-clock-tzinfo.service` (Gunicorn), `smol-clock-geoipupdate.service` and `.timer` (the database) |
| `deploy/apache/`, `deploy/nginx/` | Reverse-proxy examples |
| `deploy/GeoIP.conf.example` | The MaxMind credentials file, without the credentials |
