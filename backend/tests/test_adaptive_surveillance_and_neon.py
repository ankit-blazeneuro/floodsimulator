"""
Unit tests for HydroGuard-AI Adaptive Surveillance Scheduling and Neon PostgreSQL Integration.
"""

import sys
import os
import unittest

BACKEND_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if BACKEND_DIR not in sys.path:
    sys.path.insert(0, BACKEND_DIR)

from src.weather_surveillance import WeatherSurveillanceEngine, evaluate_rain_criteria
from src.neon_db import NeonDatabaseManager, get_neon_database_url


class TestAdaptiveSchedulingAndNeon(unittest.TestCase):

    def setUp(self):
        self.engine = WeatherSurveillanceEngine()

    def test_evaluate_rain_criteria_normal(self):
        # Light rain
        hourly = [0.5, 1.0, 0.2, 0.8, 0.0, 0.4, 0.5, 0.2, 0.1, 0.0]
        triggered, reason, r1h, r24h = evaluate_rain_criteria(hourly)
        self.assertFalse(triggered)
        self.assertEqual(reason, "Normal / Below Surveillance Threshold")

    def test_evaluate_rain_criteria_heavy_recent(self):
        # Heavy rain in recent hour
        hourly = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 18.5]
        triggered, reason, r1h, r24h = evaluate_rain_criteria(hourly)
        self.assertTrue(triggered)
        self.assertIn("Heavy Rain", reason)
        self.assertEqual(r1h, 18.5)

    def test_evaluate_rain_criteria_forecast_cloudburst(self):
        # Dry past 6 hours, but upcoming forecast predicts cloudburst (22 mm/hr)
        hourly = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.1, 22.0, 15.0]
        triggered, reason, r1h, r24h = evaluate_rain_criteria(hourly)
        self.assertTrue(triggered)
        self.assertIn("Forecast Heavy Rain", reason)

    def test_adaptive_interval_nominal_when_no_danger(self):
        # Safe dam assessments
        mock_dams = [
            {"dam_name": "Safe Dam A", "alert_level": "NORMAL", "failure_probability": 0.05},
            {"dam_name": "Safe Dam B", "alert_level": "WATCH", "failure_probability": 0.20},
        ]
        is_danger, interval, mode = self.engine.evaluate_danger_state(mock_dams)
        self.assertFalse(is_danger)
        self.assertEqual(interval, 1800)  # 30 minutes
        self.assertEqual(mode, "NOMINAL (30 min)")

    def test_adaptive_interval_crisis_when_danger_active(self):
        # Imminent danger scenario
        mock_dams = [
            {"dam_name": "Safe Dam A", "alert_level": "NORMAL", "failure_probability": 0.05},
            {"dam_name": "Critical Dam B", "alert_level": "IMMINENT", "failure_probability": 0.85},
        ]
        is_danger, interval, mode = self.engine.evaluate_danger_state(mock_dams)
        self.assertTrue(is_danger)
        self.assertEqual(interval, 120)  # 2 minutes
        self.assertEqual(mode, "CRISIS (2 min)")

    def test_adaptive_interval_crisis_when_forecast_cloudburst(self):
        # Forecast heavy rain trigger reason
        mock_dams = [
            {"dam_name": "Dam A", "alert_level": "NORMAL", "failure_probability": 0.10, "trigger_reason": "Forecast Heavy Rain (19.0 mm/h expected)"},
        ]
        is_danger, interval, mode = self.engine.evaluate_danger_state(mock_dams)
        self.assertTrue(is_danger)
        self.assertEqual(interval, 120)  # 2 minutes
        self.assertEqual(mode, "CRISIS (2 min)")

    def test_neon_db_graceful_fallback(self):
        manager = NeonDatabaseManager()
        # Test status reporting when unconfigured
        status = manager.get_status()
        self.assertIn("configured", status)
        self.assertIn("status", status)
        self.assertIn("distribution_ready", status)
        
        # Test sync gracefully skips when unconfigured without crashing
        res = manager.sync_surveillance_results(
            surveillance_results=[],
            counts={"IMMINENT": 0, "WARNING": 0, "WATCH": 0, "NORMAL": 0},
            is_danger_active=False,
            cycle_interval_seconds=1800
        )
        self.assertFalse(res)


if __name__ == "__main__":
    unittest.main()
