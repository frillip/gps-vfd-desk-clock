#!/usr/bin/env python3
"""Regression tests for Smol Clock special timebases."""

import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path

from tzinfo_service import LeapSecondTable, TzInfoService


UTC = timezone.utc
NTP_UNIX_EPOCH_DELTA = 2208988800


def epoch(iso_date: str) -> int:
    return int(datetime.fromisoformat(iso_date).replace(tzinfo=UTC).timestamp())


class SpecialTimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.leap_path = Path(cls.tmp.name) / "leap-seconds.list"
        cls.leap_path.write_text(
            "\n".join(
                [
                    "# test leap-second table",
                    "#@ 4102444800",  # 2030-01-01 UTC in NTP epoch seconds
                    "2272060800 10 # 1 Jan 1972",
                    "3692217600 37 # 1 Jan 2017",
                    "",
                ]
            ),
            encoding="ascii",
        )
        cls.service = TzInfoService(
            db_path="/nonexistent/GeoLite2-City.mmdb",
            transition_years=5,
            max_transitions=8,
            leap_seconds_path=str(cls.leap_path),
        )

    @classmethod
    def tearDownClass(cls):
        cls.service.close()
        cls.tmp.cleanup()

    def test_tai_is_utc_minus_tai(self):
        table = LeapSecondTable(str(self.leap_path))
        tai, next_change, expires = table.state(epoch("2026-10-12T10:00:00"))
        self.assertEqual(tai, -37)
        self.assertIsNone(next_change)
        self.assertEqual(expires, 4102444800 - NTP_UNIX_EPOCH_DELTA)

    def test_mecca_explicit_requires_no_coordinates(self):
        response = self.service.build_special_response(
            now_epoch=epoch("2026-10-12T10:00:00"),
            special_id="mecca_time",
        )
        self.assertEqual(response["next"], [])
        self.assertEqual(
            response["special"],
            [
                {
                    "dst": False,
                    "dst_offset": 0,
                    "offset_s": 9558,
                    "offset_ns": 300000000,
                    "transition_utc": 0,
                }
            ],
        )

    def test_tai_explicit_requires_no_coordinates(self):
        response = self.service.build_special_response(
            now_epoch=epoch("2026-10-12T10:00:00"),
            special_id="tai",
        )
        self.assertEqual(
            response["special"],
            [{
                "dst": False,
                "dst_offset": 0,
                "offset_s": 37,
                "offset_ns": 0,
                "transition_utc": 0,
            }],
        )
        self.assertEqual(response["next"], [])

        self.assertEqual(response["zone"], "Special/TAI")

    def test_geofence_mecca(self):
        response = self.service.build_special_response(
            now_epoch=epoch("2026-10-12T10:00:00"),
            lat=21.4224779,
            lon=39.8251832,
        )
        self.assertEqual(response["special"][0]["offset_s"], 9558)
        self.assertEqual(response["special"][0]["offset_ns"], 300000000)
        self.assertEqual(response["next"], [])

    def test_geofence_paris_observatory_and_louvre(self):
        for lat, lon in ((48.8364389, 2.3365056), (48.861027, 2.335708)):
            with self.subTest(lat=lat, lon=lon):
                response = self.service.build_special_response(
                    now_epoch=epoch("2026-10-12T10:00:00"),
                    lat=lat,
                    lon=lon,
                )
                self.assertEqual(response["special"][0]["offset_s"], 561)
                self.assertEqual(response["special"][0]["offset_ns"], 0)

    def test_geofence_dunsink(self):
        response = self.service.build_special_response(
            now_epoch=epoch("2026-10-12T10:00:00"),
            lat=53.38708,
            lon=-6.33756,
        )
        self.assertEqual(response["special"][0]["offset_s"], -1521)
        self.assertEqual(response["special"][0]["offset_ns"], 0)

    def test_unmatched_coordinates_return_special_none(self):
        response = self.service.build_special_response(
            now_epoch=epoch("2026-10-12T10:00:00"),
            lat=13.0827,
            lon=80.2707,
        )
        self.assertEqual(
            response["special"],
            [{
                "dst": False,
                "dst_offset": 0,
                "offset_s": 0,
                "offset_ns": 0,
                "transition_utc": 0,
            }],
        )
        self.assertEqual(response["next"], [])
        self.assertEqual(response["zone"], "Special/None")

    def test_explicit_special_overrides_geofence(self):
        response = self.service.build_special_response(
            now_epoch=epoch("2026-10-12T10:00:00"),
            special_id="dublin_mean",
            lat=21.4224779,
            lon=39.8251832,
        )
        self.assertEqual(response["special"][0]["offset_s"], -1521)


    def test_special_zone_names(self):
        now = epoch("2026-10-12T10:00:00")

        mecca = self.service.build_special_response(
            now_epoch=now, special_id="mecca_time"
        )
        self.assertEqual(mecca["zone"], "Special/Mecca")

        paris = self.service.build_special_response(
            now_epoch=now, special_id="paris_meridian"
        )
        self.assertEqual(paris["zone"], "Special/Paris_Meridian")

        dublin = self.service.build_special_response(
            now_epoch=now, special_id="dublin_mean"
        )
        self.assertEqual(dublin["zone"], "Special/DMT")

        tai = self.service.build_special_response(
            now_epoch=now, special_id="tai"
        )

        neutral = self.service.build_special_response(
            now_epoch=now, lat=13.0827, lon=80.2707
        )
        self.assertEqual(neutral["zone"], "Special/None")


    def test_greenwich_geofence_returns_special_gmt(self):
        response = self.service.build_special_response(
            now_epoch=epoch("2026-10-12T10:00:00"),
            lat=51.477890,
            lon=0.0,
        )
        self.assertEqual(response["zone"], "Special/GMT")
        self.assertEqual(
            response["special"][0],
            {
                "dst": False,
                "dst_offset": 0,
                "offset_s": 0,
                "offset_ns": 0,
                "transition_utc": 0,
            },
        )
        self.assertNotIn("tai", response)

    def test_explicit_tai_is_special_zone(self):
        response = self.service.build_special_response(
            now_epoch=epoch("2026-10-12T10:00:00"),
            special_id="tai",
        )
        self.assertEqual(response["zone"], "Special/TAI")
        self.assertEqual(response["special"][0]["offset_s"], 37)
        self.assertEqual(response["special"][0]["offset_ns"], 0)
        self.assertNotIn("tai", response)

    def test_no_geofence_returns_special_none(self):
        response = self.service.build_special_response(
            now_epoch=epoch("2026-10-12T10:00:00"),
            lat=13.0827,
            lon=80.2707,
        )
        self.assertEqual(response["zone"], "Special/None")
        self.assertEqual(
            response["special"][0],
            {
                "dst": False,
                "dst_offset": 0,
                "offset_s": 0,
                "offset_ns": 0,
                "transition_utc": 0,
            },
        )
        self.assertNotIn("tai", response)


if __name__ == "__main__":
    unittest.main()
