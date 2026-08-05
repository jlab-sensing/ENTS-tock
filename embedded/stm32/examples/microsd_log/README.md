# microsd_log

Writes a microlog "hello world" from the stm32 into the microSD card attached
to the esp32.

## Path the message takes

```
ulog_info("hello world")            stm32, microlog
  -> microsd_log_output()           microlog output handler (this app)
  -> ControllerMicroSDLog()         libents, encodes a MicroSDCommand(LOG)
  -> ControllerTransaction()        libents, 32 byte chunked i2c to addr 0x20
  -> ModuleHandler::OnReceive()     esp32, reassembles and decodes
  -> ModuleMicroSD::WriteLog()      esp32, appends to /stm32.log
```

## Build and install

```
make
tockloader install
```

## Verifying

The esp32 firmware logs at `LOG_LEVEL_TRACE`, so watch its serial output while
the app runs:

```
pio device monitor -b 115200
```

Expect `ModuleMicroSD::WriteLog` followed by `Wrote to and closed '/stm32.log'`.
That confirms i2c and the SD write separately from having to pull the card.

## Notes

- The output is registered at `ULOG_LEVEL_INFO`. Every forwarded line costs a
  full i2c transaction plus an SD mount, so do not point this at
  `ULOG_LEVEL_TRACE` in a loop.
- Requires `ULOG_BUILD_EXTRA_OUTPUTS` to be set in
  `embedded/external/microlog/Makefile`, otherwise `ulog_output_add()` is a
  stub that returns `ULOG_OUTPUT_INVALID`.
- Lines are truncated to 127 characters by `MicroSDCommand.log`.
