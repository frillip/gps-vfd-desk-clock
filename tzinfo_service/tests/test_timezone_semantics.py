#!/usr/bin/env python3
"""Regression tests for Smol Clock timezone protocol semantics.

Run from the application directory:
    .venv/bin/python -m unittest -v tests.test_timezone_semantics
"""

import unittest
from datetime import datetime, timezone
from unittest.mock import patch

from tzinfo_service import TzInfoService


UTC = timezone.utc


def epoch(iso_date: str) -> int:
    return int(datetime.fromisoformat(iso_date).replace(tzinfo=UTC).timestamp())


def effective_offset(state) -> int:
    return state.offset + (state.dst_offset if state.dst else 0)


class TimezoneSemanticsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.service = TzInfoService(
            db_path="/nonexistent/GeoLite2-City.mmdb",
            transition_years=5,
            max_transitions=8,
        )

    def assert_protocol_invariant(self, zone: str, when: str):
        ts = epoch(when)
        state, _ = self.service.snapshot(zone, ts)
        effective, _active_delta, _active = self.service._zoneinfo_state(zone, ts)
        self.assertEqual(effective_offset(state), effective, (zone, when, state))
        self.assertLessEqual(state.transition_utc, ts)

    def test_protocol_invariant_pathological_zones(self):
        cases = [
            ("Europe/London", "2026-01-15T12:00:00"),
            ("Europe/London", "2026-08-15T12:00:00"),
            ("Europe/Berlin", "2026-08-15T12:00:00"),
            ("America/New_York", "2026-01-15T12:00:00"),
            ("America/New_York", "2026-08-15T12:00:00"),
            ("Australia/Lord_Howe", "2026-01-15T12:00:00"),
            ("Australia/Lord_Howe", "2026-08-15T12:00:00"),
            ("Asia/Kolkata", "2026-08-15T12:00:00"),
            ("Asia/Kathmandu", "2026-08-15T12:00:00"),
            ("Pacific/Chatham", "2026-01-15T12:00:00"),
            ("Africa/Casablanca", "2026-03-15T12:00:00"),
            ("Africa/Casablanca", "2026-08-15T12:00:00"),
            ("Antarctica/Troll", "2026-01-15T12:00:00"),
            ("Antarctica/Troll", "2026-08-15T12:00:00"),
            ("Etc/UTC", "2026-08-15T12:00:00"),
        ]
        for zone, when in cases:
            with self.subTest(zone=zone, when=when):
                self.assert_protocol_invariant(zone, when)

    def test_current_transition_is_most_recent_past_transition(self):
        ts = epoch("2026-08-15T12:00:00")
        state, future = self.service.snapshot("Europe/London", ts)

        self.assertGreater(state.transition_utc, 0)
        self.assertLessEqual(state.transition_utc, ts)
        self.assertTrue(state.dst)
        self.assertEqual(state.offset, 0)
        self.assertEqual(state.dst_offset, 3600)
        self.assertTrue(future)
        self.assertGreater(future[0].transition_utc, ts)

    def test_no_modern_transition_uses_zero_sentinel(self):
        state, transitions = self.service.snapshot(
            "Etc/UTC", epoch("2026-08-15T12:00:00")
        )
        self.assertEqual(state.transition_utc, 0)
        self.assertFalse(state.dst)
        self.assertEqual(state.offset, 0)
        self.assertEqual(state.dst_offset, 0)
        self.assertEqual(transitions, [])

    def test_half_hour_dst_is_preserved(self):
        state, _ = self.service.snapshot(
            "Australia/Lord_Howe", epoch("2026-01-15T12:00:00")
        )
        self.assertTrue(state.dst)
        self.assertEqual(state.dst_offset, 1800)
        self.assertEqual(state.offset, 37800)

    def test_casablanca_rearguard_semantics_are_self_consistent(self):
        cases = [
            ("2026-03-15T12:00:00", 0),
            ("2026-04-01T12:00:00", 3600),
            ("2026-10-01T12:00:00", 0),
        ]
        for when, expected_effective in cases:
            with self.subTest(when=when):
                ts = epoch(when)
                state, _ = self.service.snapshot("Africa/Casablanca", ts)
                self.assertEqual(effective_offset(state), expected_effective)
                zoneinfo_effective, _, _ = self.service._zoneinfo_state(
                    "Africa/Casablanca", ts
                )
                self.assertEqual(effective_offset(state), zoneinfo_effective)

    def test_no_dst_quarter_hour_zone(self):
        state, transitions = self.service.snapshot(
            "Asia/Kathmandu", epoch("2026-08-15T12:00:00")
        )
        self.assertFalse(state.dst)
        self.assertEqual(state.offset, 20700)
        self.assertEqual(state.dst_offset, 0)
        self.assertEqual(transitions, [])

    def test_each_returned_transition_is_self_consistent(self):
        zones = (
            "Europe/London",
            "America/New_York",
            "Australia/Lord_Howe",
            "Africa/Casablanca",
            "Antarctica/Troll",
        )
        for zone in zones:
            _, transitions = self.service.snapshot(
                zone, epoch("2026-08-15T12:00:00")
            )
            for item in transitions:
                with self.subTest(zone=zone, transition_utc=item.transition_utc):
                    effective, _, _ = self.service._zoneinfo_state(
                        zone, item.transition_utc
                    )
                    self.assertEqual(effective_offset(item), effective)

    def test_snapshot_invokes_zdump_once(self):
        original = self.service._zdump_records
        with patch.object(
            self.service,
            "_zdump_records",
            wraps=original,
        ) as mocked:
            self.service.snapshot("Europe/London", epoch("2026-08-15T12:00:00"))
            self.assertEqual(mocked.call_count, 1)

    def test_response_shape_uses_current_and_transition_utc(self):
        now = epoch("2026-08-15T12:00:00")
        response = self.service.build_response("Europe/London", "gnss", now)

        self.assertEqual(response["generated"], now)
        self.assertEqual(response["source"], "gnss")
        self.assertEqual(response["version"], 1)
        self.assertEqual(response["zone"], "Europe/London")
        self.assertEqual(len(response["current"]), 1)
        self.assertIn("transition_utc", response["current"][0])
        self.assertNotIn("epoch", response["current"][0])
        for item in response["next"]:
            self.assertIn("transition_utc", item)
            self.assertNotIn("epoch", item)


if __name__ == "__main__":
    unittest.main()
