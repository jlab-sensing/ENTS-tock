# microlog to microSD: summary of changes

Branch `microlog-to-microsd`, commit `dabdaab`, branched from `main` at `0218eee`.

Routes stm32 `ulog_*` output over i2c to the esp32, which appends it to
`/stm32.log` on the microSD card. Before this, `MicroSDCommand` could only carry
a `Measurement` (`SAVE`) or a `UserConfiguration` (`USERCONFIG`), so there was no
way to send arbitrary text to the card.

Origin: the jLab task "from the stm32 get a helloworld to write into esp".

## Status

**Partially verified. The firmware has never been built.**

What has been proven, by compiling and running on the host with plain `gcc`:

- The proto change round trips. A `MicroSDCommand` of type `LOG` encodes to 15
  bytes and decodes back with `log == "hello world"`, `type == 2`,
  `which_data == 6`.
- `MicroSDCommand_size` is 503 and `Esp32Command_size` is 632, both unchanged
  from `main`. The new oneof arm genuinely costs no RAM.
- `ulog_output_add()` returns a valid id when microlog is compiled with
  `ULOG_BUILD_EXTRA_OUTPUTS=1`, and returns `ULOG_OUTPUT_INVALID` without it.
  The handler fires once per `ulog_info()` and `ulog_event_to_cstr()` fills the
  buffer as expected.

- `embedded/libents/src/libents/controller/modules/microsd.c` and
  `embedded/stm32/examples/microsd_log/main.c` both pass `gcc -fsyntax-only
  -Wall -Wextra` against the real `libtock-c`, nanopb and microlog headers, with
  zero warnings in the new code. The one warning emitted is a pre-existing `%lu`
  vs `uint32_t` format mismatch in `ControllerMicroSDUserConfig()` at
  `microsd.c:94`, which is a host artifact: `uint32_t` is `unsigned long` on
  arm-none-eabi but `unsigned int` on x86_64, so `%lu` is correct on target.

What has **not** been built at all:

- The real ARM build. `-fsyntax-only` type checks but does not codegen, link, or
  run the Tock build system. There is no `arm-none-eabi-gcc`, no `make` and no
  `tockloader` on this machine.
- The entire esp32 side. `WriteLog()` needs the Arduino/ESP32 toolchain, `SD.h`
  and `ArduinoLog`, none of which were available.
- Real i2c and a real SD card.

`format.sh` has also not been run, because `clang-format` was unavailable. Run it
before opening a PR.

## Path a log line takes

```
ulog_info("hello world")            stm32, microlog
  -> microsd_log_output()           microlog output handler (example app)
  -> ControllerMicroSDLog()         libents, encodes a MicroSDCommand(LOG)
  -> ControllerTransaction()        libents, 32 byte chunked i2c to addr 0x20
  -> ModuleHandler::OnReceive()     esp32, reassembles and decodes
  -> ModuleMicroSD::WriteLog()      esp32, appends to /stm32.log
```

## Files changed

13 files, +281 / -28.

### Protocol

| File | Change |
| --- | --- |
| `proto/controller.proto` | Adds `MicroSDCommand.Type.LOG = 2` and a `string log = 6` arm to the `data` oneof. |
| `proto/controller.options` | Bounds `MicroSDCommand.log` to `max_length:127`. |

Because `data` is a oneof and the new arm is smaller than the existing
`Measurement` arm, `MicroSDCommand_size` (503) and `Esp32Command_size` (632) are
both unchanged. The new field therefore costs no additional RAM.

### Build

| File | Change |
| --- | --- |
| `embedded/external/microlog/Makefile` | Adds `-DULOG_BUILD_EXTRA_OUTPUTS=1`. |

Without this define, `ulog_output_add()` is compiled out entirely and returns
`ULOG_OUTPUT_INVALID` at runtime. The guards live in `ulog.c` rather than
`ulog.h`, so only the library Makefile needs the flag. One extra output slot is
enough for the microSD sink.

### stm32 side, libents

| File | Change |
| --- | --- |
| `embedded/libents/src/libents/controller/modules/microsd.h` | Declares `ControllerMicroSDLog()`. |
| `embedded/libents/src/libents/controller/modules/microsd.c` | Implements it, mirroring `ControllerMicroSDSave()`. |

Details worth knowing:

- The payload is not a `Measurement`, so unlike `ControllerMicroSDSave()` there
  is no need to send a `USERCONFIG` first to open a data file.
- `strncpy` plus an explicit NUL terminator means over long lines truncate
  instead of overflowing the fixed size array.
- It returns `MicroSDCommand_ReturnCode_ERROR_GENERAL` on i2c failure rather
  than `0` like its neighbours, because `0` is `SUCCESS` in that enum.

### esp32 side

| File | Change |
| --- | --- |
| `embedded/esp32/lib/module_handler/include/modules/microsd.hpp` | Declares the private `WriteLog()`. |
| `embedded/esp32/lib/module_handler/src/modules/microsd.cpp` | Handles `MicroSDCommand_Type_LOG` in the `OnReceive` switch and implements `WriteLog()`. |

Details worth knowing:

- Named `WriteLog()` and not `Log()` to avoid shadowing the global ArduinoLog
  instance named `Log`.
- Appends to a hardcoded `/stm32.log` via `SD.open(..., FILE_APPEND)`. The fixed
  filename is deliberate: it means logging never depends on a prior `USERCONFIG`
  command having populated `dataFileFilename`.
- Reuses the existing error handling shape, returning
  `ERROR_MICROSD_NOT_INSERTED`, `ERROR_FILE_SYSTEM_NOT_MOUNTABLE` or
  `ERROR_FILE_NOT_OPENED` as appropriate.

### New example

| File | Change |
| --- | --- |
| `embedded/stm32/examples/microsd_log/main.c` | Registers a microlog output handler that forwards each line to the card. |
| `embedded/stm32/examples/microsd_log/Makefile` | Standard Tock app Makefile. |
| `embedded/stm32/examples/microsd_log/README.md` | Build, install and verification steps for the example. |

Two non obvious things in `main.c`:

- `ControllerInit()` runs before `ulog_output_add()`. It allocates the tx and rx
  buffers, so registering the output first would make the very first forwarded
  line encode into a NULL buffer.
- The handler holds a `static bool busy` recursion guard. Anything reached from
  `ControllerMicroSDLog()` that itself logs would re-enter the handler and
  recurse forever.

The stdout output is never removed, so lines still reach the Tock console as well
as the card.

### Regenerated

| File | Change |
| --- | --- |
| `embedded/libents/src/libents/proto/controller.pb.c` | nanopb output. |
| `embedded/libents/src/libents/proto/controller.pb.h` | nanopb output. |
| `python/src/ents/proto/controller_pb2.py` | protoc output. |

## Open items

1. **Compile it.** See Status above. Nothing here is verified against a
   toolchain.
2. **Run `format.sh`.** Never run, no `clang-format` available.
3. **nanopb regeneration churn, worth raising with John Madden**
   (jtmadden@ucsc.edu). On `main` the checked in `.pb.*` files were generated by
   nanopb 0.4.9.1 while the pinned submodule is 1.0.0-dev. This branch
   regenerated them, so they now match the submodule, but the diff carries
   unrelated churn: the banner comment flips to `nanopb-1.0.0-dev` and the
   previously untagged unions gain type tags (`union {` becomes
   `union _MicroSDCommand_data {`). Both versions declare
   `PB_PROTO_HEADER_VERSION 40`, so they are compatible with the runtime `pb.h`
   either way. Decide with John whether to keep the regeneration in this PR or
   split it out.
4. **Prefix truncation can eat the whole message. Design question, not a typo.**
   `ulog_event_to_cstr()` writes `LEVEL  FILE:LINE: MESSAGE` into the buffer, so
   the 128 bytes are shared between the prefix and the message. The prefix grows
   with whatever `__FILE__` expands to under the Tock build. Observed on the
   host with an absolute path:

   ```
   captured: 'INFO  C:/Users/Azam Mohamed/AppData/.../scratchpad/tes'
   ```

   The actual message, `"hello world"`, was truncated away completely and the
   line that would have reached the card carried only a file path. The same test
   passes when `__FILE__` is a short relative path. So whether this bites
   depends entirely on how the build invokes the compiler, and it fails
   silently: nothing returns an error, the card just gets a useless line.

   Options: log `ulog_event_get_message()` instead of the full formatted line,
   raise `MicroSDCommand.log` above 127, or shorten the prefix. Worth deciding
   before the PR.

5. **Cost per line.** Every forwarded line is a full i2c transaction plus an SD
   mount and unmount. This is fine for occasional `ULOG_LEVEL_INFO` messages and
   will not keep up with `ULOG_LEVEL_TRACE` in a loop. If that becomes a
   requirement, the write path needs buffering and the mount needs to be held
   open.

## Reproducing the generated files

There is no `protoc` or `nanopb_generator` on PATH. To regenerate:

```
python -m venv .venv
.venv/Scripts/pip install protobuf grpcio-tools
python embedded/external/nanopb/nanopb/generator/nanopb_generator.py
```

The `embedded/external/nanopb/nanopb` submodule must be checked out first.
