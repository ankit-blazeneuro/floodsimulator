"""
HydroGuard-AI: Layer 4 — Dam Breach Predictor

ML inference engine with dual-mode operation:
    1. ML Mode: Loads trained XGBoost ensemble + isotonic calibrator
    2. Heuristic Fallback: Calibrated sigmoid boundary for offline use

Includes SHAP-based explainability and alert classification.
"""

import os
import numpy as np
import pandas as pd
import joblib
from typing import Optional, Tuple


# ─── Alert Level Thresholds ───────────────────────────────────────────────

ALERT_LEVELS = {
    "NORMAL":    {"min": 0.00, "max": 0.25, "color": "#22c55e", "emoji": "🟢"},
    "WATCH":     {"min": 0.25, "max": 0.50, "color": "#eab308", "emoji": "🟡"},
    "WARNING":   {"min": 0.50, "max": 0.75, "color": "#f97316", "emoji": "🟠"},
    "IMMINENT":  {"min": 0.75, "max": 1.00, "color": "#ef4444", "emoji": "🔴"},
}

# Feature column order (must match training)
FEATURE_COLS = [
    "dam_height_m", "crest_length_m", "catchment_area_sqkm",
    "current_storage_mcm", "freeboard_remaining_m", "spillway_capacity_cumec",
    "rain_1h_mm", "rain_24h_mm", "inflow_surge_cumec",
    "crest_displacement_mm_yr", "structural_type_code", "is_natural",
    "dam_age_years", "overtopping_risk_index", "dam_breach_index",
]


class DamBreachPredictor:
    """
    Dam breach probability prediction engine.

    Supports trained ML model or calibrated heuristic fallback.
    Provides SHAP-based feature attribution for explainability.
    """

    def __init__(self, model_dir: str = "models"):
        self.model_dir = model_dir
        self.model = None
        self.calibrator = None
        self.explainer = None
        self._mode = "heuristic"

        self._try_load_model()

    def _try_load_model(self):
        """Attempt to load trained model artifacts."""
        model_path = os.path.join(self.model_dir, "ensemble_model.pkl")
        cal_path = os.path.join(self.model_dir, "calibrator.pkl")

        # Try ensemble first, then individual XGBoost
        for path in [model_path, os.path.join(self.model_dir, "dam_failure_xgb.pkl")]:
            if os.path.exists(path):
                try:
                    self.model = joblib.load(path)
                    self._mode = "ml"
                    print(f"[PREDICTOR] 🤖 Loaded ML model from {path}")
                    break
                except Exception as e:
                    print(f"[PREDICTOR] ⚠️  Failed to load {path}: {e}")

        if os.path.exists(cal_path):
            try:
                self.calibrator = joblib.load(cal_path)
                print(f"[PREDICTOR] 📊 Loaded probability calibrator")
            except Exception as e:
                print(f"[PREDICTOR] ⚠️  Failed to load calibrator: {e}")

        if self._mode == "heuristic":
            print("[PREDICTOR] 📐 Using heuristic fallback (no trained model found)")

    def predict_probability(self, feature_dict: dict) -> float:
        """
        Predict breach probability for a single dam.

        Args:
            feature_dict: Dictionary with all 14 feature values

        Returns:
            Calibrated breach probability P(breach) ∈ [0, 1]
        """
        if self._mode == "ml" and self.model is not None:
            return self._predict_ml(feature_dict)
        else:
            return self._predict_heuristic(feature_dict)

    def _predict_ml(self, feature_dict: dict) -> float:
        """Predict using trained ML model."""
        df = pd.DataFrame([{col: feature_dict.get(col, 0.0) for col in FEATURE_COLS}])

        try:
            prob = self.model.predict_proba(df)[0, 1]

            # Apply isotonic calibration if available
            if self.calibrator is not None:
                prob = self.calibrator.predict([prob])[0]

            return float(np.clip(prob, 0.0, 1.0))

        except Exception as e:
            print(f"[PREDICTOR] ⚠️  ML prediction failed, falling back to heuristic: {e}")
            return self._predict_heuristic(feature_dict)

    def _predict_heuristic(self, feature_dict: dict) -> float:
        """
        Calibrated sigmoid heuristic for offline/no-model operation.

        Uses domain-informed weights calibrated against known case studies:
        - Rishi Ganga 2021 → should produce P ≈ 0.94
        - Phuktal 2015     → should produce P ≈ 0.81
        - Tehri (nominal)  → should produce P ≈ 0.08
        """
        f = feature_dict

        # Normalize inflow relative to spillway capacity to avoid
        # large-catchment dams (e.g., Tehri) dominating the score.
        q_in = f.get("inflow_surge_cumec", 0)
        q_spill = max(f.get("spillway_capacity_cumec", 1), 1)
        inflow_ratio = max(q_in - q_spill, 0) / q_spill  # 0 when spillway handles it

        fb = f.get("freeboard_remaining_m", 5)
        disp = f.get("crest_displacement_mm_yr", 0)
        dbi = f.get("dam_breach_index", 3.5)
        ori = f.get("overtopping_risk_index", 0)

        z = (
            0.02 * f.get("rain_1h_mm", 0) +
            0.005 * f.get("rain_24h_mm", 0) +
            1.5 * min(inflow_ratio, 5.0) -
            1.0 * fb +
            0.06 * min(disp, 80) +
            (2.5 if f.get("is_natural", 0) else 0.0) -
            1.5 * dbi +
            300.0 * max(ori, 0) -
            2.0  # Bias term: push nominal dams towards safe
        )

        prob = 1.0 / (1.0 + np.exp(-np.clip(z, -10, 10)))
        return float(prob)

    def classify_alert(self, probability: float) -> dict:
        """
        Classify breach probability into alert level.

        Returns:
            dict with keys: level, color, emoji, description
        """
        for level, thresholds in ALERT_LEVELS.items():
            if thresholds["min"] <= probability < thresholds["max"]:
                return {
                    "level": level,
                    "color": thresholds["color"],
                    "emoji": thresholds["emoji"],
                    "probability": probability,
                    "description": self._get_alert_description(level, probability),
                }

        # Edge case: probability == 1.0
        return {
            "level": "IMMINENT",
            "color": "#ef4444",
            "emoji": "🔴",
            "probability": probability,
            "description": "CATASTROPHIC FAILURE IMMINENT — Immediate evacuation required",
        }

    def explain_prediction(self, feature_dict: dict) -> dict:
        """
        Generate SHAP-based feature attribution for a prediction.

        Falls back to coefficient-based explanation if SHAP unavailable.
        """
        # Try SHAP-based explanation
        if self._mode == "ml" and self.model is not None:
            try:
                return self._explain_shap(feature_dict)
            except Exception:
                pass

        # Fallback: coefficient-based explanation
        return self._explain_heuristic(feature_dict)

    def _explain_shap(self, feature_dict: dict) -> dict:
        """Generate SHAP explanation using TreeSHAP."""
        import shap

        if self.explainer is None:
            self.explainer = shap.TreeExplainer(self.model)

        df = pd.DataFrame([{col: feature_dict.get(col, 0.0) for col in FEATURE_COLS}])
        shap_values = self.explainer.shap_values(df)

        # Handle binary classification SHAP output
        if isinstance(shap_values, list):
            sv = shap_values[1][0]  # Class 1 (breach) SHAP values
        else:
            sv = shap_values[0]

        # Build attribution dict
        attributions = {}
        for i, col in enumerate(FEATURE_COLS):
            attributions[col] = {
                "value": feature_dict.get(col, 0.0),
                "shap_value": float(sv[i]),
                "contribution": "increases" if sv[i] > 0 else "decreases",
            }

        # Sort by absolute SHAP value
        sorted_attrs = dict(sorted(
            attributions.items(),
            key=lambda x: abs(x[1]["shap_value"]),
            reverse=True
        ))

        # Generate natural language summary
        top_factors = list(sorted_attrs.items())[:3]
        summary = self._generate_explanation_text(top_factors, feature_dict)

        return {
            "attributions": sorted_attrs,
            "summary": summary,
            "method": "TreeSHAP",
        }

    def _explain_heuristic(self, feature_dict: dict) -> dict:
        """Generate coefficient-based explanation (fallback)."""
        f = feature_dict

        # Compute individual contributions (matching recalibrated heuristic)
        q_in = f.get("inflow_surge_cumec", 0)
        q_spill = max(f.get("spillway_capacity_cumec", 1), 1)
        inflow_ratio = max(q_in - q_spill, 0) / q_spill

        contributions = {
            "rain_1h_mm": 0.02 * f.get("rain_1h_mm", 0),
            "rain_24h_mm": 0.005 * f.get("rain_24h_mm", 0),
            "inflow_surge_cumec": 1.5 * min(inflow_ratio, 5.0),
            "freeboard_remaining_m": -1.0 * f.get("freeboard_remaining_m", 5),
            "crest_displacement_mm_yr": 0.06 * min(f.get("crest_displacement_mm_yr", 0), 80),
            "is_natural": 2.5 if f.get("is_natural", 0) else 0.0,
            "dam_breach_index": -1.5 * f.get("dam_breach_index", 3.5),
            "overtopping_risk_index": 300.0 * max(f.get("overtopping_risk_index", 0), 0),
        }

        # Build attribution structure
        attributions = {}
        for col, contrib in contributions.items():
            attributions[col] = {
                "value": f.get(col, 0.0),
                "contribution_score": round(contrib, 4),
                "contribution": "increases" if contrib > 0 else "decreases",
            }

        sorted_attrs = dict(sorted(
            attributions.items(),
            key=lambda x: abs(x[1]["contribution_score"]),
            reverse=True
        ))

        top_factors = list(sorted_attrs.items())[:3]
        summary = self._generate_explanation_text(top_factors, f)

        return {
            "attributions": sorted_attrs,
            "summary": summary,
            "method": "Heuristic Coefficients",
        }

    def _generate_explanation_text(self, top_factors: list, features: dict) -> str:
        """Generate natural language explanation from top factors."""
        prob = self.predict_probability(features)
        pct = prob * 100

        descriptions = {
            "rain_1h_mm": f"intense rainfall ({features.get('rain_1h_mm', 0):.0f} mm/hr)",
            "rain_24h_mm": f"sustained 24h precipitation ({features.get('rain_24h_mm', 0):.0f} mm)",
            "inflow_surge_cumec": f"high inflow surge ({features.get('inflow_surge_cumec', 0):.0f} m³/s)",
            "freeboard_remaining_m": f"{'critical' if features.get('freeboard_remaining_m', 5) < 2 else 'low'} freeboard ({features.get('freeboard_remaining_m', 0):.1f}m remaining)",
            "crest_displacement_mm_yr": f"active crest deformation ({features.get('crest_displacement_mm_yr', 0):.1f} mm/yr)",
            "is_natural": "natural landslide barrier (inherently unstable)",
            "dam_breach_index": f"{'unstable' if features.get('dam_breach_index', 3.5) < 2.75 else 'marginal'} geomorphic index (DBI={features.get('dam_breach_index', 0):.2f})",
            "overtopping_risk_index": f"{'active' if features.get('overtopping_risk_index', 0) > 0 else 'marginal'} overtopping risk (ORI={features.get('overtopping_risk_index', 0):.6f})",
            "spillway_capacity_cumec": f"spillway capacity ({features.get('spillway_capacity_cumec', 0):.0f} m³/s)",
            "dam_age_years": f"structure age ({features.get('dam_age_years', 0)} years)",
            "catchment_area_sqkm": f"large catchment ({features.get('catchment_area_sqkm', 0):.0f} km²)",
        }

        factors_text = []
        for name, data in top_factors:
            desc = descriptions.get(name, name)
            factors_text.append(desc)

        if len(factors_text) >= 3:
            summary = (
                f"{pct:.0f}% failure risk driven by {factors_text[0]}, "
                f"{factors_text[1]}, and {factors_text[2]}."
            )
        elif len(factors_text) == 2:
            summary = f"{pct:.0f}% failure risk driven by {factors_text[0]} and {factors_text[1]}."
        else:
            summary = f"{pct:.0f}% failure risk driven by {factors_text[0]}."

        return summary

    @staticmethod
    def _get_alert_description(level: str, prob: float) -> str:
        """Generate alert level description."""
        descriptions = {
            "NORMAL": "Dam operating within safe parameters. Continue routine monitoring.",
            "WATCH": "Elevated conditions detected. Increase monitoring frequency and standby emergency protocols.",
            "WARNING": "Significant breach risk detected. Activate emergency response teams and prepare downstream evacuation.",
            "IMMINENT": "CATASTROPHIC FAILURE IMMINENT. Execute immediate downstream evacuation. Deploy NDRF/SDRF teams.",
        }
        return descriptions.get(level, "Unknown alert level")

    @property
    def mode(self) -> str:
        """Return current prediction mode ('ml' or 'heuristic')."""
        return self._mode
