# ENTS-tock
Userspace Implementation of ENTS on the tock operating system.


## Building

Any of the build shell script can be run with CI options with `CI=true` environment variable. For example the following will show formatting errors without automatically fixing them.

`CI=true ./format.sh`

## Specify ignore formatting

You can list files to be ignored by `clang-format` using `.clang-format-ignore`. This file is primarily used for files that were copied from another repository where you want to keep the original formatting. Example from `embedded/stm32/tests/` for the Unity test framework. The `clang-format` tool will search through parent directories so they can be placed anywhere in the diretory structure.

```
unity/*
```

## FAQ

The following means there is likely something wrong with the include paths to the nanopb library.

```
../../../libtock-c/../libents/src/libents/controller/modules/../../proto/user_config.pb.h:9:2: error: #error Regenerate this file with the current version of nanopb generator.
    9 | #error Regenerate this file with the current version of nanopb generator.
      |  ^~~~~
```
