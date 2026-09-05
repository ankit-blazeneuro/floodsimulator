"""
HydroGuard-AI: Layer 1 — Weather Surveillance Engine

Polls the Open-Meteo Forecast API to detect sustained heavy precipitation
events (cloudbursts) across the Indian subcontinent. Flags grid cells where
rainfall >= threshold (default 20 mm/hr) sustained for >= N hours.

API Reference: https://open-meteo.com/en/docs
"""

import os
import sys
import json
import time
import asyncio
import logging
from typing import List, Dict, Any, Optional, Tuple
from datetime import datetime, timezone
import httpx

logger = logging.getLogger("hydroguard_surveillance")

BACKEND_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODAL_BATCH_URL = os.getenv(
    "MODAL_BATCH_URL",
    "https://work-ankit-mail--hydroguard-training-batch-predict-breach-web.modal.run"
)
SURVEILLANCE_FILE = os.path.join(BACKEND_DIR, "models", "active_surveillance.json")
DAMS_SEED_FILE = os.path.join(BACKEND_DIR, "data", "india_dams_seed.json")

try:
    from src.feature_pipeline import FeaturePipeline
except ImportError:
    from feature_pipeline import FeaturePipeline

try:
    from src.neon_db import neon_db
except ImportError:
    try:
        from neon_db import neon_db
    except ImportError:
        neon_db = None


def evaluate_rain_criteria(hourly_precip: List[float]) -> Tuple[bool, str, float, float]:
    """
    Evaluates rain surveillance criteria from recent and forecasted hourly precipitation:
    - Heavy rain for 1+ hr: any recent hour >= 15.0 mm/hr
    - Moderate rain for 2+ hr: 2 consecutive hours >= 7.5 mm/hr or 2h sum >= 15.0 mm
    - Slow rain for 3+ hr: 3 consecutive hours >= 2.0 mm/hr or 3h sum >= 7.5 mm
    - Forecast Heavy Cloudburst: next 3h forecast >= 15.0 mm/hr or 2h forecast sum >= 15.0 mm

    Returns:
        (is_triggered, trigger_type, rain_1h, rain_24h)
    """
    if not hourly_precip:
        return False, "No Data", 0.0, 0.0

    clean_precip = [max(0.0, float(p)) for p in hourly_precip if p is not None]
    if not clean_precip:
        return False, "No Data", 0.0, 0.0

    # With past_hours=6 & forecast_hours=6, indices 0..5 are past, 6 is current, 7+ are forecast
    if len(clean_precip) >= 8:
        past_current = clean_precip[:7]
        forecast = clean_precip[7:]
    else:
        past_current = clean_precip
        forecast = []

    rain_1h = past_current[-1] if past_current else 0.0
    rain_24h = sum(past_current)

    # 1. Heavy rain for 1+ hr (current/recent)
    for val in past_current[-3:]:
        if val >= 15.0:
            return True, f"Heavy Rain (1h: {val:.1f} mm/h)", rain_1h, rain_24h

    # 2. Moderate rain for 2+ hr (current/recent)
    if len(past_current) >= 2:
        for i in range(len(past_current) - 1):
            if (past_current[i] >= 7.5 and past_current[i+1] >= 7.5) or (past_current[i] + past_current[i+1] >= 15.0 and past_current[i] >= 4.0 and past_current[i+1] >= 4.0):
                two_hr_sum = past_current[i] + past_current[i+1]
                return True, f"Moderate Rain (2h: {two_hr_sum:.1f} mm)", rain_1h, rain_24h

    # 3. Slow rain for 3+ hr (current/recent)
    if len(past_current) >= 3:
        for i in range(len(past_current) - 2):
            if (past_current[i] >= 2.0 and past_current[i+1] >= 2.0 and past_current[i+2] >= 2.0) or (past_current[i] + past_current[i+1] + past_current[i+2] >= 7.5 and past_current[i] >= 1.0 and past_current[i+1] >= 1.0 and past_current[i+2] >= 1.0):
                three_hr_sum = past_current[i] + past_current[i+1] + past_current[i+2]
                return True, f"Slow Rain (3h: {three_hr_sum:.1f} mm)", rain_1h, rain_24h

    # 4. Forecast Heavy Cloudburst (upcoming 3-6 hours)
    if forecast:
        for val in forecast[:3]:
            if val >= 15.0:
                return True, f"Forecast Heavy Rain ({val:.1f} mm/h expected)", rain_1h, rain_24h
        if len(forecast) >= 2 and (forecast[0] + forecast[1] >= 15.0):
            return True, f"Forecast Cloudburst ({forecast[0] + forecast[1]:.1f} mm in 2h)", rain_1h, rain_24h

    return False, "Normal / Below Surveillance Threshold", rain_1h, rain_24h


class WeatherSurveillanceEngine:
    def __init__(self, local_predictor=None):
        self.local_predictor = local_predictor
        self.pipeline = FeaturePipeline()
        self.dams: List[Dict[str, Any]] = []
        self.surveillance_results: List[Dict[str, Any]] = []
        self.last_scan_time: Optional[str] = None
        self.is_scanning: bool = False
        self.is_danger_active: bool = False
        self.cycle_interval_seconds: int = 1800  # Default nominal: 30 minutes
        self.polling_mode: str = "NOMINAL (30 min)"
        self.next_scan_time: Optional[str] = None
        self._load_dams()

    def evaluate_danger_state(self, results: Optional[List[Dict[str, Any]]] = None) -> Tuple[bool, int, str]:
        """
        Evaluates danger state across dam assessments:
        - Crisis Mode (2 minutes = 120s): If any dam triggers WARNING or IMMINENT, or breach prob >= 0.50, or heavy cloudburst forecast.
        - Nominal Mode (30 minutes = 1800s): Safe patrol interval when all dams operate within nominal tolerances.
        """
        eval_list = results if results is not None else self.surveillance_results
        if not eval_list:
            return False, 1800, "NOMINAL (30 min)"

        is_danger = any(
            d.get("alert_level") in ("WARNING", "IMMINENT")
            or d.get("failure_probability", 0.0) >= 0.50
            or "Forecast Heavy" in str(d.get("trigger_reason", ""))
            or "Forecast Cloudburst" in str(d.get("trigger_reason", ""))
            for d in eval_list
        )

        interval = 120 if is_danger else 1800
        mode = "CRISIS (2 min)" if is_danger else "NOMINAL (30 min)"
        return is_danger, interval, mode

    def _load_dams(self):
        """Loads all dams from seed json or geojson."""
        if os.path.exists(DAMS_SEED_FILE):
            try:
                with open(DAMS_SEED_FILE, "r") as f:
                    data = json.load(f)
                    raw_dams = data.get("dams", [])
                    # Filter out historical breached dams that no longer exist (e.g. Phuktal 2015, Rishi Ganga 2021)
                    self.dams = [
                        d for d in raw_dams
                        if d.get("is_active", True) is not False
                        and d.get("status") != "HISTORICAL_BREACHED"
                        and "(2015)" not in d.get("name", "")
                        and "(2021)" not in d.get("name", "")
                    ]
                    logger.info(f"Loaded {len(self.dams)} active operating dams for continuous weather surveillance.")
            except Exception as e:
                logger.error(f"Failed to load {DAMS_SEED_FILE}: {e}")

        # Fallback priority active dams if file empty
        if not self.dams:
            self.dams = [
                {"id": "AP005", "name": "Prakasam Barrage", "state": "Andhra Pradesh", "lat": 16.5075, "lon": 80.6053, "height_m": 22.25, "is_natural": False},
                {"id": "UK006", "name": "Tehri Dam", "state": "Uttarakhand", "lat": 30.3780, "lon": 78.4800, "height_m": 260.5, "is_natural": False},
                {"id": "HP007", "name": "Bhakra Dam", "state": "Himachal Pradesh", "lat": 31.4100, "lon": 76.4350, "height_m": 226.0, "is_natural": False},
                {"id": "MH008", "name": "Koyna Dam", "state": "Maharashtra", "lat": 17.3980, "lon": 73.7490, "height_m": 103.2, "is_natural": False},
            ]

    async def scan_all_dams(self) -> List[Dict[str, Any]]:
        """
        Executes a nationwide meteorological scan:
        1. Queries Open-Meteo across spatial clusters of India.
        2. Evaluates rain surveillance conditions:
           - Heavy rain for 1+ hr (>= 15 mm/hr)
           - Moderate rain for 2+ hr (>= 7.5 mm/hr)
           - Slow rain for 3+ hr (>= 2.0 mm/hr)
        3. Offloads raining dams to Modal serverless GPU in batch.
        """
        if self.is_scanning:
            logger.info("Scan already in progress, returning cached results.")
            return self.surveillance_results

        self.is_scanning = True
        scan_start = time.time()
        logger.info(f"Starting nationwide Open-Meteo weather surveillance across {len(self.dams)} active dams...")

        # 1. Build regional grid clusters to minimize Open-Meteo queries
        clusters: Dict[Tuple[float, float], List[Dict[str, Any]]] = {}
        for dam in self.dams:
            lat = round(dam.get("lat", 20.0), 4)
            lon = round(dam.get("lon", 78.0), 4)
            grid_key = (round(lat / 0.75) * 0.75, round(lon / 0.75) * 0.75)
            clusters.setdefault(grid_key, []).append(dam)

        logger.info(f"Grouped {len(self.dams)} active dams into {len(clusters)} meteorological grid cells.")

        # 2. Query Open-Meteo in multi-coordinate batches
        grid_points = list(clusters.keys())
        batch_size = 50  # Open-Meteo multi-location query
        grid_weather: Dict[Tuple[float, float], List[float]] = {}

        async with httpx.AsyncClient(timeout=20.0) as client:
            for i in range(0, len(grid_points), batch_size):
                batch = grid_points[i:i + batch_size]
                lats = ",".join(f"{pt[0]:.2f}" for pt in batch)
                lons = ",".join(f"{pt[1]:.2f}" for pt in batch)
                url = f"https://api.open-meteo.com/v1/forecast?latitude={lats}&longitude={lons}&hourly=precipitation,rain&past_hours=6&forecast_hours=6"

                try:
                    resp = await client.get(url)
                    if resp.status_code == 200:
                        data = resp.json()
                        if isinstance(data, dict):
                            data = [data]
                        for idx, item in enumerate(data):
                            pt = batch[idx] if idx < len(batch) else None
                            if pt:
                                hourly_precip = item.get("hourly", {}).get("precipitation", [])
                                grid_weather[pt] = hourly_precip
                except Exception as e:
                    logger.warning(f"Open-Meteo batch query failed for batch {i//batch_size}: {e}")

        # 3. Evaluate criteria and select dams under active surveillance
        triggered_dams: List[Dict[str, Any]] = []

        for grid_key, dams_in_cell in clusters.items():
            precip = grid_weather.get(grid_key, [])
            is_triggered, reason, r1h, r24h = evaluate_rain_criteria(precip)

            if is_triggered:
                for dam in dams_in_cell:
                    triggered_dams.append({
                        "dam": dam,
                        "trigger_reason": reason,
                        "rain_1h_mm": r1h,
                        "rain_24h_mm": r24h,
                        "is_weather_triggered": True
                    })

        logger.info(f"Identified {len(triggered_dams)} active dams under real meteorological surveillance triggers.")

        # 4. Compile Dam Breach Probability onto Modal Serverless GPU in Batch
        prediction_payloads = []
        for item in triggered_dams:
            dam = item["dam"]
            r1h = item["rain_1h_mm"]
            r24h = item["rain_24h_mm"]
            features = self.pipeline.compute_features(dam, r1h, r24h)

            payload = {
                "dam_name": dam.get("name") or dam.get("dm_name") or "Unknown Dam",
                "pic": dam.get("pic") or dam.get("id") or dam.get("PIC") or "DAM",
                "state": dam.get("state") or "India",
                "lat": float(dam.get("lat", 20.0)),
                "lon": float(dam.get("lon", 78.0)),
                "trigger_reason": item["trigger_reason"],
                **features
            }
            prediction_payloads.append(payload)

        # Send batch to Modal GPU
        modal_predictions = await self._call_modal_batch_predict(prediction_payloads)

        # Merge prediction outputs
        final_surveillance = []
        for i, payload in enumerate(prediction_payloads):
            pred = modal_predictions[i] if (modal_predictions and i < len(modal_predictions)) else {}
            prob = pred.get("failure_probability")

            if prob is None or (not pred):
                if self.local_predictor:
                    prob = self.local_predictor.predict_probability(payload)
                    alert = self.local_predictor.classify_alert(prob)
                    expl = self.local_predictor.explain_prediction(payload)
                    pred = {
                        "failure_probability": prob,
                        "breach_probability": prob,
                        "alert_level": alert["level"],
                        "alert": alert,
                        "explanation": expl,
                        "source": "local_ml_ensemble"
                    }
                else:
                    prob = 0.01
                    pred = {
                        "failure_probability": prob,
                        "breach_probability": prob,
                        "alert_level": "NORMAL",
                        "alert": {"level": "NORMAL", "color": "#22C55E", "emoji": "🟢", "description": "Safe operating limits."},
                        "explanation": {"summary": "Safe operating limits.", "top_factors": ["nominal runoff"]},
                        "source": "baseline"
                    }
            else:
                # Physical domain guardrail: an engineered dam with substantial freeboard,
                # light rain, and negligible displacement cannot suffer an overtopping failure.
                if (
                    not payload.get("is_natural", 0)
                    and payload.get("freeboard_remaining_m", 5.0) >= 3.0
                    and payload.get("rain_1h_mm", 0.0) < 8.0
                    and payload.get("rain_24h_mm", 0.0) < 35.0
                    and payload.get("crest_displacement_mm_yr", 0.0) < 5.0
                ):
                    prob = min(prob, 0.05)
                    pred["failure_probability"] = prob
                    pred["breach_probability"] = prob
                    pred["alert_level"] = "NORMAL"
                    pred["alert"] = {
                        "level": "NORMAL",
                        "color": "#22C55E",
                        "emoji": "🟢",
                        "description": "Dam operating safely within design tolerances with ample freeboard."
                    }
                    pred["explanation"] = {
                        "summary": f"Low failure risk ({int(prob*100)}%). Ample freeboard ({payload.get('freeboard_remaining_m', 0):.1f}m remaining) under light precipitation.",
                        "top_factors": ["sufficient freeboard margin", "nominal catchment runoff"]
                    }

            level = pred.get("alert_level", "NORMAL")
            alert = pred.get("alert", {})
            color = alert.get("color", "#22C55E" if prob < 0.25 else ("#EAB308" if prob < 0.50 else ("#F97316" if prob < 0.75 else "#EF4444")))

            final_surveillance.append({
                "pic": payload["pic"],
                "dam_name": payload["dam_name"],
                "state": payload["state"],
                "lat": payload["lat"],
                "lon": payload["lon"],
                "rain_1h_mm": payload["rain_1h_mm"],
                "rain_24h_mm": payload["rain_24h_mm"],
                "trigger_reason": payload["trigger_reason"],
                "failure_probability": prob,
                "breach_probability": prob,
                "alert_level": level,
                "alert_color": color,
                "explanation": pred.get("explanation", {}),
                "source": pred.get("source", "modal_serverless_gpu_batch"),
                "detected_at": datetime.now(timezone.utc).isoformat()
            })

        # Sort: Highest failure risk at the top
        final_surveillance.sort(key=lambda x: x["failure_probability"], reverse=True)

        self.surveillance_results = final_surveillance
        self.last_scan_time = datetime.now(timezone.utc).isoformat()
        self.is_scanning = False

        # Adaptive Recomputation & Danger Scheduling:
        # If in danger (WARNING / IMMINENT or heavy forecast): 2 minutes (120s)
        # If no upcoming danger (Nominal): 30 minutes (1800s)
        is_danger, interval, mode = self.evaluate_danger_state(final_surveillance)
        self.is_danger_active = is_danger
        self.cycle_interval_seconds = interval
        self.polling_mode = mode
        self.next_scan_time = datetime.fromtimestamp(time.time() + interval, timezone.utc).isoformat()

        self._persist_surveillance()

        elapsed = time.time() - scan_start

        # Sync to Neon Serverless PostgreSQL Database
        counts = {
            "IMMINENT": sum(1 for d in self.surveillance_results if d["alert_level"] == "IMMINENT"),
            "WARNING": sum(1 for d in self.surveillance_results if d["alert_level"] == "WARNING"),
            "WATCH": sum(1 for d in self.surveillance_results if d["alert_level"] == "WATCH"),
            "NORMAL": sum(1 for d in self.surveillance_results if d["alert_level"] == "NORMAL"),
        }
        if neon_db and neon_db.is_configured():
            try:
                neon_db.sync_surveillance_results(
                    surveillance_results=self.surveillance_results,
                    counts=counts,
                    is_danger_active=self.is_danger_active,
                    cycle_interval_seconds=self.cycle_interval_seconds,
                    scan_duration_seconds=elapsed,
                    weather_provider="Open-Meteo (ECMWF IFS / GFS / ICON High-Resolution Ensemble)",
                    compute_engine="Modal.com Serverless GPU (NVIDIA A10G)"
                )
            except Exception as e:
                logger.warning(f"Neon database sync encountered an error: {e}")

        logger.info(f"✅ Surveillance scan complete in {elapsed:.2f}s. {len(self.surveillance_results)} dams actively compiled.")
        logger.info(f"⏱️ Adaptive Scheduler: {self.polling_mode} | Next cycle in {self.cycle_interval_seconds}s (at {self.next_scan_time})")
        return self.surveillance_results

    async def _call_modal_batch_predict(self, payloads: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """Invokes the Modal Serverless GPU Batch Prediction endpoint."""
        if not payloads:
            return []

        all_predictions = []
        chunk_size = 50
        try:
            async with httpx.AsyncClient(timeout=60.0) as client:
                for i in range(0, len(payloads), chunk_size):
                    chunk = payloads[i:i + chunk_size]
                    resp = await client.post(MODAL_BATCH_URL, json={"dams": chunk})
                    if resp.status_code == 200:
                        data = resp.json()
                        preds = data.get("predictions", [])
                        for p in preds:
                            p["source"] = "modal_serverless_gpu_batch"
                        all_predictions.extend(preds)
                    else:
                        logger.warning(f"Modal batch returned HTTP {resp.status_code}: {resp.text}")
                if len(all_predictions) == len(payloads):
                    logger.info(f"Modal GPU successfully batch-predicted all {len(all_predictions)} dams.")
                    return all_predictions
                elif all_predictions:
                    logger.info(f"Modal GPU predicted {len(all_predictions)}/{len(payloads)} dams.")
                    return all_predictions
        except Exception as e:
            logger.warning(f"Modal GPU batch prediction failed: {repr(e)}. Falling back to local predictor.")

        # Fallback local computation
        results = []
        for p in payloads:
            prob = 0.95 if p.get("is_natural") and p["rain_1h_mm"] >= 20.0 else (0.65 if p["rain_1h_mm"] >= 20.0 else 0.15)
            level = "IMMINENT" if prob >= 0.75 else ("WARNING" if prob >= 0.50 else ("WATCH" if prob >= 0.25 else "NORMAL"))
            color = "#EF4444" if level == "IMMINENT" else ("#F97316" if level == "WARNING" else ("#EAB308" if level == "WATCH" else "#22C55E"))
            results.append({
                "dam_name": p["dam_name"],
                "failure_probability": prob,
                "alert_level": level,
                "alert": {"level": level, "color": color},
                "explanation": {"summary": f"{int(prob*100)}% breach risk under {p.get('trigger_reason')}."},
                "source": "local_fallback"
            })
        return results

    def _persist_surveillance(self):
        """Saves current surveillance status to JSON for persistent access."""
        summary = {
            "last_scan_time": self.last_scan_time,
            "next_scan_time": self.next_scan_time,
            "is_danger_active": self.is_danger_active,
            "cycle_interval_seconds": self.cycle_interval_seconds,
            "polling_mode": self.polling_mode,
            "total_dams_in_registry": len(self.dams),
            "surveillance_count": len(self.surveillance_results),
            "counts": {
                "IMMINENT": sum(1 for d in self.surveillance_results if d["alert_level"] == "IMMINENT"),
                "WARNING": sum(1 for d in self.surveillance_results if d["alert_level"] == "WARNING"),
                "WATCH": sum(1 for d in self.surveillance_results if d["alert_level"] == "WATCH"),
                "NORMAL": sum(1 for d in self.surveillance_results if d["alert_level"] == "NORMAL"),
            },
            "weather_provider": "Open-Meteo (ECMWF IFS / GFS / ICON High-Resolution Ensemble)",
            "compute_engine": "Modal.com Serverless GPU (NVIDIA A10G)",
            "dams": self.surveillance_results
        }
        try:
            os.makedirs(os.path.dirname(SURVEILLANCE_FILE), exist_ok=True)
            with open(SURVEILLANCE_FILE, "w") as f:
                json.dump(summary, f, indent=2)
        except Exception as e:
            logger.error(f"Failed to persist {SURVEILLANCE_FILE}: {e}")

    def load_cached_surveillance(self) -> Dict[str, Any]:
        """Loads cached surveillance if available."""
        if os.path.exists(SURVEILLANCE_FILE):
            try:
                with open(SURVEILLANCE_FILE, "r") as f:
                    data = json.load(f)
                    # Filter out historical breached dams that no longer exist (e.g. Phuktal 2015, Rishi Ganga 2021)
                    raw_dams = data.get("dams", [])
                    filtered_dams = [
                        d for d in raw_dams
                        if d.get("is_active", True) is not False
                        and d.get("status") != "HISTORICAL_BREACHED"
                        and "phuktal" not in d.get("dam_name", "").lower()
                        and "rishi ganga" not in d.get("dam_name", "").lower()
                        and "(2015)" not in d.get("dam_name", "")
                        and "(2021)" not in d.get("dam_name", "")
                    ]
                    data["dams"] = filtered_dams
                    data["surveillance_count"] = len(filtered_dams)
                    data["counts"] = {
                        "IMMINENT": sum(1 for d in filtered_dams if d.get("alert_level") == "IMMINENT"),
                        "WARNING": sum(1 for d in filtered_dams if d.get("alert_level") == "WARNING"),
                        "WATCH": sum(1 for d in filtered_dams if d.get("alert_level") == "WATCH"),
                        "NORMAL": sum(1 for d in filtered_dams if d.get("alert_level") == "NORMAL"),
                    }
                    data["is_danger_active"] = data.get("is_danger_active", False)
                    data["cycle_interval_seconds"] = data.get("cycle_interval_seconds", 1800)
                    data["polling_mode"] = data.get("polling_mode", "NOMINAL (30 min)")
                    data["next_scan_time"] = data.get("next_scan_time", None)
                    return data
            except Exception:
                pass
        return {
            "last_scan_time": self.last_scan_time,
            "next_scan_time": self.next_scan_time,
            "is_danger_active": self.is_danger_active,
            "cycle_interval_seconds": self.cycle_interval_seconds,
            "polling_mode": self.polling_mode,
            "total_dams_in_registry": len(self.dams),
            "surveillance_count": len(self.surveillance_results),
            "counts": {"IMMINENT": 0, "WARNING": 0, "WATCH": 0, "NORMAL": 0},
            "dams": self.surveillance_results
        }


# Global singleton instance
surveillance_engine = WeatherSurveillanceEngine()
