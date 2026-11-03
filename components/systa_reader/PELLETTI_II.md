# Pelletti II monitoring

The `examples/pelletti-ii.yaml` configuration enables both `palletti_ii`
(measurements, FC 0C01) and `comfort` (controller status, FC 0C02 and
FD AA0C firmware announcements). It is a read-only UART observer.
Copy the YAML to the ESPHome config directory and provide the referenced
keys in your own `secrets.yaml`. The external component is pinned to a
commit in the fork so rebuilding uses the same decoder revision.
It was validated with ESPHome 2026.10.1 on a NodeMCU ESP8266.

## Reader connection diagnostics

The UART is receive-only on GPIO13. Leave `tx_pin` unset so ESPHome can
select hardware UART0 in its swapped pin mapping. Combining TX GPIO1
with RX GPIO13 selects software serial instead. UART logging remains
disabled with `logger.baud_rate: 0` to free the hardware interface.

Uptime, Wi-Fi signal, free heap and reset reason describe the ESP reader,
not the boiler. Compare uptime across Home Assistant availability gaps:
a lower uptime confirms a reader restart even when HA reconnects quickly.
The reset reason is the SDK report, not a diagnosis of the underlying
cause. In particular, `Software/System restart` does not identify which
code path requested the restart.

## Fields confirmed in the capture

On 2026-09-27, 100 seconds of Pelletti II traffic contained ten valid
measurement frames, ten valid controller status frames and twenty
firmware announcements. All 1,728 captured frames had valid checksums.
The observed temperature/status interval was approximately 10 seconds.

| Entity | Kind | Observed idle value |
| --- | --- | --- |
| Boiler operating hours | `comfort_bst` | 662 h |
| Boiler starts | `comfort_kst` | 7,030 |
| Requested room temperature | `comfort_ti_s` | 0 °C |
| Requested heating flow temperature | `comfort_tv_s` | 0 °C |
| Requested hot-water temperature | `comfort_tw_s` | 0 °C |
| Heating circuit operating mode | `comfort_ba1` | 7 |
| Heating circuit operating level | `comfort_niv1` | 0 |
| Sensor error code | `comfort_sens` | 0 |
| Heating circuit pump command | `comfort_phk1` | 0% |
| Boiler pump command | `comfort_pkes` | 0% |
| Heating circuit pump active | `comfort_stat_phk` | off |
| Boiler pump active | `comfort_stat_pk` | off |
| Burner contact | `comfort_stat_b1` | off |
| LON module status | `comfort_stat_lon` | on |
| Last boiler error word | `comfort_boiler_err` | 0xFFFF |
| Controller firmware | `comfort_fw_version` | 2.4.2 |

Setpoints are controller requests, not measured temperatures. Pump and
burner bits report controller status, not proof of water flow or flame.
No active-heating transition was observed. Compare the counters and active
setpoints with the boiler menu before using them in control automations.

The independent [SystaBridge decoder](https://github.com/marvinGitHub/systa-bridge/blob/c1c3bd0a2b3008646ac4119fbfda413119a9cfc9/src/Monitor.php)
corroborates the byte offsets. Its operating-mode mapping is 0/1/2 =
automatic programs 1/2/3, 3 = heating, 4 = comfort, 5 = reduced,
6 = summer, 7 = disabled, 17 = system off. Operating level remains numeric
until verified on the boiler. Its configuration ignores boiler error
values 255 and 65535; this example keeps the error word raw and creates
no fault alarm from it.

## Freshness and display behavior

Numeric and binary entities use native ESPHome timeout filters. Valid
frames refresh these filters even when values do not change. Two minutes
without matching frames makes numeric readings unavailable and binary
states unknown; corrupt or short frames cannot refresh them.
The firmware string and explicitly named *Last Boiler Error Word* retain
the last observed diagnostic value; they are not live fault indicators.

The display decoder publishes changed text to an internal text sensor.
A template text sensor samples its latest value every 60 seconds and
suppresses identical text. Thus a final change is delivered at the next
sample even if no more display frames arrive. The first screen can take
up to 60 seconds after boot. This is a snapshot of an alternating screen,
not a reconstruction of every page shown during the minute.

## Unused equipment and legacy naming

No second circuit, buffer tank or solar controller is configured in this
installation. Their registers are omitted. Observed unused probe codes
0xFEDB and 0xFEDD are converted to NaN instead of temperatures above
6,500 °C; their exact fault meanings are not inferred. Signed outdoor
readings remain valid. Zero itself is not treated as a missing probe.

TPO means upper-buffer temperature, **not** hot-water setpoint. The example
retains the legacy firmware name `Hot Water Temperature Target (TPO)` to
preserve the installed entity's unique ID and history. Its Home Assistant
display name is overridden to `Upper Buffer Temperature (TPO)`. There is no
buffer in this installation and its zero reading is not actionable.
Use the new requested hot-water temperature entity for the setpoint.

## Source provenance and regression checks

Based on upstream commit `3e8674d9b53e5d4b8481250fb9fc6e1cb4ffeda5`.
The previously deployed local copy already contained checksum rejection
and the Pelletti display-publisher fix. Both are retained here.

Run `python3 tests/systa/run.py` with g++ installed. The test compiles the
real parser and decoders against small ESPHome doubles with AddressSanitizer
and UndefinedBehaviorSanitizer. It covers captured frames, every short
Comfort payload length, checksum failures/recovery, repeated-value
publication, display routing, signed outside temperature, missing probes
and unsigned counters. Leak detection is disabled because decoder
instances are allocated for the lifetime of the embedded component.
Full ESPHome compilation additionally checks code generation and filters.
