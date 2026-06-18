# ENTS-tock
Userspace Implementation of ENTS on the tock operating system.


## Specify ignore formatting

You can list files to be ignored by `clang-format` using `.clang-format-ignore`. This file is primarily used for files that were copied from another repository where you want to keep the original formatting. Example from `embedded/stm32/tests/` for the Unity test framework. The `clang-format` tool will search through parent directories so they can be placed anywhere in the diretory structure.

```
unity/*
```
