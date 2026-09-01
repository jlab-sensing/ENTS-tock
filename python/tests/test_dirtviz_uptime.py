"""Tests for availability derived from upload arrival gaps.

All synthetic, so they run without network access. The cases that matter are
the ones where a naive implementation looks right: a node that is dead right
now still has a perfect record of arrivals up to the moment it died, and a
node with one lonely upload has no gaps between its uploads at all.
"""

import unittest
from datetime import UTC, datetime, timedelta

from ents.dirtviz.uptime import (
    Availability,
    availability_from_timestamps,
    cell_availability,
    find_gaps,
    infer_interval,
)

HOUR = 3600.0
START = datetime(2026, 8, 1, 0, 0, tzinfo=UTC)


def series(start, count, interval_s=HOUR):
    """A run of evenly spaced uploads."""
    return [start + timedelta(seconds=i * interval_s) for i in range(count)]


class TestInferInterval(unittest.TestCase):
    def test_even_series(self):
        self.assertEqual(infer_interval(series(START, 24)), HOUR)

    def test_needs_two_points(self):
        self.assertIsNone(infer_interval([]))
        self.assertIsNone(infer_interval([START]))

    def test_outage_does_not_move_the_median(self):
        """A node down for half the window still uploads hourly when it is up."""
        stamps = series(START, 12) + series(START + timedelta(hours=36), 12)
        self.assertEqual(infer_interval(stamps), HOUR)

    def test_jitter(self):
        stamps = [
            START,
            START + timedelta(seconds=3550),
            START + timedelta(seconds=7250),
            START + timedelta(seconds=10800),
        ]
        self.assertAlmostEqual(infer_interval(stamps), 3625.0, delta=120)

    def test_unordered_input(self):
        stamps = list(reversed(series(START, 10)))
        self.assertEqual(infer_interval(stamps), HOUR)


class TestFindGaps(unittest.TestCase):
    def test_no_gaps_in_a_clean_series(self):
        stamps = series(START, 25)
        gaps = find_gaps(stamps, HOUR, START, START + timedelta(hours=24))
        self.assertEqual(gaps, [])

    def test_single_missed_upload_is_not_an_outage(self):
        """Uploads jitter and retry. One miss is noise, not a story."""
        stamps = series(START, 10)
        del stamps[5]
        gaps = find_gaps(stamps, HOUR, START, START + timedelta(hours=9))
        self.assertEqual(gaps, [])

    def test_interior_outage(self):
        stamps = series(START, 6) + series(START + timedelta(hours=12), 6)
        gaps = find_gaps(stamps, HOUR, START, START + timedelta(hours=17))
        self.assertEqual(len(gaps), 1)
        self.assertAlmostEqual(gaps[0].seconds, 7 * HOUR)
        self.assertFalse(gaps[0].truncated)

    def test_node_dead_at_the_end_of_the_window(self):
        """The case that matters: silent right now."""
        stamps = series(START, 6)
        gaps = find_gaps(stamps, HOUR, START, START + timedelta(hours=24))
        self.assertEqual(len(gaps), 1)
        self.assertTrue(gaps[0].truncated)
        self.assertAlmostEqual(gaps[0].seconds, 19 * HOUR)

    def test_node_arrived_late_in_the_window(self):
        stamps = series(START + timedelta(hours=12), 12)
        gaps = find_gaps(stamps, HOUR, START, START + timedelta(hours=23))
        self.assertEqual(len(gaps), 1)
        self.assertTrue(gaps[0].truncated)
        self.assertAlmostEqual(gaps[0].seconds, 12 * HOUR)

    def test_no_data_at_all_is_one_long_gap(self):
        gaps = find_gaps([], HOUR, START, START + timedelta(hours=24))
        self.assertEqual(len(gaps), 1)
        self.assertAlmostEqual(gaps[0].seconds, 24 * HOUR)

    def test_tolerance_is_respected(self):
        stamps = [START, START + timedelta(hours=2)]
        self.assertEqual(
            find_gaps(stamps, HOUR, START, START + timedelta(hours=2), 2.5), []
        )
        self.assertEqual(
            len(find_gaps(stamps, HOUR, START, START + timedelta(hours=2), 1.5)), 1
        )


class TestAvailability(unittest.TestCase):
    def test_perfect_node(self):
        end = START + timedelta(hours=24)
        report = availability_from_timestamps(series(START, 25), START, end)
        self.assertEqual(report.availability, 1.0)
        self.assertEqual(report.downtime_s, 0.0)
        self.assertTrue(report.reporting_now)
        self.assertEqual(report.n_points, 25)
        self.assertEqual(report.interval_s, HOUR)

    def test_half_the_window_down(self):
        end = START + timedelta(hours=24)
        stamps = series(START, 12) + series(START + timedelta(hours=23), 2)
        report = availability_from_timestamps(stamps, START, end)
        self.assertAlmostEqual(report.availability, 1 - 12 / 24, places=3)
        self.assertTrue(report.reporting_now)

    def test_dead_node_is_not_reported_available(self):
        """Six good hours then silence is 25% available, not 100%."""
        end = START + timedelta(hours=24)
        report = availability_from_timestamps(series(START, 6), START, end)
        self.assertFalse(report.reporting_now)
        self.assertLess(report.availability, 0.3)

    def test_no_data(self):
        end = START + timedelta(hours=24)
        report = availability_from_timestamps([], START, end)
        self.assertEqual(report.availability, 0.0)
        self.assertEqual(report.n_points, 0)
        self.assertIsNone(report.interval_s)
        self.assertIn("no data", report.summary())

    def test_single_upload_is_not_full_availability(self):
        """One point has no interval to infer, and no gaps between points."""
        end = START + timedelta(hours=24)
        report = availability_from_timestamps([START], START, end)
        self.assertEqual(report.n_points, 1)
        self.assertIsNone(report.interval_s)
        self.assertEqual(report.availability, 0.0)

    def test_explicit_interval_overrides_inference(self):
        """A node configured for hourly uploads that only managed one a day."""
        end = START + timedelta(days=6)
        stamps = series(START, 7, interval_s=24 * HOUR)
        inferred = availability_from_timestamps(stamps, START, end)
        self.assertEqual(inferred.availability, 1.0)

        configured = availability_from_timestamps(stamps, START, end, interval_s=HOUR)
        self.assertLess(configured.availability, 0.1)

    def test_naive_datetimes_are_treated_as_utc(self):
        naive_start = datetime(2026, 8, 1, 0, 0)  # noqa: DTZ001 - the point
        stamps = [naive_start + timedelta(hours=i) for i in range(25)]
        report = availability_from_timestamps(
            stamps, naive_start, naive_start + timedelta(hours=24)
        )
        self.assertEqual(report.availability, 1.0)

    def test_longest_gap(self):
        end = START + timedelta(hours=30)
        stamps = (
            series(START, 4)
            + series(START + timedelta(hours=8), 4)
            + series(START + timedelta(hours=24), 7)
        )
        report = availability_from_timestamps(stamps, START, end)
        self.assertEqual(len(report.gaps), 2)
        self.assertAlmostEqual(report.longest_gap.seconds, 13 * HOUR)

    def test_downtime_is_an_upper_bound(self):
        """The reported gap is longer than the true outage, never shorter.

        True outage here is 5 hours: the node stopped some time after 04:00 and
        was back by 09:00. Measured from arrival times it reads as 5 hours,
        which brackets the truth rather than understating it.
        """
        end = START + timedelta(hours=12)
        stamps = series(START, 5) + series(START + timedelta(hours=9), 4)
        report = availability_from_timestamps(stamps, START, end)
        self.assertAlmostEqual(report.downtime_s, 5 * HOUR)
        self.assertGreaterEqual(report.downtime_s, 4 * HOUR)

    def test_summary_mentions_silence(self):
        end = START + timedelta(hours=24)
        report = availability_from_timestamps(
            series(START, 4), START, end, cell_name="test_node"
        )
        self.assertIn("SILENT", report.summary())
        self.assertIn("test_node", report.summary())

    def test_window_with_no_duration(self):
        report = Availability(
            window_start=START, window_end=START, n_points=0, interval_s=None
        )
        self.assertEqual(report.availability, 0.0)


class TestMinimumGap(unittest.TestCase):
    """Nodes that upload every few seconds must not trip on ingest lag.

    Some deployed cells upload every 14 seconds. Scaling the threshold purely
    off the interval makes it 35 seconds, and the ordinary delay between a node
    uploading and dirtviz serving the row then reads as an outage, so every
    such node is reported down at the moment you ask.
    """

    FAST = 14.0

    def test_short_tail_gap_is_not_an_outage(self):
        end = START + timedelta(hours=1)
        stamps = [
            START + timedelta(seconds=i * self.FAST)
            for i in range(int(3600 / self.FAST) - 6)
        ]
        report = availability_from_timestamps(stamps, START, end)
        self.assertTrue(report.reporting_now)
        self.assertEqual(report.gaps, [])

    def test_the_floor_can_be_lowered(self):
        """With no floor the same series does trip, which is the bug."""
        end = START + timedelta(hours=1)
        stamps = [
            START + timedelta(seconds=i * self.FAST)
            for i in range(int(3600 / self.FAST) - 6)
        ]
        report = availability_from_timestamps(stamps, START, end, min_gap_s=0.0)
        self.assertFalse(report.reporting_now)

    def test_a_real_outage_still_shows(self):
        """The floor must not hide a genuine multi hour gap."""
        end = START + timedelta(hours=6)
        first = [START + timedelta(seconds=i * self.FAST) for i in range(120)]
        second = [START + timedelta(hours=4, seconds=i * self.FAST) for i in range(120)]
        report = availability_from_timestamps(first + second, START, end)
        self.assertEqual(len(report.gaps), 2)
        self.assertGreater(report.longest_gap.seconds, 3 * HOUR)

    def test_floor_applies_to_slow_nodes_too(self):
        """A four minute hiccup on an hourly node is still under the floor."""
        stamps = [START, START + timedelta(minutes=4)]
        gaps = find_gaps(
            stamps, 60.0, START, START + timedelta(minutes=4), min_gap_s=300.0
        )
        self.assertEqual(gaps, [])


class FakeFrame(list):
    """Minimal stand-in for the DataFrame the client returns."""

    def __init__(self, stamps):
        super().__init__(range(len(stamps)))
        self._stamps = stamps

    def __contains__(self, key):
        return key == "timestamp"

    def __getitem__(self, key):
        if key == "timestamp":
            return [FakeTimestamp(t) for t in self._stamps]
        return super().__getitem__(key)


class FakeTimestamp:
    def __init__(self, dt):
        self._dt = dt

    def to_pydatetime(self):
        return self._dt


class FakeCell:
    id = 1483
    name = "Field Cell 3"


class FakeClient:
    """Records which route was asked for, and serves only one of them."""

    def __init__(self, sensor_stamps=None, power_stamps=None):
        self.sensor_stamps = sensor_stamps or []
        self.power_stamps = power_stamps or []
        self.calls = []

    def sensor_data(self, cell, name, meas, start, end, resample="none"):
        self.calls.append(("sensor", name, meas))
        return FakeFrame(self.sensor_stamps)

    def power_data(self, cell, start, end, resample=None):
        self.calls.append(("power",))
        return FakeFrame(self.power_stamps)

    def teros_data(self, cell, start, end, resample=None):
        self.calls.append(("teros",))
        return FakeFrame([])


class TestSourceSelection(unittest.TestCase):
    """The fleet is split across two ingest paths, so neither alone covers it.

    A node on fPort 2 lands in the generic sensor table and is invisible to
    /power/; older nodes are the reverse. Picking the wrong one reports a
    healthy node as having no data at all.
    """

    def setUp(self):
        self.cell = FakeCell()
        self.end = START + timedelta(hours=24)

    def test_auto_prefers_the_generic_table(self):
        client = FakeClient(sensor_stamps=series(START, 25))
        report = cell_availability(client, self.cell, START, self.end)
        self.assertEqual(report.n_points, 25)
        self.assertEqual(client.calls[0][0], "sensor")
        self.assertEqual(len(client.calls), 1, "should not have fallen back")

    def test_auto_falls_back_to_power(self):
        client = FakeClient(sensor_stamps=[], power_stamps=series(START, 25))
        report = cell_availability(client, self.cell, START, self.end)
        self.assertEqual(report.n_points, 25)
        self.assertEqual([c[0] for c in client.calls], ["sensor", "power"])

    def test_auto_reports_nothing_when_neither_has_data(self):
        client = FakeClient()
        report = cell_availability(client, self.cell, START, self.end)
        self.assertEqual(report.n_points, 0)
        self.assertEqual(report.availability, 0.0)

    def test_generic_query_uses_the_enum_naming_scheme(self):
        """("power", "v") silently returns empty; the real spelling is this."""
        client = FakeClient(sensor_stamps=series(START, 3))
        cell_availability(client, self.cell, START, self.end)
        self.assertEqual(client.calls[0], ("sensor", "POWER_VOLTAGE", "Voltage"))

    def test_explicit_source_does_not_fall_back(self):
        client = FakeClient(power_stamps=series(START, 25))
        report = cell_availability(client, self.cell, START, self.end, source="sensor")
        self.assertEqual(report.n_points, 0)
        self.assertEqual([c[0] for c in client.calls], ["sensor"])

    def test_unknown_source_rejected(self):
        client = FakeClient()
        with self.assertRaises(ValueError):
            cell_availability(client, self.cell, START, self.end, source="nope")


if __name__ == "__main__":
    unittest.main()
