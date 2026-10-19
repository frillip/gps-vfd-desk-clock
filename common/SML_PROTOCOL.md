# SML — Smol Clock serial link protocol

| | |
| --- | --- |
| Protocol version | 1.2 (`SML_PROTOCOL_MAJOR` 1, `SML_PROTOCOL_MINOR` 2) |
| Settings schema | 1 (`SML_SETTINGS_SCHEMA`) |
| Document revision | 2026-10-19 (1.2) |
| Definitions | `common/serial_proto.h` (the single shared copy) |
| Reference codec | `esp/sml.c` in the dsPIC V2+ tree (portable C99) |
| Status | TIME-AUTH is stable. All other payloads are provisional until both ends implement them. |

SML is the link between the dsPIC33EP256GP504 timekeeper (the **PIC**) and the ESP32 network co-processor (the **ESP**). It replaces the 0x83-magic structs used up to V2. The framing is modelled on u-blox UBX: a two-byte sync word, a class and an ID, a 16-bit length, the payload, and a CRC.

The PIC keeps UTC and disciplines the clock's epoch. The ESP supplies what it cannot get alone: network time, time zone schedules, leap second data and user commands. The ESP also receives the PIC's state for display, logging, statistics (Allan deviation) and the web interface, and tells the PIC about its network and services.

---

## 1. Physical link

| | PIC (V2) | ESP32 (V2) |
| --- | --- | --- |
| UART | UART1: RX RC6 (`ESP_UART1_RX`), TX RC7 (`ESP_UART1_TX`) | UART2: RX GPIO13, TX GPIO12 |
| Format | 115 200 baud, 8N1, no flow control | same |
| PIC epoch to ESP | RB10 epoch output (also the display latch) | GPIO27 (`PIC_PPS_PIN`) |
| ESP PPS to PIC | RB15 (`ESP_PPS`) | GPIO5 (`ESP_PPS_OUT_PIN`) |

At 115 200 baud a byte takes 86.8 µs, so a full 248-byte frame takes 21.5 ms.

The PIC's UART1 receive ring holds 256 bytes and is serviced every 10 ms. The transmit ring holds 1 KB.

## 2. Frame format

```
offset  0     1     2      3   4      5      6 ... 6+N-1   6+N    6+N+1
       +-----+-----+------+----+------+------+-----------+------+------+
       | B5  | 73  |CLASS | ID |LEN_H |LEN_L |  PAYLOAD  |CRC_H |CRC_L |
       +-----+-----+------+----+------+------+-----------+------+------+
```

| Field | Size | Value |
| --- | --- | --- |
| Sync | 2 | `0xB5 0x73` (`SML_MAGIC_1`, `SML_MAGIC_2`) |
| Class | 1 | Message class (§5) |
| ID | 1 | Message ID within the class |
| Length | 2 | Payload length N, big-endian, 0..240 (`SML_MAX_PAYLOAD_BYTES`) |
| Payload | N | Message body (§6–§9) |
| CRC | 2 | CRC-16/IBM-3740, big-endian, over **sync through the last payload byte** |

- Header: 6 bytes (`SML_HEADER_BYTES`). Checksum: 2 bytes (`SML_CRC_BYTES`). Maximum frame: 248 bytes (`SML_MAX_FRAME_BYTES`).
- Unlike UBX, the sync bytes are covered by the CRC.

### 2.1 CRC

CRC-16/IBM-3740, also known as CRC-16/CCITT-FALSE:

| Parameter | Value |
| --- | --- |
| Polynomial | 0x1021 |
| Initial value | 0xFFFF |
| Reflect in / out | no / no |
| Final XOR | 0x0000 |
| Check (`"123456789"`) | **0x29B1** |

The reference implementation (`sml_crc`) uses a 16-entry nibble table, 32 bytes of constant data. The host tests check it against a bitwise implementation.

### 2.2 Encoding rules

1. **Byte order.** Every multi-byte field is big-endian. Signed integers are two's complement.
2. **No native types on the wire.** Floats, `time_t`, C enums, bitfields, pointers and unions are never sent. The packed structs in `serial_proto.h` define field order and size only. They are converted field by field, never cast or `memcpy`'d whole (§10.1).
3. **Reserved fields.** Reserved fields and flag bits are sent as zero. A receiver ignores reserved bits and does not reject a frame because they are set.
4. **Time.** All times are `uint32_t` Unix seconds, valid up to 2106-02-07T06:28:15Z. An inserted leap second (23:59:60) has no Unix value. It is sent as 23:59:59 a second time, and TIME-LEAP and the GNSS leap fields say when that will happen.
5. **Ages.** All `*_age_ms` fields are `uint16_t` and saturate at 65 535 ms; they never wrap. A saturated age does not by itself mean the data is invalid: the validity flags and the sender's own timeouts decide that.
6. **Flags gate fields.** Each validity flag covers its group of fields. Fields whose flag is clear carry no information, and are normally zero.
7. **Lengths.** Every message except STAT-DEBUG has a fixed length (§5). State messages are at least 8 bytes (`SML_MIN_STATE_BYTES`). Commands, responses and debug text may be shorter.

### 2.3 Receiving

A receiver must behave as the reference parser (`sml_parser_push`) does:

- **Searching for sync.** Bytes outside a frame are noise. They are counted and discarded. During migration that noise is the old firmware's messages or console text.
- **Rejected headers.** A header whose length exceeds 240 is not a frame.
- **Rescanning.** When a CRC fails, or a header is rejected, the receiver moves on by **one byte** and rescans what it has buffered. A real frame that began inside a false header is therefore still found. Every frame survived a fuzz test of 20 000 frames in noise dense with `B5` and `73` bytes.
- **Timeout.** A frame that is still incomplete when the line has been idle for 100 ms on the PIC (`ESP_LINK_FRAME_TIMEOUT_MS`) is dropped.
- **Corrupt frames are never answered.** Nothing in them can be trusted, including the token.
- **Unknown class or ID.** The frame is counted and ignored, so either end can add messages without breaking the other.
- **Wrong direction.** A known message arriving in the wrong direction is counted and ignored.
- **Wrong length.** A known fixed-length message with the wrong length is counted and dropped. A command (class 0x80) with a good CRC and at least 6 bytes is answered NACK `SML_ERROR_LENGTH`, with the token taken from payload offset 4.

## 3. Link modes and migration

The PIC must work with the old bridging ESP firmware as well as with an SML one, so its link has two modes (`esp/esp_link.h`):

| Mode | PIC sends | PIC receives |
| --- | --- | --- |
| **TEXT** (start-up default) | Console text, raw | SML frames are parsed and acted on. Anything else is counted as noise. |
| **SML** | Console text inside STAT-DEBUG frames, plus the per-epoch state messages (§11) | SML frames |

- **Switching to SML.** The PIC switches to SML mode, permanently until reset, on the **first valid SML frame from the ESP**. An SML-speaking ESP announces itself by sending any frame soon after boot. CMD-USER `INFO` is the recommended one, because it also returns the PIC's full diagnostics.
- **Building in SML mode.** `-DESP_LINK_START_SML=1` makes the PIC start in SML mode.
- **Old 0x83 traffic.** The old firmware's frames are harmless noise to an SML receiver. The chance of one passing for an SML frame is about 2⁻¹⁶ per false sync, and it would also need the right class, ID and length to be acted on.
- **Bootloader.** Class 0xF0 is reserved for the bootloader. Its IDs are reserved but its payloads are not defined. Until they are, the existing bootloader path (`pic_bootloader.cpp`, BOOTLOADER_CMD) stays outside SML, and an ESP must not send SML traffic while it is driving the bootloader.

## 4. Conventions in this document

- **Directions.** `PIC→ESP` and `ESP→PIC`.
- **Offsets** are payload offsets, from the first payload byte.
- **Types:** `u8 u16 u32` are unsigned and `i8 i16 i32` are signed, of the given width.
- **"V2+ PIC" column.** The ✓ and — marks in this column say what the dsPIC V2+ firmware does as of this revision. They are not part of the protocol.

## 5. Message catalogue

| Class | ID | Name | Direction | Length | V2+ PIC | Rate |
| --- | --- | --- | --- | --- | --- | --- |
| 0x10 TIME | 0x01 | TIME-AUTH | PIC→ESP | 16 | ✓ sent | every epoch, first |
| | 0x10 | TIME-PIC | PIC→ESP | 38 | ✓ sent | every epoch |
| | 0x11 | TIME-CTRL | PIC→ESP | 56 | ✓ sent | every epoch |
| | 0x12 | TIME-VCTCXO | PIC→ESP | 20 | — (V3 hardware) | every epoch (V3) |
| | 0x13 | TIME-MEAS | PIC→ESP | 40 | ✓ sent | every epoch |
| | 0x20 | TIME-GNSS | PIC→ESP | 54 | ✓ sent | every epoch |
| | 0x30 | TIME-NTP | ESP→PIC | 42 | ✓ received and held (not yet a sync source) | every ESP PPS while NTP time is valid |
| | 0x40 | TIME-RTC | PIC→ESP | 34 | — (RTC not yet ported) | every epoch |
| | 0x50 | TIME-LEAP | ESP→PIC | 16 | ✓ applied | on change, and periodically |
| 0x20 TZ | 0x01 | TZ-STATE | ESP→PIC | 124 | ✓ applied | on change, after a PIC reset, and every ~10 min |
| | 0x10 | *(retired: TZ-FUTURE)* | — | — | — | never reuse |
| 0x30 STAT | 0x01 | STAT-UI | PIC→ESP | 32 | ✓ sent | every epoch |
| | 0x10 | STAT-ENV | PIC→ESP | 14 | — (I²C not yet ported) | every few seconds |
| | 0x20 | STAT-SUPPLY | PIC→ESP | 14 | — (V3 hardware) | every few seconds |
| | 0x30 | STAT-SETTINGS | PIC→ESP | 36 | — (settings store not yet ported) | on change, after reset, on request |
| | 0x40 | STAT-NET | ESP→PIC | 24 | ✓ received and held | on change, and every 10 s |
| | 0xF0 | STAT-DEBUG | PIC→ESP | 1..240 | ✓ sent in SML mode | as text is produced |
| 0x80 CMD | 0x10 | CMD-USER | both ways (§9.1) | 10 | ✓ executes PIC codes; sends ESP codes | on demand |
| | 0x20 | CMD-RESPONSE | both ways | 12 | ✓ | one per command |
| | 0x30 | CMD-SETTINGS | ESP→PIC | 38 | NACK UNSUPPORTED for now | on demand |
| 0xF0 BOOT | 0x10, 0x20, 0x50, 0x60, 0x90, 0xA0 | BOOT-COMMAND, -RESPONSE, -DATA, -DATARESP, -VERIFY, -VERIFYRESP | — | deferred | — | — |

---

## 6. TIME class (0x10)

### 6.1 TIME-AUTH (0x01): the authoritative time, PIC→ESP, 16 bytes

This is the PIC's statement of the UTC second that its latest epoch edge marks. It is the only message whose meaning is frozen.

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `UTC_VALID`, bit 1 `UNCERTAINTY_VALID`, bit 2 `SOURCE_VALID` |
| 2 | source | u8 | `SML_SOURCE_*` (§12.1) |
| 3 | lock_state | u8 | `SML_LOCK_*` (§12.2) |
| 4 | utc_seconds | u32 | Unix seconds labelling the epoch edge |
| 8 | utc_phase_ns | i32 | ns; UTC at the edge = utc_seconds + utc_phase_ns × 10⁻⁹ |
| 12 | uncertainty_ns | u32 | ns, a ± bound |

- **`utc_phase_ns`** is the epoch edge minus the reference edge (OC minus IC). A positive value means the clock's edge is late. It is 0 in holdover, where there is no reference edge to measure.
- **`uncertainty_ns`** is a ± **bound** on the UTC error of the edge, not one standard deviation. It is the sum of:
  - the selected source's own accuracy (GNSS tAcc);
  - 25 ns of capture resolution;
  - |measured phase| while tracking, or 100 ns per second spent in holdover (`SYNC_HOLDOVER_DRIFT_NS_PER_S`).
- **When the flags are clear.**
  - Without `UTC_VALID`, the phase and uncertainty are invalid too.
  - In free run with the time of day still known (counted on from an earlier label), `UTC_VALID` is set and `UNCERTAINTY_VALID` is clear.
  - `SOURCE_VALID` means a source is selected (`source` ≠ NONE).
- **Timing.** It is sent first in each epoch, a few milliseconds after the edge it labels and long before the next one. The ESP pairs it with the most recent PIC edge it captured on GPIO27.

### 6.2 TIME-PIC (0x10): epoch timer and synchroniser, PIC→ESP, 38 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `PHASE_VALID`, bit 1 `PERIOD_VALID`, bit 2 `FREQUENCY_VALID`, bit 3 `PPS_VALID` |
| 2 | source | u8 | `SML_SOURCE_*` |
| 3 | lock_state | u8 | `SML_LOCK_*` |
| 4 | sync_state | u8 | `SML_SYNC_*` (§12.3) |
| 5 | previous_sync_state | u8 | `SML_SYNC_*`, before the latest change |
| 6 | last_sync_cause | u8 | `SML_CAUSE_*` (§12.4) |
| 7 | reserved | u8 | 0 |
| 8 | epoch_sequence | u32 | epochs since PIC start |
| 12 | fosc_hz | u32 | timer clock (FCY) measured against the reference, Hz (`FREQUENCY_VALID`) |
| 16 | pps_period_cycles | u32 | latest reference PPS period, timer cycles (`PERIOD_VALID`) |
| 20 | phase_cycles | i32 | epoch − reference edge, cycles (`PHASE_VALID`) |
| 24 | applied_adjustment_cycles | i32 | period requested for the next epoch − nominal, cycles |
| 28 | missing_pps_events | u32 | epochs in which the GNSS source had no edge |
| 32 | sync_events | u32 | synchroniser state changes since start |
| 36 | reference_age_ms | u16 | time since the last usable reference edge, ms (saturates) |

- **`fosc_hz`** keeps its V2 meaning: the timer clock, FCY, nominally 40 000 000. It is not the 80 MHz Fosc. One cycle is 25 ns.
- **`PPS_VALID`** means the reference edge was captured in this epoch.

### 6.3 TIME-CTRL (0x11): epoch control loop, PIC→ESP, 56 bytes

The estimator and servo state, **lossless**: the PIC's own fixed-point values, so the ESP sees the full-precision frequency estimate rather than a rounded one.

**Units.**
- A **tick** is one timer cycle: 1/`nominal_period_ticks` of a second, 25 ns on V2.
- **Q16.16** values are ticks × 65 536.
- **Q2.30** values are × 2³⁰.

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `ESTIMATE_VALID`, 1 `PI_ENABLED`, 2 `SATURATED`, 3 `MEASUREMENT_USED`, 4 `GATE_REJECTED` |
| 2 | controller_state | u8 | `SML_CONTROL_*` (§12.5) |
| 3 | reserved | u8 | 0 |
| 4 | epoch_sequence | u32 | as TIME-PIC |
| 8 | nominal_period_ticks | u32 | ticks in a nominal second (40 000 000 on V2) |
| 12 | frequency_offset_q16 | i32 | Kalman state x: reference period − nominal, Q16.16 ticks |
| 16 | frequency_variance_q30 | u32 | Kalman P / R, Q2.30 |
| 20 | measurement_variance_q16 | u32 | Kalman R, tick² Q16.16 |
| 24 | kalman_gain_q30 | u32 | last Kalman gain, Q2.30 |
| 28 | predicted_phase_q16 | i32 | phase predicted for the next epoch, Q16.16 ticks |
| 32 | phase_innovation_q16 | i32 | this epoch: measured − predicted phase, Q16.16 ticks |
| 36 | pi_proportional_q16 | i32 | PI proportional term, Q16.16 ticks of period |
| 40 | pi_integral_q16 | i32 | PI integral, Q16.16 ticks of period |
| 44 | output_offset_q16 | i32 | steering: period requested for the next epoch − nominal, Q16.16 ticks |
| 48 | gate_rejections | u32 | phase gate rejections since start |
| 52 | kalman_rejections | u32 | Kalman outlier rejections since start |

**Flags.**
- `ESTIMATE_VALID`: the Kalman filter is initialised. It gates offsets 12–28.
- `SATURATED`: the PI correction is at its limit.
- `MEASUREMENT_USED`: this epoch's phase fed the loop.
- `GATE_REJECTED`: the phase gate rejected this epoch's phase.

**Converting on the ESP** (in `float`, relative to the nominal):
- Fractional frequency of the oscillator (positive = fast) = `frequency_offset_q16` / 65 536 / `nominal_period_ticks`. One Q16 step is 0.38 × 10⁻¹² at 40 MHz.
- Measured frequency = `nominal_period_ticks` + `frequency_offset_q16` / 65 536 ticks per reference second.
- Frequency estimate standard deviation = √(`frequency_variance_q30` / 2³⁰ × `measurement_variance_q16` / 65 536) ticks.
- Phase in ns = Q16 value / 65 536 × 10⁹ / `nominal_period_ticks`.

**What is left out.** The Kalman period innovation is not sent. The ESP can form it as `reference_period_ticks` (TIME-MEAS) − nominal − x of the previous epoch.

### 6.4 TIME-VCTCXO (0x12): VCTCXO steering, V3 only, PIC→ESP, 20 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `PRESENT`, bit 1 `ENABLED`, bit 2 `SATURATED`, bit 3 `TEMP_VALID` |
| 2 | controller_state | u8 | `SML_CONTROL_*` |
| 3 | reserved | u8 | 0 |
| 4 | epoch_sequence | u32 | |
| 8 | dac_code | u16 | DAC code (range depends on the hardware) |
| 10 | control_uv | u32 | control voltage, µV |
| 14 | correction_ppt | i32 | 10⁻¹² |
| 18 | vctcxo_adc_temperature_centic | i16 | 0.01 °C, from the ADC |

### 6.5 TIME-GNSS (0x20): receiver state, PIC→ESP, 54 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `DETECTED`, 1 `FIX_OK`, 2 `UTC_VALID`, 3 `PPS_VALID`, 4 `POSITION_VALID`, 5 `LEAP_VALID`, 6 `LEAP_EVENT_VALID` |
| 2 | fix_type | u8 | UBX NAV-STATUS gpsFix: 0 none, 1 DR, 2 2D, 3 3D, 4 GNSS+DR, 5 time only |
| 3 | utc_standard | u8 | UBX NAV-TIMEUTC utcStandard (upper nibble of valid) |
| 4 | utc_seconds | u32 | Unix seconds of the latest navigation epoch (`UTC_VALID`) |
| 8 | utc_accuracy_ns | u32 | NAV-TIMEUTC tAcc, ns |
| 12 | pps_sequence | u32 | PIC capture sequence number of the PPS edge it labels (`PPS_VALID`) |
| 16 | utc_age_ms | u16 | age of the navigation epoch |
| 18 | pps_age_ms | u16 | age of the labelled PPS association |
| 20 | position_age_ms | u16 | age of the position |
| 22 | latitude_e7 | i32 | 10⁻⁷ degrees (`POSITION_VALID`) |
| 26 | longitude_e7 | i32 | 10⁻⁷ degrees |
| 30 | height_mm | i32 | height above the ellipsoid, mm |
| 34 | height_msl_mm | i32 | height above mean sea level, mm |
| 38 | horizontal_accuracy_mm | u32 | mm: the ESP's GNSS margin for its time zone anchor check |
| 42 | vertical_accuracy_mm | u32 | mm |
| 46 | gps_utc_s | i16 | GPS − UTC, s (`LEAP_VALID`; TAI − UTC = gps_utc_s + 19) |
| 48 | leap_change | i8 | −1, 0, +1 (`LEAP_EVENT_VALID`) |
| 49 | reserved_leap | u8 | 0 |
| 50 | leap_event_utc | u32 | first UTC second at which the new count applies (a midnight) |

- **Validity.**
  - `UTC_VALID` needs a usable fix, fully valid UTC, and a navigation epoch aligned to the second.
  - `POSITION_VALID` needs NAV-POSLLH in the same epoch with `FIX_OK`.
  - The leap fields come from UBX-NAV-TIMELS, at most 30 s old. `LEAP_EVENT_VALID` is set only when the announced event lands on a UTC midnight, to within 2 s.
- **Diagnostics.** NAV-CLOCK, TIM-TP and TIM-TM2 detail is available only through debug text.

### 6.6 TIME-NTP (0x30): ESP network time, ESP→PIC, 42 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 reserved (was `WIFI_CONNECTED`, now in STAT-NET), 1 `TIME_VALID`, 2 `UPDATE_VALID`, 3 `PPS_SYNC`, 4 `SCHEDULER_SYNC`, 5 `UNCERTAINTY_VALID` |
| 2 | source | u8 | `SML_SOURCE_NTP`, or `SML_SOURCE_ESP` when coasting on its own clock |
| 3 | lock_state | u8 | `SML_LOCK_*`, the ESP's view of its own time |
| 4 | utc_seconds | u32 | UTC of the **ESP PPS edge** this message labels, not of the UART arrival |
| 8 | utc_phase_ns | i32 | ESP PPS edge − UTC, ns |
| 12 | uncertainty_ns | u32 | ± bound, ns (`UNCERTAINTY_VALID`) |
| 16 | pps_sequence | u32 | ESP PPS edge count |
| 20 | last_update_utc | u32 | UTC of the last NTP exchange (`UPDATE_VALID`) |
| 24 | update_age_ms | u16 | age of that exchange; saturates normally with long poll intervals |
| 26 | update_interval_s | u32 | NTP poll interval, s |
| 30 | round_trip_us | u32 | last round trip, µs |
| 34 | last_offset_us | i32 | last NTP offset (estimated UTC − ESP time), µs |
| 38 | frequency_estimate_ppt | i32 | ESP clock frequency error, 10⁻¹² |

- **Sending.** The ESP sends this after each of its PPS edges. That gives the PIC an edge (RB15) plus a label, which is what an NTP sync source needs.
- **On the V2+ PIC.** The latest message is stored (`esp_link_ntp()`) for the NTP source adapter, which is planned work (§13).

### 6.7 TIME-RTC (0x40): battery-backed RTC, PIC→ESP, 34 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `PRESENT`, 1 `UTC_VALID`, 2 `SYNCED`, 3 `LAST_READ_VALID`, 4 `LAST_WRITE_VALID`, 5 `PPS_PRESENT`, 6 `PPS_VALID`, 7 `TEMP_VALID`, 8 `UNCERTAINTY_VALID` |
| 2 | rtc_type | u8 | `SML_RTC_*` (§12.8) |
| 3 | reserved | u8 | 0 |
| 4 | rtc_utc_seconds | u32 | RTC reckoning now, advanced from the last good read |
| 8 | last_read_utc | u32 | authoritative UTC when the last read completed |
| 12 | last_write_utc | u32 | authoritative UTC when the last write completed |
| 16 | rtc_temperature_centic | i16 | 0.01 °C, 0 if unsupported |
| 18 | pps_sequence | u32 | RTC PPS edge count (V3) |
| 22 | last_pps_utc | u32 | UTC of the last RTC PPS edge (V3) |
| 26 | pps_phase_ns | i32 | PIC epoch − paired RTC edge, ns (V3) |
| 30 | uncertainty_ns | u32 | ± bound on the RTC reckoning, ns |

- **Register contents.** RTC register contents are never sent as timestamps.
- **V2.** The PPS fields are V3-only. On V2 they are zero, with their presence and validity flags clear.

### 6.8 TIME-LEAP (0x50): the leap second in force, ESP→PIC, 16 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `VALID`, bit 1 `EVENT_PENDING` |
| 2 | source | u8 | `SML_LEAP_SOURCE_*` (§12.7) |
| 3 | reserved | u8 | 0 |
| 4 | tai_utc_s | i16 | TAI − UTC in force now (37 in 2026) |
| 6 | change | i8 | −1, 0, +1 |
| 7 | reserved2 | u8 | 0 |
| 8 | event_utc | u32 | first UTC second at which TAI − UTC becomes tai_utc_s + change (a midnight) |
| 12 | expires_utc | u32 | expiry of the leap data, 0 if unknown |

- **Choosing the source.** The ESP picks the source: GNSS (the PIC's TIME-GNSS leap fields) when valid, otherwise its server leap data. GNSS wins any disagreement, and the disagreement is logged.
- **Sending.** It is sent on change and periodically.
- **On the PIC.**
  - A message with `VALID` and `EVENT_PENDING` set, `change` ±1 and `event_utc` exactly on a UTC midnight is passed to the synchroniser (`sync_set_leap`). Anything else withdraws an earlier announcement.
  - The synchroniser uses this announcement whenever its selected source announces none, in holdover and free run too. Its own GNSS NAV-TIMELS takes precedence.
  - With +1 pending, the display shows 23:59:60 UTC (or its local equivalent) for the second before `event_utc`.

### 6.9 TIME-MEAS (0x13): raw measurements of one epoch, PIC→ESP, 40 bytes

This is the logging record for statistics and Allan deviation on the ESP.
- **Raw.** Nothing in it is filtered or rounded.
- **Self-contained.** Each record is complete on its own and keyed by `epoch_sequence`.
- **Timing.** It is sent every epoch, after TIME-CTRL.

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `LABEL_VALID`, 1 `PHASE_VALID`, 2 `PERIOD_VALID`, 3 `EPOCH_PERIOD_VALID`, 4 `MARK_VALID`, 5 `QERR_VALID`, 6 `MEASUREMENT_USED` |
| 2 | source | u8 | `SML_SOURCE_*` of the reference |
| 3 | controller_state | u8 | `SML_CONTROL_*` |
| 4 | epoch_sequence | u32 | |
| 8 | utc_seconds | u32 | UTC label of this epoch (`LABEL_VALID`) |
| 12 | reference_sequence | u32 | reference edge count; a gap means missed edges |
| 16 | reference_period_ticks | u32 | reference PPS-to-PPS interval, ticks (`PERIOD_VALID`) |
| 20 | phase_ticks | i32 | epoch capture − reference capture, whole ticks (`PHASE_VALID`) |
| 24 | epoch_period_ticks | u32 | length of the second that ended at this epoch, as loaded into the epoch timer (`EPOCH_PERIOD_VALID`) |
| 28 | mark_offset_ns | i32 | TIM-TM2: the receiver's measurement of the **previous** epoch's edge (epoch_sequence − 1) against its second; positive = late (`MARK_VALID`) |
| 32 | mark_accuracy_ns | u32 | TIM-TM2 accuracy estimate |
| 36 | qerr_ps | i32 | TIM-TP quantisation error of this epoch's reference edge (`QERR_VALID`) |

What each series measures is covered in §10.5.
- **`phase_ticks`.** It is taken before the half-tick calibration (§10.5).
- **`reference_period_ticks`.** It is measured on the free-running capture timebase, so it shows the oscillator against the reference, unsteered.
- **Marks.** Each TIM-TM2 mark is reported once only, in the record of the epoch after the edge it measured, so a mark is never paired with the wrong second.

---

## 7. TZ class (0x20)

### 7.1 TZ-STATE (0x01): local time schedule, ESP→PIC, 124 bytes

The whole schedule is replaced atomically. It holds the entry in force now, followed by up to eight future entries in ascending UTC order.

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `VALID` (the single gating flag), bit 1 `AMBIGUOUS` (informational) |
| 2 | source | u8 | `SML_TZ_SOURCE_*` (§12.6) |
| 3 | count | u8 | future entries used, 0..8 |
| 4 | issued_utc | u32 | ESP time when assembled; orders updates |
| 8 | horizon_end_utc | u32 | exclusive end of coverage: the first transition **not** included; `0xFFFFFFFF` = no further transitions known |
| 12 | subsecond_offset_ns | i32 | applies to every entry; 0 except for special timebases |
| 16 | current | entry | the entry in force at issue |
| 28 | next[0..7] | entry × 8 | ascending UTC; unused entries are zero |

Each entry is 12 bytes. Entry *k* of `next` starts at offset 28 + 12k.

| Entry offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | utc_seconds | u32 | in force from this second; 0 = since before the data |
| 4 | base_offset_s | i32 | standard offset from UTC, s |
| 8 | dst_offset_s | i16 | the zone's DST amount, s; sent even when inactive; may be negative |
| 10 | flags | u8 | bit 0 `DST_ACTIVE` |
| 11 | reserved | u8 | 0 |

**Local time = UTC + base_offset_s + (DST_ACTIVE ? dst_offset_s : 0) + subsecond_offset_ns.**

**What the PIC takes from it.** Two independent settings choose:

| Setting | Set | Clear |
| --- | --- | --- |
| `SML_SETTING_TZ_AUTO` | base offset (and sub-second offset) from the schedule | the manual `base_offset_s` |
| `SML_SETTING_DST_AUTO` | DST amount and DST state from the schedule | the manual `dst_offset_s` and `SML_SETTING_DST_ACTIVE` |

The four combinations are:

| TZ_AUTO | DST_AUTO | Typical use |
| --- | --- | --- |
| on | on | Normal: everything from the ESP |
| off | on | A fixed base offset the user chose, but DST changes still follow the ESP's zone |
| on | off | The zone's base, with DST overridden by hand |
| off | off | Fully manual: the ESP is ignored |

- **No schedule held.** While none is held (`SML_UI_TZ_HELD` clear), both settings fall back to the manual values.
- **Flags.** `STALE` and `AMBIGUOUS` are reported whenever any part comes from the schedule.
- **Where the schedule comes from.** That is the ESP's business, not the PIC's. It may be built from tzinfo lookups (GNSS, GeoIP, pinned zone), or with tzinfo disabled from a zone or rule configured on the ESP itself (`SML_TZ_SOURCE_MANUAL` or `PINNED`). The ESP therefore always sends its best schedule and never needs to know the PIC's settings.

The PIC applies the schedule as follows (`ui/timezone.c`):

| Rule | Behaviour |
| --- | --- |
| `VALID` clear | Ignored. The schedule held is kept. |
| Always held | A valid, well-formed, newer schedule is stored **whatever the PIC's settings**. The settings only choose which parts are used (below). |
| Well-formed | Required: count ≤ 8, future entries strictly ascending after `current`, every base offset within ±86 399 s, \|subsecond_offset_ns\| < 10⁹, and horizon later than the last entry. Otherwise the message is rejected and counted. |
| Ordering | Applied if `issued_utc` ≥ the applied one's, **or** the source differs, or nothing is held yet. |
| Atomicity | All entries are replaced together. The schedule is only ever read in the foreground, so a half-applied schedule cannot be seen. |
| Each second | The entry in force is the last one whose `utc_seconds` ≤ UTC. Transitions take effect exactly at their UTC second, and the frame for that second is already rendered in the new offset. |
| Horizon | From `horizon_end_utc`, the last entry is held and `SML_UI_TZ_STALE` is set. The PIC never falls back to UTC on its own. |
| Sub-second | Rounded to the nearest second, because the display has no sub-second phase shift yet. Example: Mecca, +02:39:18.3 is sent as base 9558 s and 300 000 000 ns, and displayed as +02:39:18. |
| Persistence | Keyed on a CRC of the schedule **without** `issued_utc`, so periodic resends never count as changes (`timezone_take_changed()`). It will be written by the settings store when that is ported, and never by settings revisions. |
| No acknowledgement | STAT-UI reports the offsets actually in use. |

**Sending (ESP).** The ESP sends TZ-STATE:
- on change;
- whenever STAT-UI shows `TZ_HELD` clear, for example after a PIC reset;
- every ~10 minutes.

**Example: London as of 2026-10.** `count` = 8, `horizon_end_utc` = 1919293200, `subsecond_offset_ns` = 0.

| | utc_seconds | base | dst | DST_ACTIVE |
| --- | --- | --- | --- | --- |
| current | 1774746000 | 0 | 3600 | 1 |
| next[0] | 1792890000 | 0 | 3600 | 0 |
| next[1] | 1806195600 | 0 | 3600 | 1 |
| … | … | … | … | … |
| next[7] | 1901149200 | 0 | 3600 | 1 |

---

## 8. STAT class (0x30)

### 8.1 STAT-UI (0x01): what the clock shows, PIC→ESP, 32 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | see below |
| 2 | current_display | u8 | `SML_DISPLAY_*` shown now (§12.9) |
| 3 | selected_display | u8 | `SML_DISPLAY_*` selected by the user |
| 4 | menu_state | u8 | `SML_MENU_NONE` outside the menu |
| 5 | button_state | u8 | `SML_BUTTON_*` |
| 6 | brightness | u16 | tube PWM level now (V2: 200..3800 of 4000) |
| 8 | brightness_target | u16 | level being faded to |
| 10 | reserved | u16 | 0 |
| 12 | local_seconds | u32 | UTC + applied offsets: a civil-time scalar, **not** a UTC timestamp (`LOCAL_VALID`) |
| 16 | base_offset_s | i32 | base offset in use, s |
| 20 | dst_offset_s | i32 | DST amount of the entry in use, s, even when inactive (as in TZ-STATE) |
| 24 | alarm_offset_s | u32 | alarm, seconds after local midnight, 0..86 399 |
| 28 | delta_epoch_utc | u32 | the configured delta-mode epoch, UTC |

Flags: bit 0 `LOCAL_VALID`, 1 `ALARM_ENABLED`, 2 `ALARM_ACTIVE`, 3 `BRIGHTNESS_MANUAL`, 4 `POWER_ON`, 5 `SWITCH`, 6 `BUTTON`, 7 `UPDATE_PENDING`, 8 `FORMAT_12H`, 9 `DST_ACTIVE`, 10 `TZ_STALE`, 11 `TZ_AMBIGUOUS`, 12 `TZ_HELD` (a TZ-STATE schedule is held).

On the V2+ PIC the alarm, delta, menu, 12-hour format and auto brightness fields stay zero until the UI is ported.

### 8.2 STAT-ENV (0x10): sensors, PIC→ESP, 14 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `LUX_PRESENT`, 1 `LUX_VALID`, 2 `BME_PRESENT`, 3 `TEMP_VALID`, 4 `PRESSURE_VALID`, 5 `HUMIDITY_VALID`, 6 `BOARD_TEMP_VALID` |
| 2 | reserved | u16 | 0 |
| 4 | lux | u16 | whole lux, 0..65 535 |
| 6 | ambient_temperature_centic | i16 | 0.01 °C |
| 8 | board_temperature_centic | i16 | 0.01 °C |
| 10 | pressure_deca_pa | u16 | 10 Pa per count, 0..655 350 Pa |
| 12 | humidity_centipercent | u16 | 0.01 % RH, 0..10 000 |

- **Ranges.** A value out of range clears its validity flag rather than wrapping.
- **No sentinels.** There are no sentinel values and no sample ages.

### 8.3 STAT-SUPPLY (0x20): power rails, V3, PIC→ESP, 14 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `5V0_VALID`, 1 `3V3_VALID`, 2 `VFIL_VALID`, 3 `VPP_VALID`, 4 `VFIL_ENABLED`, 5 `VPP_ENABLED`, 6 `DRIVER_ENABLED`, 7 `DRIVER_FREQ_VALID` |
| 2 | reserved | u16 | 0 |
| 4 | rail_5v0_mv | u16 | mV |
| 6 | rail_3v3_mv | u16 | mV |
| 8 | filament_mv | u16 | mV |
| 10 | grid_vpp_mv | u16 | mV |
| 12 | vfil_driver_frequency_hz | u16 | actual switching frequency, Hz, < 50 000 |

### 8.4 STAT-SETTINGS (0x30): stored settings, PIC→ESP, 36 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `VALID`, 1 `DIRTY` (not yet persisted), 2 `DEFAULTS` (factory values in use) |
| 2 | reserved | u16 | 0 |
| 4 | revision | u32 | incremented by every applied change, from either end |
| 8 | settings | body | 28 bytes, below |

The settings body is shared with CMD-SETTINGS. Offsets are relative to the body.

| Body offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | schema_version | u16 | `SML_SETTINGS_SCHEMA` (1) |
| 2 | flags | u16 | bit 0 `TZ_AUTO` (base from TZ-STATE), 1 `DST_AUTO` (DST from TZ-STATE), 2 `DST_ACTIVE` (manual DST state), 3 `ALARM_ENABLED`, 4 `BEEP_ENABLED`, 5 `FORMAT_12H`, 6 `BRI_MANUAL` |
| 4 | base_offset_s | i32 | **manual** base offset, s |
| 8 | dst_offset_s | i32 | **manual** DST amount, s |
| 12 | calibration_fosc_hz | u32 | stored oscillator baseline (FCY), Hz; not live telemetry |
| 16 | alarm_offset_s | u32 | 0..86 399 |
| 20 | delta_epoch_utc | u32 | |
| 24 | brightness | u16 | manual level; range checked by the PIC |
| 26 | selected_display | u8 | `SML_DISPLAY_*`, only values below `SML_DISPLAY_DASHES` |
| 27 | reserved | u8 | 0 |

The automatic TZ schedule is not part of the settings. It is persisted separately, so monthly ESP updates never change the revision or collide with edits made from the menu.

### 8.5 STAT-DEBUG (0xF0): console text, PIC→ESP, 1..240 bytes

- **Format.** Raw text bytes, with no NUL terminator, no flags and no struct.
- **Framing.** Longer output is split across frames. The V2+ PIC sends a frame at each line end, whenever 240 bytes are pending, and otherwise within 10 ms.
- **On the ESP.** The ESP concatenates the payloads and forwards them to its serial console and telnet. This is the SML replacement for the raw "bridge" text of the old firmware.

### 8.6 STAT-NET (0x40): the ESP's network and services, ESP→PIC, 24 bytes

STAT-NET carries the network and service status for the PIC's menu and status display. NTP timing stays in TIME-NTP.

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | see below |
| 2 | wifi_state | u8 | `SML_WIFI_*` (§12.10) |
| 3 | rssi_dbm | i8 | Wi-Fi signal, dBm (`RSSI_VALID`) |
| 4 | ipv4 | u32 | address, a.b.c.d = a << 24 \| b << 16 \| c << 8 \| d (`IP_VALID`) |
| 8 | uptime_s | u32 | ESP uptime, s (a drop means the ESP restarted) |
| 12 | tzinfo_updated_utc | u32 | last successful time zone lookup (`TZINFO_VALID`) |
| 16 | tzinfo_refresh_utc | u32 | next lookup due (`TZINFO_ENABLED`) |
| 20 | esp_version | u32 | ESP firmware, major << 16 \| minor << 8 \| patch |

Flags:

| Bit | Name | Meaning |
| --- | --- | --- |
| 0 | `WIFI_CONNECTED` | Associated and up |
| 1 | `IP_VALID` | `ipv4` is set |
| 2 | `RSSI_VALID` | `rssi_dbm` is set |
| 3 | `NTP_ENABLED` | NTP configured |
| 4 | `NTP_SYNCED` | NTP time valid by the ESP's own policy |
| 5 | `TZINFO_ENABLED` | Remote time zone lookup enabled |
| 6 | `TZINFO_VALID` | The last lookup succeeded |
| 7 | `LOCAL_TZ_VALID` | A zone or rule is configured on the ESP itself, for when tzinfo is off |
| 8 | `TELNET_ENABLED` | |
| 9 | `TELNET_CLIENT` | A telnet client is connected |
| 10 | `UPDATE_AUTO` | Automatic firmware updates enabled |
| 11 | `UPDATE_AVAILABLE` | |
| 12 | `UPDATE_IN_PROGRESS` | The PIC can show it, and expect the ESP to restart |
| 13 | `SD_PRESENT` | V3 |

- **Sending.** STAT-NET is sent on change and every 10 s.
- **Staleness.** The PIC treats it as unknown after 30 s without one (`ESP_LINK_NET_TIMEOUT_MS`).

---

## 9. CMD class (0x80)

### 9.1 CMD-USER (0x10): one command, both ways, 10 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | command | u16 | `SML_USER_*` |
| 2 | flags | u16 | reserved, 0 |
| 4 | token | u16 | chosen by the ESP; echoed in the response |
| 6 | argument | u32 | per command; signed values in two's complement |

The command codes are fixed wire values, independent of the ESP's own `USER_CMD` text-command enum.

**Who runs what.**
- Codes below 0x8000 run on the **PIC** and are sent by the ESP. The tables below list them.
- Codes from 0x8000 (`SML_USER_ESP_FIRST`) run on the **ESP** and are sent by the PIC, for example from its menu.
- A receiver given a code from the other half answers NACK `UNSUPPORTED`.

| Code | Command | Argument | V2+ PIC |
| --- | --- | --- | --- |
| 0x0001 | `INFO` | — | ✓ prints every module's diagnostics as STAT-DEBUG, then ACK |
| 0x0002 | `REQUEST_MESSAGE` | class << 8 \| ID (upper 16 bits zero) | ✓ for TIME-AUTH, TIME-PIC, TIME-CTRL, TIME-GNSS and STAT-UI: the message, then ACK. Others: NACK UNSUPPORTED |
| 0x0010 | `RESYNC` | — | NACK UNSUPPORTED (planned) |
| 0x0011 | `RESET` | — | ✓ ACK, flush, then software reset |
| 0x0020 | `SET_RTC` | UTC seconds | NACK UNSUPPORTED (RTC not ported) |
| 0x0030 | `SET_TZ_OFFSET` | manual base offset, signed s | ✓ within ±86 399 |
| 0x0031 | `SET_TZ_AUTO` | 0/1 | ✓ |
| 0x0032 | `SET_DST_OFFSET` | manual DST amount, signed s | ✓ within ±86 399 |
| 0x0033 | `SET_DST_AUTO` | 0/1 | ✓ (§7.1) |
| 0x0034 | `SET_DST_ACTIVE` | 0/1 | ✓ (manual mode) |
| 0x0040 | `SET_FOSC` | Hz | NACK UNSUPPORTED (settings store) |
| 0x0050 | `SET_ALARM_ENABLE` | 0/1 | NACK UNSUPPORTED (UI) |
| 0x0051 | `SET_ALARM` | seconds of day | NACK UNSUPPORTED (UI) |
| 0x0052 | `SET_DELTA` | UTC epoch | NACK UNSUPPORTED (UI) |
| 0x0053 | `SET_BEEP` | 0/1 | NACK UNSUPPORTED (UI) |
| 0x0060 | `SET_DISPLAY` | `SML_DISPLAY_*` | ✓ HHMM; other selectable modes UNSUPPORTED; runtime-only values ARGUMENT |
| 0x0061 | `SET_BRI_AUTO` | 0/1 | NACK UNSUPPORTED (light sensor) |
| 0x0062 | `SET_BRIGHTNESS` | 200..3800 | ✓ |
| 0x0070 | `SHOW_SETTINGS` | — | NACK UNSUPPORTED (settings store) |
| 0x0071 | `SHOW_CONFIG` | — | NACK UNSUPPORTED |
| 0x0072 | `DEFAULTS` | — | NACK UNSUPPORTED (settings store) |
| 0x0073 | `SAVE` | — | NACK UNSUPPORTED (settings store) |
| 0x0080 | `ENTER_BOOTLOADER` | — | NACK UNSUPPORTED (bootloader in SML deferred) |

On the V2+ PIC, settings changed by command are applied at once but not yet persisted.

Commands run by the ESP:

| Code | Command | Argument | Purpose |
| --- | --- | --- | --- |
| 0x8010 | `ESP_RESET` | — | Restart the ESP |
| 0x8020 | `ESP_WIFI_CLEAR` | — | Forget Wi-Fi credentials (V2 menu: reset Wi-Fi) |
| 0x8021 | `ESP_WIFI_SETUP` | — | Start the setup access point |
| 0x8030 | `ESP_NTP_RESYNC` | — | Query NTP now |
| 0x8040 | `ESP_TZINFO_ENABLE` | 0/1 | Remote time zone lookup on or off |
| 0x8041 | `ESP_TZINFO_UPDATE` | — | Look up the time zone now |
| 0x8050 | `ESP_UPDATE_CHECK` | — | Check for firmware updates |
| 0x80F0 | `ESP_DEFAULTS` | — | Clear the ESP configuration (V2 menu: reset all, ESP part) |

- **Sending.** The V2+ PIC sends these with `esp_link_request()`, which numbers the tokens itself (never 0), and keeps the ESP's latest CMD-RESPONSE for `esp_link_response()`.
- **TEXT mode.** The PIC sends nothing in TEXT mode, where the ESP would not understand it.

### 9.2 CMD-RESPONSE (0x20): the answer, both ways, 12 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | status | u16 | `SML_RESULT_ACK` 0, `NACK` 1, `ACCEPTED` 2 |
| 2 | detail | u16 | `SML_ERROR_*` (§12.11) |
| 4 | token | u16 | the request's token |
| 6 | command | u16 | the request's command, or `SML_OPERATION_SETTINGS_WRITE` (0x0100) for CMD-SETTINGS |
| 8 | settings_revision | u32 | PIC answering: current settings revision, also on rejection (0 until the settings store exists). ESP answering: 0 |

- **ACK** means the command has completed.
- **ACCEPTED** means it has been queued. A final ACK or NACK with the same token follows later. Tokens must not be reused while their command is pending.
- **Unknown command codes** get a NACK with `UNSUPPORTED`.

### 9.3 CMD-SETTINGS (0x30): write every setting, ESP→PIC, 38 bytes

| Offset | Field | Type | Unit / values |
| --- | --- | --- | --- |
| 0 | flags | u16 | bit 0 `SML_WRITE_PERSIST`: also persist |
| 2 | reserved | u16 | 0 |
| 4 | token | u16 | |
| 6 | expected_revision | u32 | the revision the ESP last saw |
| 10 | settings | body | as §8.4 |

- **Applying.** The PIC validates every field, then applies them all together and increments the revision. A failure on any field rejects the whole write.
- **Stale writes.** A write whose `expected_revision` is not the current revision is rejected with `STALE_REVISION`, so the ESP never overwrites a change it has not seen. On ACK the response carries the new revision.
- **Persistence.** When the EEPROM is written is up to the PIC. Periodic snapshots never drive it.
- **V2+ PIC.** Answers NACK `UNSUPPORTED` until the settings store is ported.

---

## 10. Implementation notes

### 10.1 Codec (`esp/sml.c`, `esp/sml.h`)

The codec is portable C99 with no hardware dependency, written so the ESP can build the same file.

- **Message table.** Each message has its class, ID, fixed length (0 for STAT-DEBUG), direction and **layout**. The layout is the list of field sizes in wire order, e.g. TIME-AUTH is `{2, 1, 1, 4, 4, 4}`. The structs are packed, so a field's struct offset equals its wire offset, and one layout describes both.
- **`sml_encode` and `sml_decode`.** These convert between a packed native struct and the wire, one field at a time, through a variable of that field's width. Native byte order and alignment therefore never matter. The host tests check every layout against `sizeof` and the length constants, round-trip random data through every message, and check the byte order of every field.
- **`sml_frame` and `sml_frame_message`.** These build complete frames; the second encodes straight from the native struct.
- **The parser.** `sml_parser_push`, `sml_parser_next` and `sml_parser_timeout` together implement the receiving rules of §2.3. The parser buffers at most one frame (248 bytes), so it cannot overflow.

### 10.2 Packing

`#pragma pack(push, 1)` is required on the ESP32, where `uint32_t` is 4-byte aligned.

On XC16 every payload already has the same layout without packing, because the compiler aligns to at most 2 bytes and every 32-bit field sits at an even offset. The compile-time size checks at the end of `serial_proto.h` confirm the sizes on every compiler.

### 10.3 dsPIC V2+ modules

| Module | Role |
| --- | --- |
| `esp/serial_proto.h` | Copy of `common/serial_proto.h`, until the V2+ tree moves into `dspic_src/` and includes `../../common/` |
| `esp/sml.c/.h` | The codec |
| `esp/esp_link.c/.h` | UART1 link: mode, receive and dispatch, commands, debug text, per-epoch messages |
| `esp/esp_report.c/.h` | Payload builders and the frozen wire mappings from firmware enums |
| `ui/timezone.c/.h` | TZ-STATE application, the manual offsets, stale and ambiguous flags |
| `sync/sync.c` | `sync_set_leap()` (TIME-LEAP) and `sync_uncertainty()` (TIME-AUTH bound) |
| `ui/console.c` | `console_set_sink()`: routes printf into STAT-DEBUG in SML mode |

- **Scheduling.** `esp_link_service()` runs every 10 ms. `esp_link_epoch()` runs right after `sync_update()`, before the display is prepared and before the console reports, so TIME-AUTH leaves first.
- **Bandwidth.** The six per-epoch messages take 284 bytes a second, 2.5 % of the link. The console report adds about 0.6–1 KB a second, as before.

### 10.4 ESP side (to do, with the ESP overhaul)

- **Build.** Copy `common/serial_proto.h` and `esp/sml.c/.h` into the sketch. The Arduino IDE cannot include files from outside the sketch folder, so add a copy step, or move the shared files into a library under `esp_src/libraries/`.
- **Legacy copies.** `esp_src/enums.h` and `esp_src/serial_proto.h` are the legacy 0x83 versions, kept so the current ESP firmware still builds. They are removed by the overhaul.
- **Mode announcement.** At boot, send CMD-USER `INFO` to put the PIC into SML mode. Then treat STAT-DEBUG payloads as the console stream.
- **STAT-NET.** Send it on change and every 10 s.
- **ESP commands.** Answer the PIC's `SML_USER_ESP_*` commands with CMD-RESPONSE.
- **TIME-AUTH.** Pair each one with the PIC epoch edge captured on GPIO27 for the same second.
- **TZ-STATE.** Assemble it per the time zone architecture document (source selection, horizon, ambiguity) and resend it as §7.1 says.
- **TIME-LEAP.** Prefer the PIC's GNSS leap fields, falling back to the server's leap object.

### 10.5 Statistics and Allan deviation (ESP)

TIME-MEAS gives three phase series. TIME-CTRL adds the loop's view of them.

| Series | From | Measures | Resolution |
| --- | --- | --- | --- |
| Output against GNSS, fine | `mark_offset_ns` (labelled epoch − 1) | The disciplined epoch output, timed by the receiver: the clock's actual performance | Receiver time base: about 1 ns, with `mark_accuracy_ns` |
| Output against reference, coarse | `phase_ticks` | The same, by the PIC's own captures | 25 ns |
| Oscillator against reference | Σ (`reference_period_ticks` − nominal) | The free-running oscillator, unsteered: the basis of holdover and of the Kalman model | 25 ns per edge; better with `qerr_ps` |

- **Forming phase.**
  - Output phase x_k is `mark_offset_ns`, or `phase_ticks` × 25 ns.
  - Oscillator phase is the running sum x_k = x_{k−1} + (`reference_period_ticks` − `nominal_period_ticks`) × 25 ns.
  - ADEV(τ) then follows from the second differences of x at τ = n seconds.
- **qErr.** It corrects the receiver's sawtooth on the reference edge. Its sign is still to be confirmed on hardware (§14): correct one edge by hand before relying on it.
- **Discontinuities.** Restart a series at:
  - a gap in `epoch_sequence` or `reference_sequence`;
  - `controller_state` other than TRACK, for output series;
  - `MEASUREMENT_USED` clear, for output series;
  - a change of `source`.
  - Phase steps happen only in STEP, which is outside TRACK.
- **The Kalman state** (`frequency_offset_q16`) is a filtered estimate. It is right for monitoring the model, but **not** for ADEV at short τ, where the filter suppresses the noise being measured.
- **Storage.** A TIME-MEAS record plus the TIME-CTRL fields worth keeping is about 60 bytes a second, or 5 MB a day. That fits on the V3 SD card. On V2, decimate, or keep running ADEV accumulators instead.

## 11. Send policy summary

| Message | Sender | When |
| --- | --- | --- |
| TIME-AUTH, TIME-PIC, TIME-CTRL, TIME-MEAS, TIME-GNSS, STAT-UI | PIC | every epoch, in this order, within a few ms of the edge (SML mode only) |
| TIME-RTC, STAT-ENV, STAT-SUPPLY, TIME-VCTCXO | PIC | when ported; RTC and VCTCXO every epoch, sensors every few seconds |
| STAT-SETTINGS | PIC | on change, after reset, on `SHOW_SETTINGS` |
| STAT-DEBUG | PIC | as text is produced |
| CMD-RESPONSE | both | one per CMD-USER or CMD-SETTINGS received, plus a final one for each ACCEPTED |
| CMD-USER (ESP codes) | PIC | on demand (menu) |
| STAT-NET | ESP | on change, and every 10 s |
| TIME-NTP | ESP | after each ESP PPS edge while it has time |
| TIME-LEAP | ESP | on change, and every ~10 min |
| TZ-STATE | ESP | on change, whenever STAT-UI shows `TZ_HELD` clear, and every ~10 min |
| CMD-USER | ESP | on demand; `INFO` once at boot |

## 12. Constants

### 12.1 Sources: `SML_SOURCE_*`

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | INIT | Not yet determined (start-up) |
| 1 | NONE | No source |
| 2 | RTC | Battery-backed RTC |
| 3 | ESP | ESP holdover or internal time, not a fresh NTP fix |
| 4 | NTP | NTP via the ESP |
| 5 | GNSS | GNSS receiver |

These values deliberately differ from the retired `CLOCK_SOURCE` enum.

### 12.2 Lock state: `SML_LOCK_*`

| Value | Name | V2+ synchroniser state |
| --- | --- | --- |
| 0 | UNINITIALISED | no epoch processed yet |
| 1 | ACQUIRING | ACQUIRE |
| 2 | LOCKED | LOCKED |
| 3 | HOLDOVER | HOLDOVER (within its limit, 1 h) |
| 4 | FREE_RUNNING | FREE_RUN |

### 12.3 Synchroniser state: `SML_SYNC_*` (TIME-PIC; frozen 2026-10-19)

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | INIT | No epoch processed yet |
| 1 | FREE_RUN | No source: last frequency estimate; time of day counted on |
| 2 | ACQUIRE | Source qualified: learning frequency, stepping onto its edge |
| 3 | LOCKED | Loop tracks the selected source |
| 4 | HOLDOVER | Selected source failed: frequency held, within the limit |

### 12.4 Cause of the latest change: `SML_CAUSE_*` (TIME-PIC; frozen 2026-10-19)

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | NONE | No change yet |
| 1 | QUALIFIED | A source qualified with none selected |
| 2 | TRACKING | The loop is tracking |
| 3 | PHASE_LOST | The loop lost phase and re-acquires |
| 4 | SOURCE_LOST | The selected source failed |
| 5 | SOURCE_BACK | … and is good again |
| 6 | ALTERNATIVE | Another source took over a failed one |
| 7 | BETTER_SOURCE | A better-ranked source took over |
| 8 | EXPIRED | Holdover limit reached |

### 12.5 Controller state: `SML_CONTROL_*` (TIME-CTRL, TIME-VCTCXO; frozen 2026-10-19)

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | INIT | Not running |
| 1 | ACQUIRE | Waiting for reference periods before the step |
| 2 | STEP | Phase step loaded, being followed |
| 3 | TRACK | PI servo on the Kalman frequency estimate |

### 12.6 Time zone source: `SML_TZ_SOURCE_*`

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | NONE | Nothing (`VALID` clear) |
| 1 | CACHE | The ESP's cached state |
| 2 | GEOIP | Server lookup by IP address |
| 3 | GNSS | Lookup by GNSS position |
| 4 | MANUAL | A rule or offsets entered by the user on the ESP (tzinfo off) |
| 5 | PINNED | Zone pinned by name, rules automatic |
| 6 | SPECIAL | On-device special timebase (GMT, DMT, TAI, Mecca, …) |

### 12.7 Leap source: `SML_LEAP_SOURCE_*`

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | NONE | None |
| 1 | SERVER | tzinfo leap object or release artifact |
| 2 | GNSS | UBX-NAV-TIMELS, via the PIC |

### 12.8 RTC type: `SML_RTC_*`

| Value | Name |
| --- | --- |
| 0 | NONE |
| 1 | DS1307 |
| 2 | PCF8563 |
| 3 | RV3028: Micro Crystal RV-3028-C7 (V3) |

### 12.9 Display, button, menu (STAT-UI, settings; frozen 2026-10-19)

The display values match the V2 `UI_DISPLAY_STATE` order, so V2 EEPROM values carry over.

| Value | `SML_DISPLAY_*` | Selectable |
| --- | --- | --- |
| 0 | INIT | no |
| 1 | HHMM | yes (V2+ ✓) |
| 2 | MMSS | yes |
| 3 | SSMM | yes |
| 4 | YYYY | yes |
| 5 | MMDD | yes |
| 6 | DELTA | yes |
| 7 | DASHES | no: shown while the time is unknown |
| 8 | TEMP | no: runtime only |
| 9 | MENU | no: runtime only |

**Buttons:** `SML_BUTTON_NONE` 0, `SHORT` 1, `LONG` 2, `HOLD` 3.

**Menu:** `SML_MENU_NONE` 0. The remaining menu values are frozen when the menu is ported.

### 12.10 Wi-Fi state: `SML_WIFI_*` (STAT-NET)

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | UNKNOWN | Not reported yet |
| 1 | OFF | Wi-Fi disabled |
| 2 | NOT_CONFIGURED | No credentials |
| 3 | CONNECTING | |
| 4 | CONNECTED | |
| 5 | DISCONNECTED | Configured, but the link is lost or failed |
| 6 | PROVISIONING | Setup access point active |

### 12.11 Results and errors

| Status | Value | | Detail | Value |
| --- | --- | --- | --- | --- |
| `SML_RESULT_ACK` | 0 | | `SML_ERROR_NONE` | 0 |
| `SML_RESULT_NACK` | 1 | | `SML_ERROR_UNSUPPORTED` | 1 |
| `SML_RESULT_ACCEPTED` | 2 | | `SML_ERROR_LENGTH` | 2 |
| | | | `SML_ERROR_ARGUMENT` | 3 |
| | | | `SML_ERROR_BUSY` | 4 |
| | | | `SML_ERROR_STALE_REVISION` | 5 |
| | | | `SML_ERROR_SCHEMA` | 6 |
| | | | `SML_ERROR_STORAGE` | 7 |
| | | | `SML_ERROR_VERIFY` | 8 |
| | | | `SML_ERROR_STATE` | 9 |

## 13. Golden frames

These frames are produced by the reference codec and checked byte for byte in `tests/host/test_sml.c`.

**TIME-AUTH**: locked to GNSS, 2026-10-18T12:34:56Z (1792326896), edge 150 ns late, ±75 ns.

```
B5 73 10 01 00 10 00 07 05 02 6A D4 BC F0 FF FF FF 6A 00 00 00 4B 0F F3
```

**CMD-USER**: `SET_TZ_OFFSET` −18 000 s (UTC−5), token 0x1234.

```
B5 73 80 10 00 0A 00 30 00 00 12 34 FF FF B9 B0 11 76
```

**STAT-DEBUG**: the text `"Hi\r\n"`.

```
B5 73 30 F0 00 04 48 69 0D 0A 35 88
```

**TZ-STATE**: London as in §7.1, issued 1791911413.

```
B5 73 20 01 00 7C 00 01 03 08 6A CE 65 F5 72 66
17 10 00 00 00 00 69 C8 79 90 00 00 00 00 0E 10
01 00 6A DD 54 90 00 00 00 00 0E 10 00 00 6B A8
5B 90 00 00 00 00 0E 10 01 00 6C C6 71 10 00 00
00 00 0E 10 00 00 6D 88 3D 90 00 00 00 00 0E 10
01 00 6E A6 53 10 00 00 00 00 0E 10 00 00 6F 68
1F 90 00 00 00 00 0E 10 01 00 70 86 35 10 00 00
00 00 0E 10 00 00 71 51 3C 10 00 00 00 00 0E 10
01 00 8F 3D
```

## 14. Open points

1. **Settings store.** STAT-SETTINGS, CMD-SETTINGS, `SAVE`, `DEFAULTS`, `SHOW_SETTINGS` and `SET_FOSC` wait for the EEPROM and settings port. The TZ schedule's persistence hook (`timezone_take_changed()`) is ready for it.
2. **Menu values.** The `SML_MENU_*` values beyond NONE are frozen when the V2+ menu exists.
3. **`RESYNC` semantics.** Possible meanings are restarting the control loop, re-qualifying sources, or both. To be defined with the NTP source.
4. **NTP as a sync source.** TIME-NTP is received and held. The NTP source adapter uses it together with the ESP PPS on RB15. It needs its own uncertainty limit and per-source loop parameters.
5. **Bootloader in SML.** The class 0xF0 payloads are still to be designed. The existing bootloader path stays outside SML until then.
6. **Sub-second offset.** It is rounded to the second. Applying it as a display phase shift is possible later, by delaying the latch for that zone, but it is not planned.
7. **qErr sign.** To be confirmed against captures on hardware before qErr is used in the phase measurement or in the ESP's statistics.
8. **Time zone architecture document.** Its §6.1 should follow the model of §7.1 here:
   - the ESP no longer needs to know the PIC's TZ auto setting, and always sends its best schedule;
   - `SML_TZ_SOURCE_MANUAL` now means a rule or offsets entered on the ESP;
   - the "manual offsets, the ESP does not override" row is replaced by the PIC's TZ_AUTO and DST_AUTO settings.

**Resolved in 1.2:**
- `DST_AUTO` now selects DST from the ESP's schedule, independently of `TZ_AUTO`.
- The network status is a STAT-NET message of its own.
- TIME-CTRL is lossless, and TIME-MEAS carries the raw data for statistics.

## 15. Changes

| Date | Change |
| --- | --- |
| 2026-10-19 (1.2) | TIME-CTRL redefined as lossless Kalman and servo fixed-point state (28 → 56 bytes). TIME-MEAS (0x13) added: raw per-epoch measurements for statistics and ADEV. STAT-NET (0x40, ESP→PIC) added; TIME-NTP flag bit 0 (`WIFI_CONNECTED`) retired into it. CMD-USER and CMD-RESPONSE both ways: codes 0x8000 and up run on the ESP (`SML_USER_ESP_*`). Time zone: the PIC always holds the latest TZ-STATE; `TZ_AUTO` selects the base offset and `DST_AUTO` the DST state from it, independently; `SML_UI_TZ_HELD`; `SML_TZ_SOURCE_MANUAL` redefined as ESP-side manual configuration. `SML_RTC_*` 3 is the RV-3028-C7. |
| 2026-10-19 (1.1) | Frozen wire mappings: `SML_SYNC_*`, `SML_CAUSE_*`, `SML_CONTROL_*`, `SML_DISPLAY_*`, `SML_BUTTON_*`, `SML_MENU_NONE`. TIME-AUTH uncertainty defined as a ± bound. `SML_*_BYTES` length constants for every message. Link modes (§3) and receiver rules (§2.3) specified. `common/enums.h` loses the enums SML now defines. No wire layout change. |
| 2026-10-14 | Minor version 1: TZ-CURRENT and TZ-FUTURE merged into the atomic TZ-STATE (ID 0x10 retired). Per-entry DST-active flag. `horizon_end_utc`. `subsecond_offset_ns`. `AMBIGUOUS` flag. TIME-LEAP added. GNSS leap fields in TIME-GNSS. `TZ_STALE` and `TZ_AMBIGUOUS` in STAT-UI. Transition table removed from the settings body (88 → 28 bytes). |
