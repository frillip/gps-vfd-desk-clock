# tzinfo API, protocol v1

The clock's ESP calls `/tzinfo`. The other endpoints are for special timebases and for monitoring. Every endpoint is a `GET` under `/smol_clock/api/v1/`, and every response is JSON with its keys in alphabetical order. The production service is at `https://rubidium.darksky.io/smol_clock/api/v1/`.

| Endpoint | Purpose |
| --- | --- |
| `/tzinfo` | The timezone at a position or the caller's GeoIP location, and its schedule of transitions |
| `/special` | A fixed special timebase, by id or by geofence |
| `/health` | Liveness |
| `/ready` | Readiness of everything the service depends on |

## /tzinfo

```
GET /smol_clock/api/v1/tzinfo?lat=51.5074&lon=-0.1278
GET /smol_clock/api/v1/tzinfo
```

With `lat` and `lon` (decimal degrees, both or neither) the timezone is found from the position, and `source` is `gnss`. Without them the caller's public IP address is located with MaxMind GeoLite2 City, and `source` is `geoip`. The ESP rounds the position before sending it (`tz precision`), and sends none at all with `tz precision none` or without a GNSS fix.

The base offset and the DST adjustment are kept apart:

```
effective offset = offset + (dst ? dst_offset : 0)
```

Example, London on 2026-10-30:

```json
{
  "current": [
    {"dst": false, "dst_offset": 3600, "offset": 0, "transition_utc": 1792890000}
  ],
  "generated": 1793361600,
  "next": [
    {"dst": true, "dst_offset": 3600, "offset": 0, "transition_utc": 1806195600},
    {"dst": false, "dst_offset": 3600, "offset": 0, "transition_utc": 1824944400}
  ],
  "source": "gnss",
  "valid_until": 1793966400,
  "version": 1,
  "zone": "Europe/London"
}
```

(`next` is shortened here. It holds up to eight entries.)

| Field | Meaning |
| --- | --- |
| `version` | Protocol version, 1 |
| `source` | `gnss` (coordinates given) or `geoip` |
| `zone` | The IANA timezone |
| `generated` | UTC epoch the response was evaluated at |
| `valid_until` | When to ask again at the latest: `generated` plus 7 days. The schedule in `next` covers the transitions before then |
| `current` | One entry: the state in force at `generated` |
| `current[0].transition_utc` | When that state began, the latest transition at or before `generated`. 0 if there has been none since 1970 (`Etc/UTC`, say) |
| `next` | The states to come, each in force from its `transition_utc`, in order. Up to `TZINFO_MAX_TRANSITIONS` (8) within `TZINFO_TRANSITION_YEARS` (5) years |
| `offset` | Base (standard) UTC offset, seconds |
| `dst_offset` | The DST adjustment, seconds. Kept while DST is off where tzdata gives one, so the clock can show it |
| `dst` | Whether `dst_offset` applies |

The states come from the host's tzdata, through Python `zoneinfo` (the state at an instant) and one `zdump` run per request (the transitions). The service follows whatever that tzdata build says, and does not reinterpret rearguard or vanguard data.

How the ESP turns this into a schedule for the PIC (folding past transitions, the horizon after the eighth) is in the [ESP README](../esp_src/README.md#timezone).

## /special

```
GET /smol_clock/api/v1/special?id=mecca_time
GET /smol_clock/api/v1/special?lat=21.4224779&lon=39.8251832
GET /smol_clock/api/v1/special
```

A fixed timebase in place of civil time. The clock does not use this yet. With `id`, that timebase, whatever the position. With `lat` and `lon` and no `id`, the first timebase whose geofence holds the position. With neither, or with a position no geofence holds, `Special/None`, a zero offset. GeoIP is never used here, as no city-level location falls inside a geofence a few hundred metres across.

| `id` | `zone` | Offset | Geofences |
| --- | --- | --- | --- |
| `gmt` | `Special/GMT` | 0 | Royal Observatory Greenwich, 51.477890, -0.001468, 300 m |
| `dublin_mean` | `Special/DMT` | -00:25:21 | Dunsink Observatory, 53.38708, -6.33756, 300 m |
| `paris_meridian` | `Special/Paris_Meridian` | +00:09:21 | Paris Observatory, 48.8364389, 2.3365056, 300 m. Louvre Pyramid, 48.861027, 2.335708, 500 m |
| `mecca_time` | `Special/Mecca` | +02:39:18.3 | Kaaba, 21.4224779, 39.8251832, 1000 m |
| `tai` | `Special/TAI` | TAI-UTC, +37 s at present | None: by `id` only |
| | `Special/None` | 0 | No `id` and no geofence |

The `zone` names are this protocol's own, not IANA identifiers.

```json
{
  "generated": 1793361600,
  "next": [],
  "special": [
    {"dst": false, "dst_offset": 0, "offset_ns": 300000000, "offset_s": 9558, "transition_utc": 0}
  ],
  "valid_until": 1793966400,
  "version": 1,
  "zone": "Special/Mecca"
}
```

The time shown is `UTC + offset_s + offset_ns`. `next` is always empty, as none of these changes. `valid_until` is `generated` plus 7 days.

TAI-UTC comes from the host tzdata's `leap-seconds.list` (`TZINFO_LEAP_SECONDS`, by default `/usr/share/zoneinfo/leap-seconds.list`). For `tai`, `valid_until` is also no later than the next change in that table or the file's own expiry. If the file is missing, unreadable or expired, `id=tai` fails (503, `leap_seconds_unavailable`, `leap_seconds_invalid` or `leap_seconds_expired`) rather than guess. The other timebases do not need it.

## /health and /ready

`/health` answers `{"status": "ok"}` while the process is up.

`/ready` checks each dependency: `zdump`, `timezonefinder`, `geoip` (the GeoLite2 database loaded) and `leap_seconds` (the table readable and current). It answers 200 with `"status": "ready"` when the first three are, and 503 with `"not_ready"` otherwise. `leap_seconds` is reported, but as only `/special?id=tai` needs it, stale tzdata does not make the service unready.

## Errors

Every error is JSON with an HTTP status:

```json
{"error": {"code": "incomplete_coordinates", "message": "lat and lon must be supplied together"}}
```

| Status | Codes |
| --- | --- |
| 400 | `invalid_lat`, `invalid_lon`, `incomplete_coordinates`, `invalid_special`, `invalid_client_ip`, and Flask's own (`bad_request`) |
| 404 | `not_found` (no such endpoint), `geoip_not_found` (no location for the address) |
| 405 | `method_not_allowed` (anything but `GET`) |
| 422 | `timezone_not_found` (open sea, say), `geoip_unresolvable` (a private or local address), `tai_undefined` |
| 500 | `timezone_database_error`, `zdump_failed`, `internal_error` |
| 503 | `geoip_unavailable`, `leap_seconds_unavailable`, `leap_seconds_invalid`, `leap_seconds_expired` |
| 504 | `zdump_timeout` |

The ESP retries a failed lookup after 5 minutes, doubling up to an hour, and keeps the schedule it has meanwhile.

## The caller's address

Behind a reverse proxy, set `TZINFO_TRUST_PROXY=1` and the address is taken from `X-Forwarded-For`. The service takes the last address in that header, the one the proxy itself added. Anything before it came from the client. Without `TZINFO_TRUST_PROXY` the connection's own address is used.

At INFO level the log records the source and zone of each request, never the coordinates or the address. Those are logged at DEBUG only.
