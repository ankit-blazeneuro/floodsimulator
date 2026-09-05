"""
HydroGuard-AI: Model Download Script

Downloads trained model artifacts from Modal Volume to local models/ directory.
"""

import os
import sys
import json

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, PROJECT_ROOT)


def main():
    print("=" * 60)
    print("  HydroGuard-AI: Download Trained Model from Modal")
    print("=" * 60)

    import modal

    local_model_dir = os.path.join(PROJECT_ROOT, "models")
    os.makedirs(local_model_dir, exist_ok=True)

    print("\n📥 Downloading model artifacts directly from Modal Volume ('hydroguard-models')...")

    vol = modal.Volume.from_name("hydroguard-models")
    artifact_filenames = [
        "dam_failure_xgb.pkl", "dam_failure_lgbm.pkl",
        "ensemble_model.pkl", "calibrator.pkl",
        "training_metrics.json", "shap_summary.png"
    ]

    for filename in artifact_filenames:
        local_path = os.path.join(local_model_dir, filename)
        try:
            with open(local_path, "wb") as f:
                for chunk in vol.read_file(f"models/{filename}"):
                    f.write(chunk)
            size_kb = os.path.getsize(local_path) / 1024
            print(f"   ✅ {filename} ({size_kb:.1f} KB)")
        except Exception as e:
            print(f"   ⚠️  Could not download {filename}: {e}")

    # Verify by loading metrics
    metrics_path = os.path.join(local_model_dir, "training_metrics.json")
    if os.path.exists(metrics_path):
        with open(metrics_path, "r") as f:
            metrics = json.load(f)
        print(f"\n📊 Model Performance:")
        print(f"   Ensemble AUC-ROC:  {metrics.get('ensemble_auc_roc', 'N/A')}")
        print(f"   Ensemble Brier:    {metrics.get('ensemble_brier_score', 'N/A')}")
        print(f"   XGBoost AUC-ROC:   {metrics.get('xgb_auc_roc', 'N/A')}")
        print(f"   LightGBM AUC-ROC:  {metrics.get('lgbm_auc_roc', 'N/A')}")

    # Quick verification
    print("\n🔍 Quick verification with test prediction...")
    try:
        sys.path.insert(0, PROJECT_ROOT)
        from src.predictor import DamBreachPredictor
        predictor = DamBreachPredictor(model_dir=local_model_dir)

        # Rishi Ganga case study
        test_features = {
            "dam_height_m": 55.0, "crest_length_m": 200.0,
            "catchment_area_sqkm": 520.0, "current_storage_mcm": 2.8,
            "freeboard_remaining_m": 0.8, "spillway_capacity_cumec": 0.0,
            "rain_1h_mm": 35.0, "rain_24h_mm": 80.0,
            "inflow_surge_cumec": 3852.6, "crest_displacement_mm_yr": 45.0,
            "structural_type_code": 4, "is_natural": 1,
            "dam_age_years": 0,
            "overtopping_risk_index": 0.0012,
            "dam_breach_index": 0.81,
        }
        prob = predictor.predict_probability(test_features)
        alert = predictor.classify_alert(prob)
        print(f"   Rishi Ganga Test: P(breach) = {prob:.4f} [{alert['emoji']} {alert['level']}]")
        print(f"   Mode: {predictor.mode}")
    except Exception as e:
        print(f"   ⚠️  Verification skipped: {e}")

    print("\n✅ Model download complete! Models saved to models/")


if __name__ == "__main__":
    main()
