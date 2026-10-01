"""Availability of deployed nodes, derived from when their data arrived.

Uploads are paced by ``UserConfiguration.Upload_interval``, so a node that
stops reporting leaves a gap in its own time series. That is enough to answer
"was this node alive", with no firmware change and no new endpoint, and it
works retroactively on data already in dirtviz.

What this cannot tell you is *why* a node went quiet. A gap looks identical
whether the node was powered off, the radio failed, or the backend dropped the
upload, and those have opposite fixes. Separating them is what the on-device
record in ``libents/util/uptime.h`` is for. Use this for "which nodes are
down", and the device counters for "and why".

One property worth repeating whenever these numbers are quoted: a gap is
measured from the last reading before it to the first reading after, and
nothing observable pins down where inside those two intervals the node really
stopped and restarted. **Reported downtime is an upper bound and availability
is a lower bound.** That is the safe direction for alerting, but the figures
are not exact and should not be presented as though they were.

Example:

    from datetime import datetime, timedelta, timezone
    from ents.dirtviz import BackendClient
    from ents.dirtviz.uptime import cell_availability

    client = BackendClient()
    cell = client.cell_from_name("nu_local_ENTS_1")
    end = datetime.now(timezone.utc)
    report = cell_availability(client, cell, end - timedelta(days=7), end)
    print(report.summary())

Or from the command line, over every cell that reported recently::

    python -m ents.dirtviz.uptime --days 7
"""

from dataclasses import dataclass, field
from datetime import UTC, datetime, timedelta
from itertools import pairwise
from statistics import median

__all__ = [
    "Availability",
    "Gap",
    "availability_from_timestamps",
    "cell_availability",
    "find_gaps",
    "infer_interval",
]

#: A gap counts as an outage once it exceeds the upload interval by this
#: factor. One missed upload is routine, since uploads jitter and a single
#: retry covers it; a run of them is not.
DEFAULT_TOLERANCE = 2.5

#: Floor on what counts as an outage, whatever the upload interval works out
#: to. Some deployed nodes upload every 14 seconds, and against a threshold of
#: 35 seconds the ordinary lag between a node uploading and dirtviz serving the
#: row reads as an outage: every such node is reported down at the moment you
#: ask, which is exactly the false alarm that gets a monitoring tool ignored.
#: Nothing shorter than this is operationally interesting for a soil
#: deployment anyway, and nothing shorter can be distinguished from ingest lag.
MIN_GAP_S = 300.0

#: What to ask the generic sensor table for as proof of life. Every node
#: reports its own supply voltage, so this is the one series that exists
#: everywhere. Note the spelling: dirtviz files generic measurements under the
#: SensorType enum name plus the human readable measurement name from
#: SENSOR_DATA, so it is ("POWER_VOLTAGE", "Voltage") and not ("power", "v").
#: The endpoint returns an identical empty series for a name that does not
#: exist and one that merely has no data, so a typo here looks exactly like a
#: dead node. /cell/<id>/sensors lists what a given cell actually has.
DEFAULT_SENSOR_NAME = "POWER_VOLTAGE"
DEFAULT_SENSOR_MEAS = "Voltage"


@dataclass(frozen=True)
class Gap:
    """A stretch of time with no data from a node."""

    start: datetime
    end: datetime
    #: True if the gap runs off the start or end of the window, which means
    #: its real length is unknown and only bounded by the window.
    truncated: bool = False

    @property
    def seconds(self) -> float:
        return (self.end - self.start).total_seconds()

    def __str__(self) -> str:
        mark = " (open ended)" if self.truncated else ""
        return (
            f"{self.start:%Y-%m-%d %H:%M} -> {self.end:%Y-%m-%d %H:%M}  "
            f"{_duration(self.seconds)}{mark}"
        )


@dataclass
class Availability:
    """What the arrival times say about one node over one window."""

    window_start: datetime
    window_end: datetime
    n_points: int
    #: Inferred upload interval in seconds, or None when there is too little
    #: data to infer one.
    interval_s: float | None
    gaps: list[Gap] = field(default_factory=list)
    first_seen: datetime | None = None
    last_seen: datetime | None = None
    cell_id: int | None = None
    cell_name: str | None = None

    @property
    def window_seconds(self) -> float:
        return (self.window_end - self.window_start).total_seconds()

    @property
    def downtime_s(self) -> float:
        """Total time in gaps. An upper bound, see the module docstring."""
        return sum(g.seconds for g in self.gaps)

    @property
    def availability(self) -> float:
        """Fraction of the window the node was reporting. A lower bound."""
        if self.window_seconds <= 0:
            return 0.0
        return max(0.0, 1.0 - self.downtime_s / self.window_seconds)

    @property
    def longest_gap(self) -> Gap | None:
        return max(self.gaps, key=lambda g: g.seconds, default=None)

    @property
    def reporting_now(self) -> bool:
        """True if the most recent gap does not run to the end of the window."""
        return not any(g.end >= self.window_end for g in self.gaps)

    def summary(self) -> str:
        name = self.cell_name or (f"cell {self.cell_id}" if self.cell_id else "node")
        if self.n_points == 0:
            return f"{name}: no data in the window at all"

        interval = _duration(self.interval_s) if self.interval_s else "unknown"
        lines = [
            (
                f"{name}: {self.availability * 100:.2f}% available "
                f"(lower bound), {self.n_points} uploads, interval ~{interval}"
            ),
            (
                f"  last seen {self.last_seen:%Y-%m-%d %H:%M} UTC, "
                f"{'reporting' if self.reporting_now else 'SILENT'}"
            ),
        ]
        if self.gaps:
            lines.append(
                f"  {len(self.gaps)} gap(s), {_duration(self.downtime_s)} total"
            )
            for gap in sorted(self.gaps, key=lambda g: -g.seconds)[:5]:
                lines.append(f"    {gap}")
        else:
            lines.append("  no gaps")
        return "\n".join(lines)


def _duration(seconds: float | None) -> str:
    """Render a duration the way someone reads it out in a meeting."""
    if seconds is None:
        return "unknown"
    seconds = round(seconds)
    if seconds < 60:
        return f"{seconds}s"
    minutes, s = divmod(seconds, 60)
    if minutes < 60:
        return f"{minutes}m {s}s" if s else f"{minutes}m"
    hours, m = divmod(minutes, 60)
    if hours < 24:
        return f"{hours}h {m}m" if m else f"{hours}h"
    days, h = divmod(hours, 24)
    return f"{days}d {h}h" if h else f"{days}d"


def _timestamps(data) -> list[datetime]:
    """Pull the timestamp column out of a client DataFrame, if it has one."""
    if data is None or not len(data) or "timestamp" not in data:
        return []
    return [ts.to_pydatetime() for ts in data["timestamp"]]


def _as_utc(dt: datetime) -> datetime:
    """Normalise to aware UTC, so naive and aware inputs can be compared."""
    if dt.tzinfo is None:
        return dt.replace(tzinfo=UTC)
    return dt.astimezone(UTC)


def infer_interval(timestamps: list[datetime]) -> float | None:
    """Infer the upload interval from the arrival times.

    The median gap between consecutive uploads, which is robust to the outages
    being looked for: even a node that was down half the window still has a
    median spacing equal to its real interval.

    Args:
        timestamps: Arrival times, in any order.

    Returns:
        Interval in seconds, or None if there are fewer than two points.
    """
    if len(timestamps) < 2:
        return None

    ordered = sorted(_as_utc(t) for t in timestamps)
    deltas = [
        (b - a).total_seconds()
        for a, b in pairwise(ordered)
        if (b - a).total_seconds() > 0
    ]
    if not deltas:
        return None
    return median(deltas)


def find_gaps(
    timestamps: list[datetime],
    interval_s: float,
    window_start: datetime,
    window_end: datetime,
    tolerance: float = DEFAULT_TOLERANCE,
    min_gap_s: float = MIN_GAP_S,
) -> list[Gap]:
    """Find stretches where uploads stopped arriving.

    Includes the edges: a window that starts well before the first upload, or
    ends well after the last one, is an outage too. Leaving those out is how a
    node that is dead right now ends up reported as 100% available.

    Args:
        timestamps: Arrival times.
        interval_s: Expected seconds between uploads.
        window_start: Start of the window being reported on.
        window_end: End of the window.
        tolerance: Multiple of the interval that counts as an outage.
        min_gap_s: Floor on what counts as an outage at all. See MIN_GAP_S.

    Returns:
        Gaps, in time order.
    """
    threshold = max(interval_s * tolerance, min_gap_s)
    window_start = _as_utc(window_start)
    window_end = _as_utc(window_end)
    ordered = sorted(_as_utc(t) for t in timestamps)

    if not ordered:
        return [Gap(window_start, window_end, truncated=True)]

    gaps: list[Gap] = []

    # Silent at the start of the window. Truncated, because nothing here says
    # when the node actually went down; it may predate the window.
    if (ordered[0] - window_start).total_seconds() > threshold:
        gaps.append(Gap(window_start, ordered[0], truncated=True))

    for a, b in pairwise(ordered):
        if (b - a).total_seconds() > threshold:
            gaps.append(Gap(a, b))

    # Silent at the end, which is the case anyone actually cares about: the
    # node is down right now.
    if (window_end - ordered[-1]).total_seconds() > threshold:
        gaps.append(Gap(ordered[-1], window_end, truncated=True))

    return gaps


def availability_from_timestamps(
    timestamps: list[datetime],
    window_start: datetime,
    window_end: datetime,
    interval_s: float | None = None,
    tolerance: float = DEFAULT_TOLERANCE,
    cell_id: int | None = None,
    cell_name: str | None = None,
    min_gap_s: float = MIN_GAP_S,
) -> Availability:
    """Turn a list of arrival times into an availability report.

    Args:
        timestamps: Arrival times of uploads inside the window.
        window_start: Start of the window.
        window_end: End of the window.
        interval_s: Upload interval, inferred from the data when omitted.
        tolerance: Multiple of the interval that counts as an outage.
        cell_id: Optional, for labelling.
        cell_name: Optional, for labelling.
        min_gap_s: Floor on what counts as an outage. See MIN_GAP_S.

    Returns:
        An Availability report.
    """
    window_start = _as_utc(window_start)
    window_end = _as_utc(window_end)
    ordered = sorted(_as_utc(t) for t in timestamps)

    if interval_s is None:
        interval_s = infer_interval(ordered)

    if interval_s is None:
        # Not enough data to say anything about cadence. A single upload in a
        # week is not a 100% available node, so treat the whole window as
        # unaccounted for rather than inventing an interval.
        gaps = [Gap(window_start, window_end, truncated=True)]
        return Availability(
            window_start=window_start,
            window_end=window_end,
            n_points=len(ordered),
            interval_s=None,
            gaps=gaps,
            first_seen=ordered[0] if ordered else None,
            last_seen=ordered[-1] if ordered else None,
            cell_id=cell_id,
            cell_name=cell_name,
        )

    gaps = find_gaps(
        ordered, interval_s, window_start, window_end, tolerance, min_gap_s
    )

    return Availability(
        window_start=window_start,
        window_end=window_end,
        n_points=len(ordered),
        interval_s=interval_s,
        gaps=gaps,
        first_seen=ordered[0] if ordered else None,
        last_seen=ordered[-1] if ordered else None,
        cell_id=cell_id,
        cell_name=cell_name,
    )


def cell_availability(
    client,
    cell,
    start: datetime,
    end: datetime,
    source: str = "auto",
    tolerance: float = DEFAULT_TOLERANCE,
    resample: str | None = "none",
    min_gap_s: float = MIN_GAP_S,
    sensor_name: str = DEFAULT_SENSOR_NAME,
    sensor_meas: str = DEFAULT_SENSOR_MEAS,
) -> Availability:
    """Availability for one cell, pulled from dirtviz.

    Args:
        client: A :class:`~ents.dirtviz.client.BackendClient`.
        cell: A :class:`~ents.dirtviz.client.Cell`.
        start: Start of the window.
        end: End of the window.
        source: Which series to use as proof of life.

            - "auto" (default) tries the generic sensor table first and falls
              back to the dedicated power route. The fleet is mid migration
              between the two ingest paths, so neither alone covers it: nodes
              on fPort 2 land in the generic tables and are invisible to
              /power/, while older nodes are the reverse.
            - "sensor" forces the generic table, see sensor_name and
              sensor_meas.
            - "power" and "teros" force the dedicated routes.
        tolerance: Multiple of the interval that counts as an outage.
        resample: Passed to the client. Defaults to "none", meaning raw
            measurements. This matters more than it looks: the API's default
            is hourly buckets, and against hourly buckets every node appears
            to upload once an hour no matter what it really does, so nothing
            shorter than a multi-hour outage can be seen. On the deployment
            measured in August 2026 the raw interval was 5 minutes, a factor
            of twelve better in resolution.
        min_gap_s: Floor on what counts as an outage. See MIN_GAP_S.
        sensor_name: Sensor name for the generic table. This is the SensorType
            enum name, not a friendly label.
        sensor_meas: Measurement name for the generic table, which is the
            human readable name from SENSOR_DATA rather than a short code.

    Returns:
        An Availability report.
    """

    def _sensor():
        return client.sensor_data(
            cell, sensor_name, sensor_meas, start, end, resample=resample or "none"
        )

    def _power():
        return client.power_data(cell, start, end, resample=resample)

    if source == "auto":
        data = _sensor()
        if not _timestamps(data):
            data = _power()
    elif source == "sensor":
        data = _sensor()
    elif source == "power":
        data = _power()
    elif source == "teros":
        data = client.teros_data(cell, start, end, resample=resample)
    else:
        raise ValueError(
            f"unknown source {source!r}, expected auto, sensor, power or teros"
        )

    timestamps = _timestamps(data)

    return availability_from_timestamps(
        timestamps,
        start,
        end,
        tolerance=tolerance,
        cell_id=cell.id,
        cell_name=cell.name,
        min_gap_s=min_gap_s,
    )


def _main(argv=None) -> int:
    """Report availability for recently active cells."""
    import argparse

    from .client import BackendClient

    parser = argparse.ArgumentParser(
        prog="python -m ents.dirtviz.uptime",
        description="Node availability derived from upload arrival gaps.",
    )
    parser.add_argument(
        "--days", type=float, default=7.0, help="window length (default 7)"
    )
    parser.add_argument(
        "--cell",
        type=int,
        action="append",
        dest="cells",
        help="cell id, repeatable. Default: every cell with data in the window",
    )
    parser.add_argument(
        "--source",
        default="auto",
        choices=["auto", "sensor", "power", "teros"],
        help="which series to use as proof of life. auto (default) tries the "
        "generic sensor table then falls back to /power/, since the fleet is "
        "split across both ingest paths",
    )
    parser.add_argument(
        "--sensor-name",
        default=DEFAULT_SENSOR_NAME,
        help=f"sensor name in the generic table, the SensorType enum name "
        f"(default {DEFAULT_SENSOR_NAME})",
    )
    parser.add_argument(
        "--sensor-measurement",
        default=DEFAULT_SENSOR_MEAS,
        dest="sensor_meas",
        help=f"measurement name in the generic table (default {DEFAULT_SENSOR_MEAS})",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=DEFAULT_TOLERANCE,
        help=f"missed intervals before it counts as an outage "
        f"(default {DEFAULT_TOLERANCE})",
    )
    parser.add_argument(
        "--min-gap",
        type=float,
        default=MIN_GAP_S,
        dest="min_gap_s",
        help=f"shortest gap that counts as an outage, in seconds "
        f"(default {MIN_GAP_S:g})",
    )
    parser.add_argument(
        "--resample",
        default="none",
        help='server side resampling, "none" for raw measurements (default). '
        "The API's own default is hourly buckets, which hides any outage "
        "shorter than a few hours",
    )
    parser.add_argument("--url", default=None, help="override the API base url")
    args = parser.parse_args(argv)

    client = BackendClient(args.url) if args.url else BackendClient()

    end = datetime.now(UTC)
    start = end - timedelta(days=args.days)

    cells = client.cells()
    if args.cells:
        wanted = set(args.cells)
        cells = [c for c in cells if c.id in wanted]

    reports = []
    for cell in cells:
        try:
            report = cell_availability(
                client,
                cell,
                start,
                end,
                args.source,
                args.tolerance,
                args.resample,
                args.min_gap_s,
                args.sensor_name,
                args.sensor_meas,
            )
        except Exception as exc:  # noqa: BLE001
            print(f"cell {cell.id} ({cell.name}): request failed, {exc}")
            continue
        if report.n_points:
            reports.append(report)

    if not reports:
        print("no cell reported any data in the window")
        return 1

    reports.sort(key=lambda r: r.availability)

    print(
        f"window {start:%Y-%m-%d %H:%M} -> {end:%Y-%m-%d %H:%M} UTC "
        f"({args.days:g} days), source {args.source}, resample {args.resample}\n"
    )
    print(
        f"{'cell':>6}  {'name':<28} {'avail':>8}  {'uploads':>7}  "
        f"{'interval':>9}  {'downtime':>9}  status"
    )
    for r in reports:
        print(
            f"{r.cell_id:>6}  {(r.cell_name or ''):<28} "
            f"{r.availability * 100:>7.2f}%  {r.n_points:>7}  "
            f"{_duration(r.interval_s):>9}  {_duration(r.downtime_s):>9}  "
            f"{'ok' if r.reporting_now else 'SILENT'}"
        )

    degraded = [r for r in reports if r.gaps]
    if degraded:
        print("\ndetail for cells with gaps:\n")
        for r in degraded:
            print(r.summary())
            print()

    print(
        "Downtime is an upper bound and availability a lower bound: a gap is\n"
        "measured from the last upload before it to the first after, so each\n"
        "edge is overstated by up to one upload interval."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(_main())
