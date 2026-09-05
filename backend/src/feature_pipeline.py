"""
HydroGuard-AI: Layer 3 — Feature Pipeline

Assembles the 14-dimensional feature vector for each at-risk dam.
Computes physics-informed derived features:
    - Overtopping Risk Index (ORI)
    - Stefanelli Dam Breach Index (DBI)
    - Peak inflow surge via Rational Method
"""

import numpy as np
import pandas as pd
from typing import List, Optional


# Structural type to numeric code mapping
STRUCTURAL_TYPE_MAP = {
    "Earthfill": 0,
    "Rockfill": 1,
    "Gravity Concrete": 2,
    "Masonry": 3,
    "Landslide Debris": 4,
}

# Runoff coefficients by structural type (proxy for terrain)
RUNOFF_COEFFICIENTS = {
    "Earthfill": 0.55,
    "Rockfill": 0.50,
    "Gravity Concrete": 0.50,
    "Masonry": 0.52,
    "Landslide Debris": 0.75,
}


def _clean_val(data_dict: dict, key: str, default: float) -> float:
    """Extract float from dictionary, falling back to default if None or NaN."""
    val = data_dict.get(key)
    if val is None:
        return default
    try:
        if np.isnan(val):
            return default
        return float(val)
    except (TypeError, ValueError):
        return default


class FeaturePipeline:
    """
    Feature engineering pipeline for dam breach prediction.

    Transforms raw dam parameters and weather data into the
    14-dimensional feature matrix required by the ML model.
    """

    def compute_features(self, dam_data: dict, rain_1h: float, rain_24h: float) -> dict:
        """
        Compute the full 14-feature vector for a single dam.
        """
        is_natural = dam_data.get("is_natural", False)
        struct_type = dam_data.get("structural_type", "Earthfill")
        if not isinstance(struct_type, str) or pd.isna(struct_type):
            struct_type = "Earthfill"

        # ── Safe parameter extraction with NaN fallback ──
        catchment = _clean_val(dam_data, "catchment_area_sqkm", 100.0)
        height = max(_clean_val(dam_data, "dam_height_m", _clean_val(dam_data, "height_m", 20.0)), 1.0)
        crest_len = _clean_val(dam_data, "crest_length_m", 200.0)
        storage = max(_clean_val(dam_data, "current_storage_mcm", _clean_val(dam_data, "max_storage_mcm", 5.0)), 0.1)
        spillway = _clean_val(dam_data, "spillway_capacity_cumec", 500.0)
        # Government registries (CWC/NRLD) often record nominal irrigation sluice discharge (e.g. 0.9 m³/s)
        # for minor tanks instead of the emergency broad-crested waste weir.
        # Under IS 11223, every engineered dam has emergency weir overflow capacity >= 15.0 m³/s.
        if not is_natural and spillway < 10.0 and height >= 10.0:
            spillway = max(spillway, 15.0)

        freeboard = max(_clean_val(dam_data, "freeboard_remaining_m", _clean_val(dam_data, "freeboard_m", 2.0)), 0.05)
        crest_disp = _clean_val(dam_data, "crest_displacement_mm_yr", 1.0)
        age = int(_clean_val(dam_data, "dam_age_years", 30))

        # ── 1. Peak Inflow Surge (Rational Method with Infiltration & Abstraction) ──
        c_runoff = RUNOFF_COEFFICIENTS.get(struct_type.title(), 0.55)
        # Light rainfall (< 5 mm/hr) is largely absorbed by soil infiltration (5-20 mm/hr) and depression storage.
        if rain_1h <= 1.0:
            q_in = 0.0
        elif rain_1h < 8.0:
            rain_eff = max(0.0, rain_1h - 1.0)
            c_eff = c_runoff * min(1.0, max(0.05, (rain_1h - 1.0) / 7.0))
            q_in = 0.278 * c_eff * rain_eff * catchment
        else:
            q_in = 0.278 * c_runoff * rain_1h * catchment

        # ── 2. Overtopping Risk Index (ORI) ──
        res_area = max(storage * 1e6 / height, 10000.0)
        net_flow = max(0.0, q_in - spillway)
        ori = net_flow / (res_area * freeboard)

        # ── 3. Stefanelli Dam Breach Index (DBI) ──
        if is_natural:
            vol_barrier = _clean_val(dam_data, "barrier_volume_m3", 1e6)
            vol_lake = max(storage * 1e6, 1.0)
            slope = _clean_val(dam_data, "river_slope", 0.04)
            dbi = np.log10(max(vol_barrier / (vol_lake * slope), 1e-4))
        else:
            dbi = 3.50  # Engineered dams: high nominal geometric resistance

        # ── Assemble Feature Vector ──
        features = {
            "dam_height_m": height,
            "crest_length_m": crest_len,
            "catchment_area_sqkm": catchment,
            "current_storage_mcm": storage,
            "freeboard_remaining_m": freeboard,
            "spillway_capacity_cumec": spillway,
            "rain_1h_mm": float(rain_1h),
            "rain_24h_mm": float(rain_24h),
            "inflow_surge_cumec": round(q_in, 2),
            "crest_displacement_mm_yr": crest_disp,
            "structural_type_code": STRUCTURAL_TYPE_MAP.get(struct_type.title(), 0),
            "is_natural": int(is_natural),
            "dam_age_years": age,
            "overtopping_risk_index": round(ori, 8),
            "dam_breach_index": round(dbi, 4),
        }

        return features

    def compute_batch(
        self,
        at_risk_dams: list,
    ) -> pd.DataFrame:
        """
        Compute features for a batch of at-risk dams.

        Args:
            at_risk_dams: List of AtRiskDam objects from SpatialCatchmentEngine

        Returns:
            DataFrame with features for all dams, ready for ML inference
        """
        records = []

        for dam_obj in at_risk_dams:
            features = self.compute_features(
                dam_data=dam_obj.dam_data,
                rain_1h=dam_obj.rain_1h_mm,
                rain_24h=dam_obj.rain_24h_mm,
            )
            # Add metadata for tracking
            features["_dam_id"] = dam_obj.dam_id
            features["_dam_name"] = dam_obj.name
            features["_lat"] = dam_obj.lat
            features["_lon"] = dam_obj.lon
            features["_distance_km"] = dam_obj.distance_km
            records.append(features)

        df = pd.DataFrame(records)
        print(f"[FEATURES] 🧮 Computed {len(df)} feature vectors")

        return df

    def get_feature_columns(self) -> list:
        """Return the ordered list of ML feature column names."""
        return [
            "dam_height_m", "crest_length_m", "catchment_area_sqkm",
            "current_storage_mcm", "freeboard_remaining_m", "spillway_capacity_cumec",
            "rain_1h_mm", "rain_24h_mm", "inflow_surge_cumec",
            "crest_displacement_mm_yr", "structural_type_code", "is_natural",
            "dam_age_years", "overtopping_risk_index", "dam_breach_index",
        ]

    def get_feature_descriptions(self) -> dict:
        """Return human-readable descriptions for each feature."""
        return {
            "dam_height_m": "Dam Height (m)",
            "crest_length_m": "Crest Length (m)",
            "catchment_area_sqkm": "Catchment Area (km²)",
            "current_storage_mcm": "Current Storage (MCM)",
            "freeboard_remaining_m": "Freeboard Remaining (m)",
            "spillway_capacity_cumec": "Spillway Capacity (m³/s)",
            "rain_1h_mm": "Rainfall - Last 1 Hour (mm)",
            "rain_24h_mm": "Rainfall - Last 24 Hours (mm)",
            "inflow_surge_cumec": "Inflow Surge Q_in (m³/s)",
            "crest_displacement_mm_yr": "Crest Displacement (mm/yr)",
            "structural_type_code": "Structural Type Code",
            "is_natural": "Is Natural Dam (0/1)",
            "dam_age_years": "Dam Age (years)",
            "overtopping_risk_index": "Overtopping Risk Index (ORI)",
            "dam_breach_index": "Dam Breach Index (DBI)",
        }

    @staticmethod
    def interpret_dbi(dbi: float) -> str:
        """Interpret the Stefanelli DBI stability classification."""
        if dbi < 2.75:
            return "UNSTABLE — High probability of rapid breaching"
        elif dbi <= 3.08:
            return "MODERATELY STABLE — Monitor closely"
        else:
            return "STABLE — Quasi-permanent barrier"

    @staticmethod
    def interpret_ori(ori: float) -> str:
        """Interpret the Overtopping Risk Index."""
        if ori > 0.001:
            return "CRITICAL — Active overtopping risk, freeboard depleting rapidly"
        elif ori > 0:
            return "ELEVATED — Net inflow exceeding spillway capacity"
        elif ori > -0.0001:
            return "MARGINAL — Near equilibrium, monitor closely"
        else:
            return "SAFE — Spillway capacity handles inflow with margin"
