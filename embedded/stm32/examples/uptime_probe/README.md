# uptime_probe

A throwaway diagnostic, kept because it is what established the hardware facts
the uptime design rests on. It is not part of the product and nothing depends
on it.

> [!CAUTION]
> This writes its own incompatible record to the same FRAM addresses the
> shipping library uses, `0x1F000` and `0x1F100`. **Never install it on a board
> running the core app or `uptime_lib`.** With both installed the library sees
> its record clobbered every boot and reports `boot_count=1` forever. Erase
> apps before and after using it.

What it measures, and what it found on the bench stm32wle5jc:

| Question | Assumed | Measured |
|---|---|---|
| Alarm frequency | 32768 Hz | **16000 Hz** |
| Tick wrap period | ~36 h | **74 h 33 m 55 s** |
| FRAM size | 131072 B | 131072 B, confirmed |
| FRAM read/write high in the part | assumed to work | byte exact readback |
| RTC survives a reset | "yes if VBAT held" | **no**, reads 946684800 |

The last row is the one that shaped the design: VBAT is not held on that unit,
so the RTC is useless at boot and downtime can only be measured after a
timesync on the new boot.

```bash
make install
tockloader listen
```

See UPTIME_TRACKING_DESIGN.md at the repo root for what was concluded from it.
