# Using the package

For people consuming ENTS data or working with a node. If you are changing the
package itself, see [Developing the package](development.md).

## Install

```bash
pip install ents
```

Python 3.11 or newer.

## Getting data out of DirtViz

`BackendClient` wraps the DirtViz API. Data is keyed by *cell*, and a cell is
looked up by name or id.

```python
from datetime import datetime, timedelta
from ents.dirtviz import BackendClient

client = BackendClient()
cell = client.cell_from_name("Field Cell 3")

end = datetime.now()
start = end - timedelta(days=7)

power = client.power_data(cell, start, end)
teros = client.teros_data(cell, start, end)

print(power.head())
```

Every method returns a pandas `DataFrame` with a `timestamp` column.

`cells()` lists everything available, and `cell_from_id()` is the counterpart to
`cell_from_name()`.

### Two ingest paths

A node's data lands in one of two places depending on which fPort its firmware
uploads on, and they are served by different endpoints:

| fPort | Stored in | Read with |
|---|---|---|
| 1 | dedicated power / teros tables | `power_data()`, `teros_data()` |
| 2 | generic sensor table | `sensor_data()` |

Current firmware uses fPort 2. Older nodes are still on fPort 1, so a query
across the whole fleet has to try both. A node that returns nothing from
`power_data()` is not necessarily dead; it may simply be on the other path.

`sensor_data()` is named by the `SensorType` enum name plus the human readable
measurement name from `SENSOR_DATA`, not by a short code:

```python
client.sensor_data(cell, "POWER_VOLTAGE", "Voltage", start, end)
```

`("power", "v")` returns nothing at all, which is the trap.

:::{warning}
The endpoint returns an identical empty response for a sensor that does not
exist and one that simply has no data in the window. A typo is indistinguishable
from a dead node.

`GET /cell/<id>/sensors` lists what a cell actually has registered, and is the
only reliable way to find the right names:

```console
$ curl https://dirtviz.jlab.ucsc.edu/api/cell/1483/sensors
[{"name": "POWER_VOLTAGE", "measurement": "Voltage", "unit": "mV"}, ...]
```
:::

### Resampling

The API aggregates into hourly buckets by default. That matters whenever you
care about *when* readings happened rather than their values: against hourly
buckets every node appears to report once an hour no matter what it really
does, and anything shorter than a multi-hour gap is invisible. Raw intervals on
the current deployment range from 14 seconds to 5 minutes.

`sensor_data()` takes a `resample` argument for this. Pass `"none"` for raw
measurements.

## Plotting

```python
from ents.dirtviz import plot_data
```

Takes the DataFrames above and renders them with matplotlib.

## Encoding and decoding measurements

```python
from ents.proto import decode_measurement

meas = decode_measurement(payload_bytes)
```

For the newer repeated-measurement format used on fPort 2:

```python
from ents.proto.sensor import parse_sensor_measurement

meas = parse_sensor_measurement(payload_bytes)
for m in meas["measurements"]:
    print(m["type"], m["name"], m["unit"])
```

`parse_sensor_measurement()` is also what the DirtViz backend calls on every
uplink, so a `SensorType` it cannot resolve rejects the whole batch. See
[Developing the package](development.md#adding-a-sensortype) before adding one.

## Configuring a node

```bash
ents --help
```

The CLI covers encoding test payloads, calibration against a reference
instrument, and writing a node's user configuration over serial.

## Simulating a node

Useful when you want data flowing without hardware:

```python
from ents.simulator import NodeSimulator
```

`tools/http_decoder.py` in the repository runs a small webserver that decodes
uploads, which pairs well with the simulator.
