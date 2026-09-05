"""
HydroGuard-AI: Real-World Stress Test Suite

Evaluates the trained ML ensemble and feature pipeline across 15 real-world historical
disasters, operational benchmarks, and extreme hydrological corner cases.
"""

import os
import sys
import json
import time
import pandas as pd
import numpy as np

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, PROJECT_ROOT)

from src.predictor import DamBreachPredictor
from src.spatial_engine import SpatialCatchmentEngine
from src.feature_pipeline import FeaturePipeline


def run_stress_tests():
    print("=" * 100)
    print("           HYDROGUARD-AI: EXPANDED REAL-WORLD STRESS & ACCURACY TEST SUITE")
    print("=" * 100 + "\n")

    predictor = DamBreachPredictor(model_dir=os.path.join(PROJECT_ROOT, "models"))
    engine = SpatialCatchmentEngine(dam_registry_path=os.path.join(PROJECT_ROOT, "data", "india_dams_seed.json"))
    pipeline = FeaturePipeline()

    dams = engine.get_all_dams()

    # 15 Benchmark Scenarios: Real-World Disasters, Monsoons, & Boundary Corner Cases
    scenarios = [
        {
            "category": "Historical Disaster",
            "name": "Rishi Ganga Landslide Dam (2021)",
            "query": "rishi ganga",
            "rain_1h": 35.0, "rain_24h": 120.0,
            "custom_dam": None,
            "expected": ["IMMINENT", "WARNING"],
            "real_outcome": "BREACHED (Feb 2021 - 200+ casualties in Chamoli)",
        },
        {
            "category": "Historical Disaster",
            "name": "Phuktal River Landslide Dam (2015)",
            "query": "phuktal",
            "rain_1h": 22.0, "rain_24h": 85.0,
            "custom_dam": None,
            "expected": ["IMMINENT", "WARNING"],
            "real_outcome": "BREACHED (May 2015 - Destroyed 12 bridges in Zanskar)",
        },
        {
            "category": "Historical Disaster",
            "name": "Teesta III Dam / Chungthang GLOF (2023)",
            "query": None,
            "rain_1h": 60.0, "rain_24h": 250.0,
            "custom_dam": {
                "name": "Teesta III (Chungthang)",
                "height_m": 60.0, "crest_length_m": 430.0,
                "catchment_area_sqkm": 2800.0, "current_storage_mcm": 50.0,
                "freeboard_m": 0.2, "spillway_capacity_cumec": 3000.0,
                "structural_type": "Concrete Gravity", "dam_age_years": 8,
                "is_natural": False, "crest_displacement_mm_yr": 2.0
            },
            "expected": ["IMMINENT", "WARNING"],
            "real_outcome": "BREACHED (Oct 2023 - South Lhonak GLOF destroyed dam)",
        },
        {
            "category": "Historical Disaster",
            "name": "Machhu II Dam Collapse (1979)",
            "query": None,
            "rain_1h": 90.0, "rain_24h": 400.0,
            "custom_dam": {
                "name": "Machhu II (Morbi 1979)",
                "height_m": 26.0, "crest_length_m": 4350.0,
                "catchment_area_sqkm": 1900.0, "current_storage_mcm": 110.0,
                "freeboard_m": 0.2, "spillway_capacity_cumec": 6000.0,
                "structural_type": "Composite", "dam_age_years": 7,
                "is_natural": False, "crest_displacement_mm_yr": 1.5
            },
            "expected": ["IMMINENT", "WARNING"],
            "real_outcome": "BREACHED (Aug 1979 - Morbi catastrophic overtopping)",
        },
        {
            "category": "Historical Disaster",
            "name": "Cheyyeru / Annamayya Dam (2021)",
            "query": "cheyyeru",
            "rain_1h": 85.0, "rain_24h": 380.0,
            "custom_dam": {
                "name": "Cheyyeru Project (Annamayya)",
                "height_m": 25.0, "crest_length_m": 250.0,
                "catchment_area_sqkm": 627.0, "current_storage_mcm": 63.47,
                "freeboard_m": 0.1, "spillway_capacity_cumec": 2170.0,
                "structural_type": "Earthfill", "dam_age_years": 35,
                "is_natural": False, "crest_displacement_mm_yr": 1.2
            },
            "expected": ["IMMINENT", "WARNING"],
            "real_outcome": "BREACHED (Nov 2021 - Cyclone Jawad 5L cusec surge)",
        },
        {
            "category": "Recent Critical Surge",
            "name": "Prakasam Barrage Extreme Flood (2024)",
            "query": "prakasam",
            "rain_1h": 70.0, "rain_24h": 250.0,
            "custom_dam": None,
            "expected": ["IMMINENT", "WARNING"],
            "real_outcome": "SEVERE DAMAGE (Sept 2024 - 11.4L cusecs surge & boat hit)",
        },
        {
            "category": "Recent Critical Surge",
            "name": "Kadam Dam Overtopping Threat (2022/23)",
            "query": "kadam",
            "rain_1h": 65.0, "rain_24h": 300.0,
            "custom_dam": {
                "name": "Kadam Dam",
                "height_m": 23.0, "crest_length_m": 2100.0,
                "catchment_area_sqkm": 2590.0, "current_storage_mcm": 140.0,
                "freeboard_m": 0.3, "spillway_capacity_cumec": 8500.0,
                "structural_type": "Composite", "dam_age_years": 58,
                "is_natural": False, "crest_displacement_mm_yr": 3.5
            },
            "expected": ["IMMINENT", "WARNING"],
            "real_outcome": "CRITICAL OVERTOPPING THREAT (Inflow 5.1L cusecs > capacity)",
        },
        {
            "category": "High Monsoon Alert",
            "name": "Mullaperiyar Dam High Rain Scenario",
            "query": "mulla",
            "rain_1h": 65.0, "rain_24h": 280.0,
            "custom_dam": {
                "name": "Mullaperiyar Dam",
                "height_m": 53.6, "crest_length_m": 365.0,
                "catchment_area_sqkm": 643.0, "current_storage_mcm": 443.0,
                "freeboard_m": 0.3, "spillway_capacity_cumec": 3450.0,
                "structural_type": "Masonry", "dam_age_years": 131,
                "is_natural": False, "crest_displacement_mm_yr": 2.5
            },
            "expected": ["IMMINENT", "WARNING"],
            "real_outcome": "HIGH ALERT (131-yr masonry dam under extreme rain)",
        },
        {
            "category": "Operational Normal",
            "name": "Tehri Dam (Moderate Monsoon)",
            "query": "tehri hpp",
            "rain_1h": 8.0, "rain_24h": 25.0,
            "custom_dam": None,
            "expected": ["WATCH", "NORMAL"],
            "real_outcome": "SAFE / OPERATIONAL (High rockfill dam with 15k spillway)",
        },
        {
            "category": "Operational Normal",
            "name": "Bhakra Dam (Moderate Monsoon)",
            "query": None,
            "rain_1h": 10.0, "rain_24h": 30.0,
            "custom_dam": {
                "name": "Bhakra Dam (Concrete Gravity)",
                "height_m": 225.55, "crest_length_m": 518.0,
                "catchment_area_sqkm": 56980.0, "current_storage_mcm": 9868.0,
                "freeboard_m": 5.0, "spillway_capacity_cumec": 8500.0,
                "structural_type": "Gravity Concrete", "dam_age_years": 63,
                "is_natural": False, "crest_displacement_mm_yr": 0.5
            },
            "expected": ["WATCH", "NORMAL"],
            "real_outcome": "SAFE / OPERATIONAL (Managed BBMB storage)",
        },
        {
            "category": "Operational Normal",
            "name": "Bhale Dam (Minor Tank in Light Rain)",
            "query": "bhale",
            "rain_1h": 2.8, "rain_24h": 11.3,
            "custom_dam": None,
            "expected": ["NORMAL"],
            "real_outcome": "SAFE / NORMAL (16m earthen tank with 4.8m freeboard under drizzle)",
        },
        {
            "category": "Corner Case",
            "name": "Drought / Zero Rainfall Scenario",
            "query": "tehri hpp",
            "rain_1h": 0.0, "rain_24h": 0.0,
            "custom_dam": None,
            "expected": ["NORMAL"],
            "real_outcome": "SAFE (Zero hydrologic stress)",
        },
        {
            "category": "Corner Case",
            "name": "Severe Freeboard Depletion (<0.05m)",
            "query": None,
            "rain_1h": 50.0, "rain_24h": 200.0,
            "custom_dam": {
                "name": "Overtopping Depleted Dam",
                "height_m": 30.0, "crest_length_m": 400.0,
                "catchment_area_sqkm": 1500.0, "current_storage_mcm": 150.0,
                "freeboard_m": 0.05, "spillway_capacity_cumec": 1000.0,
                "structural_type": "Earthfill", "dam_age_years": 45,
                "is_natural": False, "crest_displacement_mm_yr": 5.0
            },
            "expected": ["IMMINENT"],
            "real_outcome": "IMMINENT (Freeboard exhausted)",
        },
        {
            "category": "Corner Case",
            "name": "Unstable Landslide Dam (Low DBI < 2.0)",
            "query": None,
            "rain_1h": 30.0, "rain_24h": 100.0,
            "custom_dam": {
                "name": "Unstable Debris Dam",
                "height_m": 40.0, "crest_length_m": 150.0,
                "catchment_area_sqkm": 800.0, "current_storage_mcm": 25.0,
                "freeboard_m": 0.5, "spillway_capacity_cumec": 0.0,
                "structural_type": "Landslide Debris", "dam_age_years": 0,
                "is_natural": True, "barrier_volume_m3": 5e5,
                "river_slope": 0.06, "crest_displacement_mm_yr": 15.0
            },
            "expected": ["IMMINENT"],
            "real_outcome": "IMMINENT (High natural breach index)",
        },
        {
            "category": "Corner Case",
            "name": "Aging Dam (140 yrs) + Structural Displacement (35mm/yr)",
            "query": None,
            "rain_1h": 40.0, "rain_24h": 160.0,
            "custom_dam": {
                "name": "Old Deforming Dam",
                "height_m": 35.0, "crest_length_m": 600.0,
                "catchment_area_sqkm": 2000.0, "current_storage_mcm": 200.0,
                "freeboard_m": 1.0, "spillway_capacity_cumec": 2500.0,
                "structural_type": "Masonry", "dam_age_years": 140,
                "is_natural": False, "crest_displacement_mm_yr": 35.0
            },
            "expected": ["IMMINENT", "WARNING"],
            "real_outcome": "IMMINENT / WARNING (High structural displacement)",
        },
        {
            "category": "Corner Case",
            "name": "Mega Spillway Safety Margin (Q_spill >> Q_in)",
            "query": None,
            "rain_1h": 20.0, "rain_24h": 60.0,
            "custom_dam": {
                "name": "Super Spillway Dam",
                "height_m": 100.0, "crest_length_m": 1200.0,
                "catchment_area_sqkm": 1000.0, "current_storage_mcm": 500.0,
                "freeboard_m": 6.0, "spillway_capacity_cumec": 25000.0,
                "structural_type": "Gravity Concrete", "dam_age_years": 15,
                "is_natural": False, "crest_displacement_mm_yr": 0.1
            },
            "expected": ["NORMAL"],
            "real_outcome": "SAFE (Huge spillway margin)",
        }
    ]

    results = []
    passed = 0
    t0 = time.time()

    for s in scenarios:
        if s["custom_dam"]:
            dam_data = s["custom_dam"]
            dam_name = dam_data["name"]
        else:
            dam_data = next((d for d in dams if s["query"].lower() in d["name"].lower()), None)
            dam_name = dam_data["name"] if dam_data else s["name"]

        features = pipeline.compute_features(dam_data, rain_1h=s["rain_1h"], rain_24h=s["rain_24h"])
        prob = predictor.predict_probability(features)
        alert = predictor.classify_alert(prob)

        is_match = alert["level"] in s["expected"]
        if is_match:
            passed += 1

        status_str = "✅ PASS" if is_match else "❌ FAIL"

        results.append({
            "Status": status_str,
            "Category": s["category"],
            "Scenario Name": dam_name,
            "P(Breach)": f"{prob*100:5.1f}%",
            "Alert Level": alert["level"],
            "Expected": "/".join(s["expected"]),
            "Real Event / Physical Reality": s["real_outcome"],
        })

    inference_time = (time.time() - t0) * 1000 / len(scenarios)

    df_results = pd.DataFrame(results)

    print(df_results.to_string(index=False))

    acc = (passed / len(scenarios)) * 100
    print("\n" + "=" * 100)
    print(f"🎯 TEST SUITE ACCURACY: {acc:.1f}% ({passed}/{len(scenarios)} Scenarios Passed)")
    print(f"⚡ AVERAGE INFERENCE LATENCY: {inference_time:.2f} ms per dam")
    print("=" * 100 + "\n")


if __name__ == "__main__":
    run_stress_tests()
