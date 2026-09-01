# Uptime host unit tests

Exercises `libents/util/uptime.c` on a workstation, with FRAM, the RTC and the
Tock alarm replaced by stubs the test drives directly.

```
./run.sh
```

The hardware test app in `embedded/stm32/examples/uptime_lib` covers the same
library on a real board. This exists for the cases hardware cannot reach in a
reasonable time or at all:

- a tick counter a few ticks short of its 32 bit rollover
- a slot whose CRC fails
- a write torn in half by a brownout
- both slots unreadable
- FRAM or the alarm driver failing outright
- a timesync that steps the clock backwards

A "reboot" in these tests is another call to `ents_uptime_init()`. That is
faithful, because everything the library carries across a reset lives in the
FRAM array and `init()` resets the rest.

`stubs/` shadows two headers so `uptime.c` compiles off-target unchanged:
`libtock/peripherals/syscalls/alarm_syscalls.h` and
`libtock-sync/peripherals/rtc.h`.
