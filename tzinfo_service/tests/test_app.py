#!/usr/bin/env python3
"""HTTP-level tests: error handling, /special without coordinates or
leap-second data, readiness, and the forwarded client address."""

import os
import tempfile
import unittest
from pathlib import Path

import tzinfo_service
from tzinfo_service import create_app


class AppTests(unittest.TestCase):
    def make_app(self, leap_path: str, trust_proxy: bool = False):
        environment = {
            "TZINFO_GEOIP_DB": "/nonexistent/GeoLite2-City.mmdb",
            "TZINFO_LEAP_SECONDS": leap_path,
            "TZINFO_TRUST_PROXY": "1" if trust_proxy else "0",
        }
        saved = {name: os.environ.get(name) for name in environment}
        os.environ.update(environment)
        try:
            return create_app()
        finally:
            for name, value in saved.items():
                if value is None:
                    os.environ.pop(name, None)
                else:
                    os.environ[name] = value

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.leap_path = Path(self.tmp.name) / "leap-seconds.list"
        self.leap_path.write_text(
            "#@ 4102444800\n2272060800 10 # 1 Jan 1972\n3692217600 37 # 1 Jan 2017\n",
            encoding="ascii",
        )
        self.missing_leap = str(Path(self.tmp.name) / "missing.list")

    def tearDown(self):
        self.tmp.cleanup()

    def test_wrong_method_is_405_not_500(self):
        client = self.make_app(str(self.leap_path)).test_client()
        response = client.post(tzinfo_service.API_PATH)
        self.assertEqual(response.status_code, 405)
        self.assertEqual(response.get_json()["error"]["code"], "method_not_allowed")

    def test_unknown_path_is_404(self):
        client = self.make_app(str(self.leap_path)).test_client()
        response = client.get("/smol_clock/api/v1/nothing")
        self.assertEqual(response.status_code, 404)
        self.assertEqual(response.get_json()["error"]["code"], "not_found")

    def test_special_without_coordinates_is_none_without_geoip(self):
        client = self.make_app(str(self.leap_path)).test_client()
        response = client.get(tzinfo_service.SPECIAL_PATH)
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.get_json()["zone"], "Special/None")

    def test_special_needs_leap_data_only_for_tai(self):
        client = self.make_app(self.missing_leap).test_client()
        response = client.get(tzinfo_service.SPECIAL_PATH + "?id=mecca_time")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.get_json()["zone"], "Special/Mecca")
        response = client.get(tzinfo_service.SPECIAL_PATH + "?lat=51.477890&lon=-0.001468")
        self.assertEqual(response.get_json()["zone"], "Special/GMT")
        response = client.get(tzinfo_service.SPECIAL_PATH + "?id=tai")
        self.assertEqual(response.status_code, 503)
        self.assertEqual(response.get_json()["error"]["code"], "leap_seconds_unavailable")

    def test_readiness_does_not_depend_on_leap_data(self):
        app = self.make_app(self.missing_leap)
        service = app.extensions["tzinfo_service"]
        service.geoip_ready = lambda: True
        response = app.test_client().get(tzinfo_service.READY_PATH)
        body = response.get_json()
        self.assertEqual(response.status_code, 200)
        self.assertEqual(body["status"], "ready")
        self.assertFalse(body["leap_seconds"])

    def test_forwarded_address_is_the_last(self):
        app = self.make_app(str(self.leap_path), trust_proxy=True)
        headers = {"X-Forwarded-For": "8.8.8.8, 203.0.113.7"}
        with app.test_request_context(headers=headers, environ_base={"REMOTE_ADDR": "127.0.0.1"}):
            self.assertEqual(tzinfo_service.get_client_ip(True), "203.0.113.7")
            self.assertEqual(tzinfo_service.get_client_ip(False), "127.0.0.1")
        with app.test_request_context(headers={"X-Forwarded-For": "junk"},
                                      environ_base={"REMOTE_ADDR": "127.0.0.1"}):
            self.assertEqual(tzinfo_service.get_client_ip(True), "127.0.0.1")


if __name__ == "__main__":
    unittest.main()
