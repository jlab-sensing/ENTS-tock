# ents

Python package for the Environmental NeTworked Sensor (ENTS) project. It
encodes and decodes the protobuf messages nodes upload, talks to the
[DirtViz](https://dirtviz.jlab.ucsc.edu/) backend, configures nodes over
serial, and simulates a node when you do not have hardware to hand.

The firmware that runs on the nodes themselves is documented separately, with
Doxygen, in `embedded/doxygen`.

## Where to start

Split by what you are trying to do:

**[Using the package](usage.md)** — you want to pull data out of DirtViz,
decode a payload, check whether a node is alive, or configure one. Start here.

**[Developing the package](development.md)** — you want to change the package
itself: environment setup, tests, linting, and regenerating the protobuf
bindings after editing a `.proto` file.

**[API reference](api.md)** — generated from the docstrings.

## Install

```bash
pip install ents
```

See [Using the package](usage.md) for the rest.

```{toctree}
:hidden:
:maxdepth: 2

usage
development
api
```
