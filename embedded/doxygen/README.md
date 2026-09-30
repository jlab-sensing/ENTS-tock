# Source code documentation with doxygen

API reference for the embedded sources, generated from the `@brief` / `@param`
comments already in the headers. Covers the stm32 side (libents and the Tock
applications) and the esp32 side, and pulls in every README in the repository
so they are browsable in one place.

For the python package see [`python/`](../../python/README.md), which is
documented with Sphinx instead.

## Build it

```bash
cd embedded/doxygen
./build.sh
```

Output lands in `build/html/index.html`. `./build.sh --open` opens it.

`build/` is gitignored; this is generated on demand and never committed.

### Installing doxygen

| | |
|---|---|
| macOS | `brew install doxygen` |
| Ubuntu | `sudo apt install doxygen` |
| Windows | `choco install doxygen.install` or `scoop install doxygen` |

Written against doxygen 1.17. Optional: install Graphviz and set
`HAVE_DOT = YES` in the `Doxyfile` for call graphs, at the cost of a slower
build.

## Warnings

The build writes `build/doxygen-warnings.log` and prints it. With `CI=true` a
warning fails the build, matching the other scripts in this repo:

```bash
CI=true ./build.sh
```

Not every warning is a doxygen problem. Most are real mistakes in the comments
themselves, and the build is how you find them:

- a `@param` naming an argument the function does not have, usually left behind
  when a signature changed
- `@retrun` and similar typos, which doxygen reports as an unknown command
- `@ref` pointing at a page or file that does not exist

## How this is organised

Doxygen groups, not directories, decide the navigation. The top level is:

- `stm32` — the Tock applications and libents, defined in
  [`../stm32/stm32.dox`](../stm32/stm32.dox)
- `esp32` — the coprocessor firmware, defined in
  [`../esp32/esp32.dox`](../esp32/esp32.dox)

Everything else hangs off one of those with `@ingroup`. A new module should
declare its own group and attach it:

```c
/**
 * @ingroup stm32
 * @defgroup mymodule My Module
 * @brief One line describing it
 * @{
 */

/* ... declarations ... */

/** @} */
```

If you add `@ingroup something` without a matching `@defgroup something`
anywhere, the group silently does not appear. Both `stm32` and `util` were in
that state before this build existed. To check:

```bash
grep -rho "@defgroup \S*" --include=*.h --include=*.dox .. | sort -u
grep -rho "@ingroup \S*"  --include=*.h --include=*.dox .. | sort -u
```

Anything in the second list but not the first is dangling.

## What gets scanned

`INPUT` in the `Doxyfile`, which is the libents sources, the esp32 libraries,
the stm32 apps and examples, and the markdown READMEs. Vendored and generated
code is excluded: `libtock-c`, `external/`, nanopb output (`*.pb.c`, `*.pb.h`)
and the Unity test framework. None of it is ours to document and the generated
protobuf in particular would swamp everything else.

`EXTRACT_ALL` is off, so only entities with a doc comment appear. If something
you expect is missing, either it has no comment or its group is dangling. Set
`EXTRACT_ALL = YES` temporarily to confirm which.

One doxygen default worth knowing about: since 1.15, `IMPLICIT_DIR_DOCS`
defaults to `YES`, which turns every `README.md` into the documentation *of its
directory* rather than a page of its own. That removes them from the Pages tab
and breaks any `@subpage` pointing at one. It is set to `NO` here.
