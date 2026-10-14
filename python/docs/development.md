# Developing the package

For changing the `ents` package itself. If you only want to use it, see
[Using the package](usage.md).

## Setup

From the repository root:

```bash
cd python
python -m venv .venv
source .venv/bin/activate      # Windows: .venv\Scripts\activate
pip install -e .[dev]
```

Editable install, so changes take effect without reinstalling.

## Running things locally

All three scripts honour `CI=true`, which makes them report problems instead of
fixing them, exactly as CI runs them.

```bash
./test.sh                  # python -m unittest
./lint.sh                  # ruff check --fix   (CI=true: check only)
./format.sh                # ruff format        (CI=true: --check only)
```

A single test file, while iterating:

```bash
python -m unittest tests.test_sensor -v
```

:::{important}
Match CI's ruff version. `pip install -e .[dev]` is unpinned, so a newer ruff
can report findings CI does not, or miss ones it does. If `./lint.sh` passes
locally and CI disagrees, check the version in the CI log first.
:::

## Building these docs

```bash
cd python/docs
./build.sh            # ./build.sh --open to open it afterwards
```

Output in `docs/build/html/index.html`. Requires the docs extra:

```bash
pip install -e .[docs]
```

Pages are Markdown via MyST. The API reference is generated from docstrings, so
document the code, not the reference page.

Docstrings are **Google style**, which is what `sphinx.ext.napoleon` is
configured for:

```python
def example(cell, start):
    """One line summary.

    Longer explanation if it earns its place.

    Args:
        cell: What it is.
        start: What it is.

    Returns:
        What comes back.

    Raises:
        ValueError: When.
    """
```

## Adding a SensorType

Adding a value to the `SensorType` enum takes three steps, and skipping the
third breaks data collection in a way nothing warns you about.

1. Add it to `proto/sensor.proto`.

2. Regenerate the bindings:

   ```bash
   cd proto
   make            # or: make c / make python
   ```

   This writes both the C sources under `embedded/libents/` and
   `sensor_pb2.py` here. Commit the generated files; they are checked in.

3. **Add an entry to `SENSOR_DATA` in `src/ents/proto/sensor.py`.**

Step 3 is not optional. `get_sensor_data()` looks the type up with a bare dict
access, and DirtViz calls `parse_sensor_measurement()` from this package on
every uplink. A missing entry raises `KeyError`, which the backend turns into
an HTTP 400 for the *entire batch* — discarding every real measurement that
happened to be uploaded alongside it, on every upload, with nothing on the node
to indicate anything is wrong.

Nothing currently fails the test suite when an entry is missing, so this is
worth checking by hand until something does.

The two names matter downstream: DirtViz files a measurement under the enum
name (`POWER_VOLTAGE`) with the `name` field as its measurement
(`Voltage`), and that pair is what `sensor_data()` queries by.

## Regenerating protobuf without a system protoc

The `proto/Makefile` expects `protoc` and `nanopb_generator` on PATH. If you
only need the python side:

```bash
pip install grpcio-tools
python -m grpc_tools.protoc -I proto --python_out=proto/build proto/sensor.proto
cp proto/build/sensor_pb2.py python/src/ents/proto/
```

Check the generated header comment says a protobuf version compatible with the
pin in `pyproject.toml`; generated code newer than the runtime fails at import.

## Releasing

`pyproject.toml` holds the version. Bumping it matters beyond PyPI: DirtViz
imports `parse_sensor_measurement` from this package, so a decoding change only
reaches production once a release ships and the backend picks it up.
