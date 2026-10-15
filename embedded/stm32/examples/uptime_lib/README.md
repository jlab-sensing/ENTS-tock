# uptime_lib

Exercises the shipping `libents/util/uptime` API on real hardware, across
repeated resets.

Install it, then reset the board several times and watch the output. Each boot
runs a self test against what the previous boot stored, so the interesting
result only appears from the second boot onwards.

```bash
make install
tockloader listen
```

It walks a small state machine held in FRAM, advancing one step per reset, and
uses `set_epoch()` as a stand in for `lorawan_timesync()` so both the clean and
unclean shutdown branches and a measured outage can be checked on a bench board
with no network.

Confirms across resets that the record survives, `boot_count` and
`unclean_boots` advance, `cumulative_seconds` carries over, and a simulated two
hour outage is measured as exactly 7200 s.

For the cases hardware cannot reach in reasonable time, a tick counter about to
wrap or a torn FRAM write, see the host tests in `libents/test/uptime`.

> [!WARNING]
> Do not install this alongside `uptime_probe`. Both write the same FRAM slots.
