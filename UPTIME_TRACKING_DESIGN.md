# Tracking uptime per device

Written against `microlog-to-microsd` at 560630c. The firmware side is built
and tested; the server side is built and has been run against the live API.
Nothing here is reviewed or merged yet. See [Implementation
status](#implementation-status) for what stands where.

A "device" here is one deployed node, stm32 running Tock plus the esp32
coprocessor, identified by `UserConfiguration.logger_id`.

## What we actually want to know

"Uptime" collapses three separate questions that need different mechanisms:

1. **Session uptime.** Seconds since this boot. Cheap, monotonic, always available.
2. **Reset churn.** How many times has it restarted, and were the restarts clean?
   A node that reboots every 40 minutes has excellent session uptime and is still
   broken.
3. **Availability.** What fraction of the deployment window was the node alive?
   This is the number anyone actually asks for.

(3) cannot be measured from inside the device, because the device is off during
exactly the interval being measured. Something has to survive the outage.

## The cheapest version needs no firmware at all

Every uploaded `SensorMeasurement` already carries `Metadata.ts`, and uploads are
paced by `UserConfiguration.Upload_interval`. So the backend can already derive
availability from arrival gaps: expected packets versus received packets over a
window. Zero firmware change, works retroactively on data already collected, and
it can be written today against `ents.dirtviz.BackendClient`.

Worth doing first, and worth being clear about why it is not sufficient:

**Arrival gaps cannot distinguish "the node was powered off" from "the node was
running fine but the radio or backend failed."** Those two have opposite fixes.
Everything below exists to separate them. If that distinction does not matter for
the current deployment, stop here and do the server-side version only.

This part is now built: `python/src/ents/dirtviz/uptime.py`, with 27 unit tests
in `python/tests/test_dirtviz_uptime.py` and a CLI:

```
python -m ents.dirtviz.uptime --days 14
```

Run against the live API on 2026-08-25, over a 14 day window: **18 cells
reported at all, and all 18 were reporting at the time of the run.** Fourteen
had no gaps whatsoever. The four in the `S#3-*` group show 60% availability,
but the entire deficit is a single open-ended gap ending 2026-08-16 19:54,
which is the moment they first appear in the data. That is almost certainly
their deployment date rather than an outage, and it is exactly the ambiguity a
gap-based method cannot resolve on its own. Everything else in the deployment
looks healthy.

Two things that fell out of running it, both of which change how the numbers
should be read:

**The API resamples by default, and the default hides outages.** `/power/{id}`
returns hourly buckets unless `resample=none` is passed, regardless of how
often the node uploads. Against hourly buckets every node looks like it uploads
once an hour, so nothing shorter than a multi-hour outage is visible at all.
With raw data the real intervals are 5 minutes for the `nu_*` cells and **14
seconds** for the `S#*` cells. `power_data()` and `teros_data()` did not accept
a `resample` argument, and `sensor_data()` accepted one and silently dropped it
on the floor; both are fixed in `client.py`.

**A threshold scaled purely off the upload interval is unusable at 14 second
cadence.** Interval times the 2.5 tolerance gives 35 seconds, and the ordinary
lag between a node uploading and dirtviz serving the row then reads as an
outage, so all eight `S#1`/`S#2` cells were reported down at the moment of the
run. Hence `MIN_GAP_S`, a 300 second floor under the threshold. It costs
nothing in detection, since no outage shorter than five minutes matters for a
soil deployment, and it is the difference between a report someone trusts and
one they learn to ignore.

## Going through dirtviz

This is the right surface, and the existing client shape is what makes the
firmware decision easy.

`BackendClient.sensor_data()` in `python/src/ents/dirtviz/client.py:132` is
already generic:

```python
endpoint = "/sensor/"
params = {"cellId": cell.id, "name": name, "measurement": meas}
```

It is parameterized by `name` and `measurement` strings rather than having a
hardcoded route per sensor, unlike `power_data()` and `teros_data()` which hit
`/power/{id}` and `/teros/{id}`. So **if uptime is emitted as a `SensorType`,
dirtviz serves it through an endpoint that already exists**:

```python
client.sensor_data(cell, name="device", meas="uptime", start=..., end=...)
```

No new route, no new client method, and it returns the same timestamped
DataFrame as everything else, so it plots against sensor data with `plots.py`
unchanged. That was the strongest argument for Option A below over a dedicated
message: the dedicated message would need its own route, its own client method,
and its own plotting path.

**The argument holds, and the whole thing was blocked on five missing lines in
this repo.** The dirtviz source is public at `jlab-sensing/dirtviz`, so this no
longer has to be settled by asking.

The ingest path, `backend/api/resources/util.py`:

```python
meas_dict = {"type": m["type"], ..., "data": {m["name"]: value}}
obj = Sensor.add_data(m["name"], m["unit"], meas_dict=meas_dict)
```

and `Sensor.add_data()` in `backend/api/models/sensor.py` **creates the sensor
row if it does not exist**. There is no allowlist, no mapping table, no
per-sensor route. A new `SensorType` registers itself the first time one
arrives, and `/sensor/` serves it immediately. Option A was right.

The catch is one level up. `parse_sensor_measurement()` is called before any of
that, and it lives *here*, in `python/src/ents/proto/sensor.py`:

```python
meta = SENSOR_DATA[SensorType.Value(meas_type)]   # bare lookup, no default
```

`SENSOR_DATA` had no entries for the five `DEVICE_*` counters, so any uplink
carrying one raised `KeyError`. `process_generic_measurement()` catches that
and returns **HTTP 400 for the entire batch**. So flashing a node with the
counters would not merely have failed to record uptime, it would have **thrown
away that node's real sensor data too**, every batch, silently, from the
backend's point of view looking like a malformed uplink.

Fixed: the five entries are added, and `test_sensor_metadata_is_complete`
asserts every `SensorType` has metadata so the next enum value cannot repeat
this. Verified end to end by encoding a batch of a voltage reading plus four
counters and running the exact call dirtviz makes:

```
sensor.name                sensor.measurement      value  unit
POWER_VOLTAGE              Voltage                3312.5  mV
DEVICE_BOOT_COUNT          Boot Count                 47  count
DEVICE_UNCLEAN_BOOTS       Unclean Boots              12  count
DEVICE_UPTIME              Uptime                  86400  s
DEVICE_DOWNTIME            Downtime                 7200  s
```

Nothing in the dirtviz repo needs to change. What does need to happen is a
release of the `ents` python package, since dirtviz imports
`parse_sensor_measurement` from it and will keep rejecting the counters until
it picks up a version containing these entries.

### Why the earlier probing was misleading

An earlier version of this document reported 1728 `(cell, name, measurement)`
combinations returning empty and concluded the generic table was never
populated. That conclusion was wrong, and the reason is worth recording.

The probe guessed the naming scheme. The real one is `sensor.name` = the
`SensorType` **enum name** and `sensor.measurement` = the human readable name
from `SENSOR_DATA`, so the correct query is
`name=POWER_VOLTAGE&measurement=Voltage`, not `name=power&measurement=v`. Every
guessed spelling missed.

Worse, the endpoint cannot be probed this way at all: `get_sensor_data_obj()`
returns the same well-formed empty series whether the sensor is unknown or
merely has no data in the window, so a wrong name is indistinguishable from a
dead node. The only reliable discovery route is `/cell/<id>/sensors`, which
lists what is actually registered:

```
/cell/2439/sensors -> [{"name": "POWER_VOLTAGE", "measurement": "Voltage", "unit": "mV"},
                       {"name": "POWER_CURRENT", "measurement": "Current", "unit": "uA"}]
```

So cell 2439 has used the generic path, on 2026-07-10. The table works; those
cells simply upload through the legacy fPort 1 route most of the time.

That is also the tell that was visible in the failed probe and got missed: the
empty response for cell 2439 carried `unit: "mV"`, which only happens when the
sensor row exists. The other cells returned `unit: ""`. A single field
distinguished "registered but quiet" from "never seen", and it was in the
output all along.

### Two ingest paths, selected by fPort

Worth knowing, since it explains which table anything lands in:

| fPort | dirtviz handler | Tables | Read endpoint |
|---|---|---|---|
| 1 | `process_measurement` | `power_data`, `teros_data` | `/power/`, `/teros/` |
| 2 | `process_generic_measurement` | `sensor`, `data` | `/sensor/` |
| 202 | ignored, timesync | | |

`lorawan.cc:26` sets `fport = 2`, so current firmware already targets the
generic path. Anything still arriving on fPort 1 is running older firmware.

### Still worth raising

**dirtviz is keyed by cell, uptime is a property of the logger.** Every data
method in the client takes `cell.id`. But `/logger/` exists on the API and is
routed in `backend/api/__init__.py:137` as `/logger/<int:logger_id>`; it
returns 401 rather than 404, so it is real and authenticated, just absent from
the python client. `Metadata` carries both `cell_id` and `logger_id`, and one
logger can serve several cells, so a dead logger currently reads as several
independent dead cells. The question for John is narrow: can the client get a
token for `/logger/`, and should the counters be filed there instead?

**Should an unmapped `SensorType` really discard the whole batch?** The bare
lookup is defensible for catching mistakes early, but the blast radius is
disproportionate: one unknown counter loses every real measurement batched with
it, and the node has no way to know. A fallback that stores the unknown type
under its enum name and keeps the rest of the batch would be more forgiving.
That is a behaviour change affecting every deployment, so it is John's call
rather than something to slip in.

## On-device design

### Storage: FRAM

The MB85RC1MT is 131072 bytes. The measurement FIFO occupies `FRAM_BUFFER_START`
(0) through `FRAM_BUFFER_END` (1769). About 129 KB is unallocated.

FRAM is the right medium and this is not incidental. It has no erase cycle and
roughly 10^13 write endurance, so rewriting a liveness heartbeat every few
seconds forever is free. The same pattern on flash or EEPROM would burn the part
out inside a season.

Place the record high, away from the FIFO, so the buffer can grow later without
collision. Two copies for ping-pong writes:

```
0x1F000  record A
0x1F100  record B
```

### The record

```c
/* libents/util/uptime.h */
#define ENTS_UPTIME_MAGIC 0x454E5455u /* "ENTU" */

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;               /* bit0 clean_shutdown, bit1 time_valid */
  uint32_t seq;                 /* ++ per write, picks the newer of A/B */
  uint32_t boot_count;
  uint32_t unclean_boots;
  uint32_t session_seconds;     /* this boot, from alarm ticks */
  uint32_t cumulative_seconds;  /* all boots */
  uint32_t boot_epoch;          /* RTC at first valid timesync this boot */
  uint32_t heartbeat_epoch;     /* last moment we know we were alive */
  uint32_t downtime_seconds;    /* measured, summed across outages */
  uint32_t crc32;
} ents_uptime_record_t;         /* 44 bytes, measured */
```

Ping-pong plus CRC is not defensive padding here. The failure this record exists
to measure is the brownout, and a brownout landing mid-write is therefore an
expected event rather than a rare one. One copy would lose the history at exactly
the moment the history became interesting. On read, take the valid copy with the
higher `seq`; if neither validates, treat it as a first-ever boot.

### Clocks, and which one to use where

Three clocks are available and each is broken in a different way, so mixing them
up produces plausible-looking nonsense.

| Clock | Reset-safe | Failure mode |
|---|---|---|
| Alarm ticks, `libtock_alarm_command_read()` | no | 32-bit, wraps every 74 h at the measured 16 kHz |
| RTC epoch, `epoch()` | **no on the bench unit** | reads 946684800 at boot, arbitrary until timesync |
| esp32 `RTC_DATA_ATTR` | deep sleep only | lost on real power loss |

**The RTC does not survive a reset on the bench board.** Measured: `epoch()`
returns 946684800 immediately after reset, which is 2000-01-01, the STM32 RTC
power-on default. So VBAT is not backed on that unit. Whether the field boards
differ is worth checking, but the design must not assume it.

This matters less than it first appears, and the probe run shows exactly why.
Downtime is still measurable, but the subtraction has to happen **after timesync,
not at boot**:

```
boot N:    ... timesync ... heartbeat writes heartbeat_epoch = T1 (valid)
           power cut
boot N+1:  epoch() == 946684800, useless
           ... timesync ...
           epoch() == T2, now valid
           downtime = T2 - T1        <-- correct, and only computable here
```

The probe checks at boot, before any timesync, and therefore correctly reports
`n/a, need a valid clock on both boots`. That is the gating logic working, not a
failure. The real implementation must defer the downtime calculation until
`lorawan_timesync()` returns rather than doing it in the boot sequence.

If timesync never succeeds on the new boot, downtime stays unmeasurable and the
node can only report *that* it restarted, not for how long. That case is still
worth reporting, since an unclean boot count alone distinguishes a rebooting node
from a merely unreachable one.

Rules that follow:

- **Session uptime comes from alarm ticks, never from subtracting two epochs.**
  `lorawan_timesync()` can step the RTC by an arbitrary amount partway through a
  session, so an epoch difference can come out negative or wildly large. Ticks
  are monotonic and valid before any network exists.
- **Wall clock comes from the RTC, and only after timesync.** In `apps/core/main.cc`
  the sequence is `ControllerInit()` then `UserConfigStart()` then
  `lorawan_timesync()` at line 148. Anything epoch-stamped before line 148 is
  untrustworthy on a cold start with a flat backup cell. Hence the `time_valid`
  flag: record the epoch fields only once timesync has returned, and let the
  backend discard epoch fields from records that never got there.

### Tick wrap is a real constraint, but a looser one than expected

The counter is 32 bits, so the wrap period is `2^32 / frequency`.

**Measured on the bench stm32wle5jc: 16000 Hz, giving a wrap period of 268435 s,
which is 74 h 33 m.** That is roughly double what a 32768 Hz assumption would
have predicted, so the heartbeat has far more slack than this document originally
claimed. Sampling at a tenth of the wrap is about every 7.5 hours, which is
comfortably slower than any plausible upload interval.

Accumulating across wraps is straightforward with unsigned arithmetic:

```c
uint32_t now;
libtock_alarm_command_read(&now);
uint32_t delta = now - last_ticks;   /* correct across a single wrap */
last_ticks = now;
accum_ticks += delta;
```

This is only correct if sampled **more than once per wrap period**. The core main
loop blocks on `yield_for(&has_data)` at `main.cc:161`, which can in principle sit
idle longer than that, so the heartbeat still wants its own
`libtock_alarm_repeating_every_ms()` rather than riding whenever a measurement
happens to arrive. At 74 hours of headroom this is cheap insurance rather than a
tight deadline.

Verified on hardware. The probe samples ticks five times two seconds apart and
the deltas are exact, and the wrap arithmetic above was separately exercised on
the host against a counter set 256 ticks short of 2^32.

### Boot sequence

Inserted early in `apps/core/main.cc`, before the network comes up:

1. Read A and B, validate, take the newer.
2. `boot_count++`.
3. If `clean_shutdown` was not set, `unclean_boots++`. This is the watchdog and
   brownout counter.
4. Reset `session_seconds`, snapshot `last_ticks`.
5. Clear `clean_shutdown`, `seq++`, write to the older slot.
6. Once timesync lands: if the stored `heartbeat_epoch` was valid, then
   `downtime_seconds += epoch() - heartbeat_epoch`. Set `boot_epoch`, set
   `time_valid`.
7. Emit a boot banner through `ulog_info()`.

Step 7 costs nothing extra and is already wired: the `microlog-to-microsd`
branch routes ulog output to `/stm32.log` on the card, so the boot history
becomes human-readable on the card with no protocol work.

### Periodic

On each heartbeat alarm: sample ticks, accumulate into `session_seconds` and
`cumulative_seconds`, set `heartbeat_epoch = epoch()` when time is valid,
`seq++`, write the older slot.

Write cost is one 48-byte FRAM write per heartbeat. Negligible against the
endurance budget, and unlike the microSD log path it is not an i2c round trip.

## Getting it off the device

Three options, in increasing order of cost and cleanliness.

### Option C, free today

The microSD boot banner above. Works with no protocol change at all. It is a
debugging aid rather than monitoring, because reading it means physically
retrieving the card, but it costs nothing and should go in regardless.

### Option A, recommended for the dashboard

Add device counters to `SensorType` in `sensor.proto`:

```protobuf
DEVICE_UPTIME = 20;              /* session seconds */
DEVICE_CUMULATIVE_UPTIME = 21;
DEVICE_BOOT_COUNT = 22;
DEVICE_UNCLEAN_BOOTS = 23;
DEVICE_DOWNTIME = 24;
```

These encode as ordinary `SensorMeasurement` with the `unsigned_int` arm, which
means they ride the machinery that already exists: `fifo_put()`, `get_payload()`,
`lorawan_upload()`, the existing backend ingest, and the generic `/sensor/`
endpoint described above. No new transport, no new decode path, no new dirtviz
client method, and they arrive on the same time axis as the sensor data, so
uptime plots directly against the measurements it explains.

Cost is the reason to be selective. `get_payload()` builds into `uint8_t
buffer[60]` at `main.cc:199`, and a `SensorMeasurement` with metadata runs
roughly 15 to 25 bytes encoded. Sending all five counters every batch would
crowd out real measurements. Send `DEVICE_BOOT_COUNT` and `DEVICE_UNCLEAN_BOOTS`
only when they change, and the uptime counters once every N batches.

The honest objection: uptime is not a sensor reading, and this overloads
`SensorType` with something that is not one. That is a deliberate trade for
landing the whole feature in one PR against one repo instead of three.

### Option B, if John wants it clean

A dedicated `DeviceStatus` message with its own uplink path. Semantically right,
and it costs new encode and decode in libents, a new branch in the uplink, and
backend ingest work.

## Verified on hardware

`embedded/stm32/examples/uptime_probe` measures the facts this design rests on.
Built with the ARM toolchain on the HARE lab server, installed alongside the
existing `sdi12` app on the bench stm32wle5jc, and run across repeated openocd
resets.

Build: clean from scratch with `make CFLAGS=-Werror`, zero warnings and zero
errors across all six target architectures, producing
`org.ents.examples.uptime_probe.tab`.

| Question | Assumed | Measured |
|---|---|---|
| Alarm frequency | 32768 Hz | **16000 Hz** |
| Tick wrap period | ~36 h | **74 h 33 m 55 s** |
| FRAM size | 131072 B | 131072 B, confirmed |
| FRAM r/w at 0x1F200 | assumed to work | **PASS**, byte exact readback |
| Record fits above the FIFO | assumed | yes, 3808 B spare |
| RTC across reset | "yes if VBAT held" | **no**, reads 946684800 |

The boot record survives real resets, and the A/B ping-pong alternates as
intended:

```
loaded from slot A, seq 3   previous boot_count: 3   this boot is 4   stored to B
loaded from slot B, seq 4   previous boot_count: 4   this boot is 5   stored to A
```

`unclean_boots` tracked 2 then 3, correctly, because the probe never sets the
clean shutdown flag and every openocd reset therefore looks like a brownout.
That is the detection path working end to end on real silicon.

Tick accumulation over five samples two seconds apart:

```
delta 32047, 32271, 32271, 32270, 32272 ticks
accumulated 161131 ticks = 10070 ms over ~10000 ms of requested delay
```

At 16000 Hz a 2 s delay is 32000 ticks, so the ~271 tick excess is the printf and
syscall time between samples. Worth noting because it confirms the accumulator is
measuring real elapsed time rather than just adding up the delays it asked for,
which is exactly what an uptime counter has to do.

### Both shutdown branches, and downtime, on hardware

The runs above only ever showed `previous shutdown: UNCLEAN`, which was trivially
true because nothing ever set the clean flag. Proving the metric is worth
anything needs the other branch too, plus a downtime figure that is not `n/a`.

The probe therefore walks a three phase state machine held in FRAM at 0x1F300,
advancing one step per reset, using `set_epoch()` from `libents/util/time.h` as a
stand in for `lorawan_timesync()`. That makes both branches testable on a bench
board with no network and no VBAT.

```
phase 0   simulate timesync, store the epoch, exit CLEAN
phase 1   check clean was seen; then measure downtime across the reset
phase 2   check unclean was seen
```

Result over five resets, cycling the sequence twice: **13 checks passed, 0
failed**, and the FRAM scratch test passed on all five boots.

```
-- self test, phase 1 --
  [PASS] previous shutdown was clean: got 1, want 1
  [PASS] previous epoch survived: got 1767225600, want 1767225600
  [PASS] previous time_valid survived: got 1, want 1
  RTC after reset reads 946684800 (expected to be stale)
  [PASS] clock was indeed stale at boot: got 0, want 0
  simulated timesync -> epoch() now 1767232800
  measured downtime: 7200 s (2 h 0 m 0 s)
  [PASS] downtime matches the simulated outage: got 7200, want 7200
```

That trace is the corrected design working end to end on silicon. The RTC is
wiped by the reset, FRAM carries the previous epoch across it, the boot time
clock is correctly rejected as stale, and the subtraction happens only once
timesync has produced a trustworthy clock. Both shutdown branches were exercised
twice, so neither is a fluke.

Not yet exercised: **a true power cycle rather than a debug reset.** Every result
here comes from an openocd `reset run`, which is not the same event as the
brownout the record is built for. It leaves RAM and peripheral state differently
than a real power loss would, and a torn FRAM write mid brownout has only been
simulated on the host, never provoked on hardware. This is the one gap that needs
someone at the bench, or a switchable supply.

## Implementation status

Built and verified, not yet reviewed or merged. Everything below was
re-verified on the HARE lab server on 2026-08-25.

| Piece | State |
|---|---|
| `libents/util/uptime.{h,c}` | done, 68/68 host unit tests, 33/33 on hardware |
| `apps/core/main.cc` integration | done, builds clean with `-Werror` |
| `sensor.proto` device counters | done, protos regenerated |
| Counter emission into the uplink | done, compiles, **not yet seen on a real uplink** |
| `ents.proto` counter metadata | done, unblocks the whole ingest path |
| Server side gap analysis | done, 27/27 unit tests, run against the live API |
| `ents` package release | **required**, dirtviz rejects the counters until it ships |
| Counters seen end to end in dirtviz | not yet, needs a flashed node plus the release |

Verification:

- Host unit tests in `embedded/libents/test/uptime` link the real `uptime.c`
  against stubbed FRAM, RTC and alarm. `./run.sh` builds with
  `-Wall -Wextra -Werror` and runs 68 checks covering boot counting, clean and
  unclean shutdown, downtime, tick wrap, sub second accumulation, the persist
  interval, ping pong alternation, CRC rejection of a corrupt slot, torn
  writes, both slots unreadable, a clock stepping backwards, driver failures,
  and the pre-init guards. All pass.
- `examples/uptime_lib` drives the shipping API on real hardware across resets.
  33 checks, 0 failures. Confirms the record survives real resets, `boot_count`
  and `unclean_boots` advance, `cumulative_seconds` carries over, and a two hour
  outage is measured as exactly 7200 s.
- `make CFLAGS=-Werror` clean on all six architectures for both the core app
  and `uptime_lib`, `clang-format -n -Werror` clean on every new and changed
  file.
- `python -m unittest tests.test_dirtviz_uptime`, 27 checks, 0 failures.

An earlier version of this document claimed 35 host checks. The harness that
produced that number was never committed and no longer exists anywhere; the 68
above come from its replacement, which is committed at
`embedded/libents/test/uptime`. The record is also 44 bytes, not the 48 this
document previously stated.

Two things the tests caught that are worth recording, because both would have
been invisible until deployment:

**One flag was doing two jobs.** `init()` cleared `FLAG_TIME_VALID` on the
grounds that the RTC is untrustworthy at boot. But that same flag was also the
only evidence that the *previous* boot's `heartbeat_epoch` meant anything, so
clearing it made downtime permanently unmeasurable while every other counter
kept working. Now split: `FLAG_HEARTBEAT_VALID` persists in the record and
describes the stored epoch, while `s_time_synced` is RAM only and describes the
current session.

**`uptime_probe` and the library share FRAM addresses.** The probe was written to
validate the exact layout at 0x1F000 / 0x1F100, so it writes its own
incompatible record to the same slots. With both apps installed the library saw
its record clobbered every boot and reported `boot_count=1` forever. Not a
product bug, since only one of them ships, but the two must never be flashed
together. The probe has been uninstalled from the bench board.

## Also available for free

`ModulePower` on the esp32 already keeps `boot_count` in `RTC_DATA_ATTR`
(`power.cpp:12`) and already reports it plus a wakeup reason in the `WAKEUP`
response (`power.cpp:76`). `ControllerPowerWakeup()` then discards the whole
response, with `power.c:64` noting exactly that. Plumbing those two values out
gives esp32-side reset visibility with no protocol change whatsoever.

One nit on that path: `power.c:35` copies from `cmd.command.wifi_command` when it
means `power_command`. It currently works only because both are members of the
same union and therefore alias to the same address. It is not a live bug, but it
is wrong as written and will break the moment anyone reorders that union.

## Suggested order

1. ~~Server-side gap analysis through dirtviz.~~ Done. Lives at
   `python/src/ents/dirtviz/uptime.py`, 27 unit tests, run against the live API
   on 2026-08-25.
2. Option C boot banner plus the esp32 counters. Both are nearly free.
3. FRAM record with heartbeat, and Option A counters on the uplink. Blocked on
   the generic endpoint question above: the counters are built, but there is no
   evidence yet that anything would be able to read them.

Steps 1 and 2 answer "is this node alive" quickly. Step 3 is what answers
"and why did it stop."

One property of step 1 to keep in mind when reading its numbers: a gap is
measured from the last reading before it to the first reading after, so it
overstates downtime by up to one upload interval at each edge. Nothing
observable pins down where inside those intervals the device really stopped.
Reported downtime is an upper bound and availability a lower bound, which is the
safe direction for alerting but means the figures should not be quoted as exact.
The on-device record in step 3 is what tightens them, since it knows its own boot
time.

## Open questions

Answered by the probe:

- ~~Alarm frequency on the deployed board.~~ 16000 Hz on the bench unit, giving
  74 h of wrap headroom. Heartbeat cadence is not a constraint.
- ~~Is VBAT actually held?~~ Not on the bench unit. The RTC resets to
  946684800 on every reset. Downtime is still measurable, but only after
  timesync on the new boot, not during the boot sequence.

Answered by probing the live API:

- ~~Does the generic `/sensor/` endpoint exist and accept arbitrary names?~~ It
  exists and accepts anything, including names that are not sensors, always
  returning HTTP 200. That is less reassuring than it sounds, see below.
- ~~Is there a logger-keyed endpoint at all?~~ Yes. `/logger/` returns 401, not
  404. It is real, authenticated, and missing from the python client.

Still open:

- Do the **field** boards hold VBAT, or do they behave like the bench unit? This
  changes nothing structurally, since the deferred subtraction works either way,
  but it decides whether a node that cannot reach the network can ever report how
  long it was down.
- ~~Does anything at all reach the generic `sensor` table?~~ Answered by
  reading `jlab-sensing/dirtviz`: yes, and it self-registers new sensors. The
  real blocker was five missing entries in this repo's own `SENSOR_DATA`, now
  added. What remains is releasing the `ents` package so dirtviz picks them up.
- What does `/logger/` serve, and can the python client get a token for it?
  Uptime is a property of the logger, and filing it under a cell id means one
  dead logger reads as several independent dead cells.
- Does the LoRaWAN payload budget have room for counters, or should they go only
  over the WiFi path where the budget is much larger?
- Is a debug reset a fair stand-in for a brownout? Every hardware result so far
  comes from an openocd `reset run`. A real power cycle is the case the record is
  built for, and it needs someone at the bench or a switchable supply.
