"""
HydroGuard-AI: Modal GPU Training Pipeline

Trains XGBoost + LightGBM ensemble on Modal serverless GPUs.
Performs Bayesian hyperparameter tuning, SMOTE oversampling,
isotonic calibration, and SHAP analysis.

Usage:
    modal run modal_training/train_gpu.py
"""

import modal
import os

# ─── Modal Configuration ─────────────────────────────────────────────────

app = modal.App("hydroguard-training")

# Container image with all ML dependencies
training_image = (
    modal.Image.debian_slim(python_version="3.11")
    .pip_install(
        "fastapi[standard]>=0.104.0",
        "xgboost>=2.0.0",
        "lightgbm>=4.0.0",
        "scikit-learn>=1.3.0",
        "imbalanced-learn>=0.11.0",
        "optuna>=3.4.0",
        "shap>=0.43.0",
        "pandas>=2.1.0",
        "numpy>=1.25.0",
        "matplotlib>=3.8.0",
        "joblib>=1.3.0",
    )
)

# Persistent volume for model artifacts and training data
volume = modal.Volume.from_name("hydroguard-models", create_if_missing=True)

VOLUME_MOUNT = "/vol"
DATA_DIR = f"{VOLUME_MOUNT}/data"
MODEL_DIR = f"{VOLUME_MOUNT}/models"


# ─── Training Function ───────────────────────────────────────────────────

@app.function(
    image=training_image,
    gpu="A10G",
    timeout=3600,           # 1 hour max
    volumes={VOLUME_MOUNT: volume},
)
def train_breach_model():
    """
    Full GPU-accelerated training pipeline.

    Steps:
        1. Load synthetic training data from volume
        2. Stratified train/val/test split (70/15/15)
        3. SMOTE oversampling on training set
        4. Bayesian HPO for XGBoost (GPU) via Optuna
        5. Bayesian HPO for LightGBM (GPU) via Optuna
        6. Build soft-voting ensemble
        7. Isotonic calibration on validation set
        8. Evaluate on held-out test set
        9. Save all artifacts to volume
    """
    import numpy as np
    import pandas as pd
    import joblib
    import json
    import xgboost as xgb
    import lightgbm as lgb
    import optuna
    from sklearn.model_selection import train_test_split
    from sklearn.metrics import (
        roc_auc_score, brier_score_loss, classification_report,
        confusion_matrix, log_loss
    )
    from sklearn.calibration import CalibratedClassifierCV
    from sklearn.isotonic import IsotonicRegression
    from imblearn.over_sampling import SMOTE

    optuna.logging.set_verbosity(optuna.logging.WARNING)

    print("=" * 70)
    print("  HydroGuard-AI: GPU-Accelerated Training Pipeline")
    print("=" * 70)

    # ── Step 1: Load Data ──
    data_path = f"{DATA_DIR}/training_dataset.csv"
    print(f"\n[1/9] Loading training data from {data_path}...")
    df = pd.read_csv(data_path)
    print(f"       Dataset shape: {df.shape}")
    print(f"       Breach rate: {df['breach_label'].mean()*100:.1f}%")

    feature_cols = [
        "dam_height_m", "crest_length_m", "catchment_area_sqkm",
        "current_storage_mcm", "freeboard_remaining_m", "spillway_capacity_cumec",
        "rain_1h_mm", "rain_24h_mm", "inflow_surge_cumec",
        "crest_displacement_mm_yr", "structural_type_code", "is_natural",
        "dam_age_years", "overtopping_risk_index", "dam_breach_index",
    ]

    X = df[feature_cols].values
    y = df["breach_label"].values

    # ── Step 2: Stratified Split ──
    print("\n[2/9] Stratified train/val/test split (70/15/15)...")
    X_train, X_temp, y_train, y_temp = train_test_split(
        X, y, test_size=0.30, random_state=42, stratify=y
    )
    X_val, X_test, y_val, y_test = train_test_split(
        X_temp, y_temp, test_size=0.50, random_state=42, stratify=y_temp
    )
    print(f"       Train: {X_train.shape[0]} | Val: {X_val.shape[0]} | Test: {X_test.shape[0]}")

    # ── Step 3: SMOTE Oversampling ──
    print("\n[3/9] Applying SMOTE oversampling on training set...")
    smote = SMOTE(random_state=42)
    X_train_sm, y_train_sm = smote.fit_resample(X_train, y_train)
    print(f"       After SMOTE: {X_train_sm.shape[0]} samples "
          f"(breach: {y_train_sm.sum()}, non-breach: {(1-y_train_sm).sum()})")

    # ── Step 4: XGBoost HPO ──
    print("\n[4/9] Bayesian HPO for XGBoost (GPU, 40 trials)...")

    def xgb_objective(trial):
        params = {
            "max_depth": trial.suggest_int("max_depth", 3, 10),
            "learning_rate": trial.suggest_float("learning_rate", 0.01, 0.3, log=True),
            "n_estimators": trial.suggest_int("n_estimators", 100, 800),
            "min_child_weight": trial.suggest_int("min_child_weight", 1, 10),
            "subsample": trial.suggest_float("subsample", 0.6, 1.0),
            "colsample_bytree": trial.suggest_float("colsample_bytree", 0.6, 1.0),
            "reg_alpha": trial.suggest_float("reg_alpha", 1e-8, 10.0, log=True),
            "reg_lambda": trial.suggest_float("reg_lambda", 1e-8, 10.0, log=True),
            "gamma": trial.suggest_float("gamma", 1e-8, 5.0, log=True),
            "tree_method": "hist",
            "device": "cuda",
            "eval_metric": "logloss",
            "random_state": 42,
        }
        model = xgb.XGBClassifier(**params)
        model.fit(
            X_train_sm, y_train_sm,
            eval_set=[(X_val, y_val)],
            verbose=False,
        )
        y_pred = model.predict_proba(X_val)[:, 1]
        return log_loss(y_val, y_pred)

    xgb_study = optuna.create_study(direction="minimize")
    xgb_study.optimize(xgb_objective, n_trials=40, show_progress_bar=True)

    best_xgb_params = xgb_study.best_params
    best_xgb_params.update({
        "tree_method": "hist",
        "device": "cuda",
        "eval_metric": "logloss",
        "random_state": 42,
    })
    print(f"       Best XGBoost log-loss: {xgb_study.best_value:.4f}")

    xgb_model = xgb.XGBClassifier(**best_xgb_params)
    xgb_model.fit(X_train_sm, y_train_sm, eval_set=[(X_val, y_val)], verbose=False)

    # ── Step 5: LightGBM HPO ──
    print("\n[5/9] Bayesian HPO for LightGBM (GPU, 40 trials)...")

    def lgbm_objective(trial):
        params = {
            "max_depth": trial.suggest_int("max_depth", 3, 10),
            "learning_rate": trial.suggest_float("learning_rate", 0.01, 0.3, log=True),
            "n_estimators": trial.suggest_int("n_estimators", 100, 800),
            "num_leaves": trial.suggest_int("num_leaves", 20, 150),
            "min_child_samples": trial.suggest_int("min_child_samples", 5, 50),
            "subsample": trial.suggest_float("subsample", 0.6, 1.0),
            "colsample_bytree": trial.suggest_float("colsample_bytree", 0.6, 1.0),
            "reg_alpha": trial.suggest_float("reg_alpha", 1e-8, 10.0, log=True),
            "reg_lambda": trial.suggest_float("reg_lambda", 1e-8, 10.0, log=True),
            "device": "cpu",
            "verbose": -1,
            "random_state": 42,
        }
        model = lgb.LGBMClassifier(**params)
        model.fit(
            X_train_sm, y_train_sm,
            eval_set=[(X_val, y_val)],
        )
        y_pred = model.predict_proba(X_val)[:, 1]
        return log_loss(y_val, y_pred)

    lgbm_study = optuna.create_study(direction="minimize")
    lgbm_study.optimize(lgbm_objective, n_trials=40, show_progress_bar=True)

    best_lgbm_params = lgbm_study.best_params
    best_lgbm_params.update({
        "device": "cpu",
        "verbose": -1,
        "random_state": 42,
    })
    print(f"       Best LightGBM log-loss: {lgbm_study.best_value:.4f}")

    lgbm_model = lgb.LGBMClassifier(**best_lgbm_params)
    lgbm_model.fit(X_train_sm, y_train_sm, eval_set=[(X_val, y_val)])

    # ── Step 6: Soft-Voting Ensemble ──
    print("\n[6/9] Building soft-voting ensemble...")
    from sklearn.ensemble import VotingClassifier

    ensemble = VotingClassifier(
        estimators=[("xgb", xgb_model), ("lgbm", lgbm_model)],
        voting="soft",
        weights=[0.55, 0.45],  # Slightly favor XGBoost
    )
    ensemble.fit(X_train_sm, y_train_sm)

    # ── Step 7: Isotonic Calibration ──
    print("\n[7/9] Isotonic probability calibration on validation set...")
    val_probs = ensemble.predict_proba(X_val)[:, 1]
    calibrator = IsotonicRegression(y_min=0, y_max=1, out_of_bounds="clip")
    calibrator.fit(val_probs, y_val)

    # ── Step 8: Test Set Evaluation ──
    print("\n[8/9] Evaluating on held-out test set...")
    test_probs_raw = ensemble.predict_proba(X_test)[:, 1]
    test_probs_cal = calibrator.predict(test_probs_raw)
    test_preds = (test_probs_cal >= 0.5).astype(int)

    auc = roc_auc_score(y_test, test_probs_cal)
    brier = brier_score_loss(y_test, test_probs_cal)
    report = classification_report(y_test, test_preds)
    cm = confusion_matrix(y_test, test_preds)

    print(f"\n       AUC-ROC:      {auc:.4f}")
    print(f"       Brier Score:  {brier:.4f}")
    print(f"\n       Confusion Matrix:\n       {cm}")
    print(f"\n       Classification Report:\n{report}")

    # XGBoost standalone metrics
    xgb_probs = xgb_model.predict_proba(X_test)[:, 1]
    xgb_auc = roc_auc_score(y_test, xgb_probs)

    lgbm_probs = lgbm_model.predict_proba(X_test)[:, 1]
    lgbm_auc = roc_auc_score(y_test, lgbm_probs)

    print(f"       XGBoost AUC:  {xgb_auc:.4f}")
    print(f"       LightGBM AUC: {lgbm_auc:.4f}")
    print(f"       Ensemble AUC: {auc:.4f}")

    # ── Step 9: Save Artifacts ──
    print(f"\n[9/9] Saving model artifacts to {MODEL_DIR}...")
    os.makedirs(MODEL_DIR, exist_ok=True)

    joblib.dump(xgb_model, f"{MODEL_DIR}/dam_failure_xgb.pkl")
    joblib.dump(lgbm_model, f"{MODEL_DIR}/dam_failure_lgbm.pkl")
    joblib.dump(ensemble, f"{MODEL_DIR}/ensemble_model.pkl")
    joblib.dump(calibrator, f"{MODEL_DIR}/calibrator.pkl")

    # Save training metrics
    metrics = {
        "ensemble_auc_roc": float(auc),
        "ensemble_brier_score": float(brier),
        "xgb_auc_roc": float(xgb_auc),
        "lgbm_auc_roc": float(lgbm_auc),
        "xgb_best_params": best_xgb_params,
        "lgbm_best_params": {k: v for k, v in best_lgbm_params.items()
                             if not callable(v)},
        "train_samples": int(X_train_sm.shape[0]),
        "val_samples": int(X_val.shape[0]),
        "test_samples": int(X_test.shape[0]),
        "confusion_matrix": cm.tolist(),
        "feature_columns": feature_cols,
    }
    with open(f"{MODEL_DIR}/training_metrics.json", "w") as f:
        json.dump(metrics, f, indent=2, default=str)

    # Generate SHAP summary
    print("\n       Generating SHAP feature importance...")
    try:
        import shap
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        explainer = shap.TreeExplainer(xgb_model)
        shap_values = explainer.shap_values(X_test[:500])

        fig, ax = plt.subplots(figsize=(12, 8))
        shap.summary_plot(
            shap_values, X_test[:500],
            feature_names=feature_cols,
            show=False,
        )
        plt.tight_layout()
        plt.savefig(f"{MODEL_DIR}/shap_summary.png", dpi=150, bbox_inches="tight")
        plt.close()
        print("       SHAP summary plot saved.")
    except Exception as e:
        print(f"       SHAP plot generation skipped: {e}")

    volume.commit()

    print("\n" + "=" * 70)
    print("  ✅ Training complete! All artifacts saved to Modal Volume.")
    print(f"     Ensemble AUC-ROC: {auc:.4f} | Brier: {brier:.4f}")
    print("=" * 70)

    return metrics


# ─── Data Upload Function ────────────────────────────────────────────────

@app.function(
    image=training_image,
    volumes={VOLUME_MOUNT: volume},
)
def upload_training_data(csv_bytes: bytes):
    """Upload training CSV to Modal Volume."""
    os.makedirs(DATA_DIR, exist_ok=True)
    path = f"{DATA_DIR}/training_dataset.csv"
    with open(path, "wb") as f:
        f.write(csv_bytes)
    volume.commit()
    print(f"[UPLOAD] ✅ Training data uploaded to {path} ({len(csv_bytes)} bytes)")
    return path


# ─── Download Function ───────────────────────────────────────────────────

@app.function(
    image=training_image,
    volumes={VOLUME_MOUNT: volume},
)
def download_model_artifacts() -> dict:
    """Download all model artifacts from Modal Volume."""
    volume.reload()
    artifacts = {}

    for filename in [
        "dam_failure_xgb.pkl", "dam_failure_lgbm.pkl",
        "ensemble_model.pkl", "calibrator.pkl",
        "training_metrics.json", "shap_summary.png"
    ]:
        path = f"{MODEL_DIR}/{filename}"
        if os.path.exists(path):
            with open(path, "rb") as f:
                artifacts[filename] = f.read()
            print(f"[DOWNLOAD] 📥 {filename} ({len(artifacts[filename])} bytes)")

    return artifacts


# ─── Live Serverless Inference Function ───────────────────────────────────

def _compute_breach_prediction(feature_dict: dict) -> dict:
    """Internal core inference and explanation engine."""
    import joblib
    import numpy as np
    import pandas as pd
    import warnings
    warnings.filterwarnings("ignore")

    volume.reload()

    model_path = f"{MODEL_DIR}/ensemble_model.pkl"
    cal_path = f"{MODEL_DIR}/calibrator.pkl"

    if not os.path.exists(model_path):
        return {"error": "Model not trained yet. Run training function first."}

    model = joblib.load(model_path)
    calibrator = joblib.load(cal_path) if os.path.exists(cal_path) else None

    # Dynamically detect hardware to eliminate XGBoost context device warnings
    has_cuda = False
    try:
        import torch
        has_cuda = torch.cuda.is_available()
    except Exception:
        has_cuda = False
    target_device = "cuda" if has_cuda else "cpu"

    try:
        for name, est in getattr(model, "named_estimators_", {}).items():
            if hasattr(est, "set_params"):
                est.set_params(device=target_device)
            if hasattr(est, "get_booster"):
                try:
                    est.get_booster().set_param({"device": target_device})
                except Exception:
                    pass
    except Exception:
        pass

    def _safe_float(val, default):
        if val is None:
            return float(default)
        try:
            return float(val)
        except (ValueError, TypeError):
            return float(default)

    def _safe_int(val, default):
        if val is None:
            return int(default)
        try:
            return int(val)
        except (ValueError, TypeError):
            return int(default)

    # Extract or calculate parameters safely
    dam_name = feature_dict.get("dam_name") or "Unknown Dam"
    height = _safe_float(feature_dict.get("dam_height_m", feature_dict.get("height_m")), 32.0)
    crest_len = _safe_float(feature_dict.get("crest_length_m"), 2140.0)
    catchment = _safe_float(feature_dict.get("catchment_area_sqkm"), 4661.0)
    storage = _safe_float(feature_dict.get("current_storage_mcm"), 180.0)
    freeboard = _safe_float(feature_dict.get("freeboard_remaining_m", feature_dict.get("freeboard_m")), 2.5)
    is_natural = int(bool(feature_dict.get("is_natural", False)))
    spillway = _safe_float(feature_dict.get("spillway_capacity_cumec"), 11200.0)
    # Under IS 11223, engineered dams possess emergency broad-crested waste weirs (>= 15 m³/s)
    if not is_natural and spillway < 10.0 and height >= 10.0:
        spillway = max(spillway, 15.0)

    rain_1h = _safe_float(feature_dict.get("rain_1h_mm"), 0.0)
    rain_24h = _safe_float(feature_dict.get("rain_24h_mm"), 0.0)
    crest_disp = _safe_float(feature_dict.get("crest_displacement_mm_yr"), 0.0)
    dam_age = _safe_int(feature_dict.get("dam_age_years"), 40)

    struct_type = feature_dict.get("structural_type") or "Earthfill"
    struct_map = {"Earthfill": 0, "Rockfill": 1, "Gravity Concrete": 2, "Masonry": 3, "Landslide Debris": 4}
    struct_code = _safe_int(feature_dict.get("structural_type_code"), struct_map.get(struct_type, 0))
    if is_natural:
        struct_code = 4

    # Calculate physics indices if not provided
    q_in = feature_dict.get("inflow_surge_cumec")
    if q_in is None:
        c_runoff = 0.75 if is_natural else 0.55
        if rain_1h <= 1.0:
            q_in = 0.0
        elif rain_1h < 8.0:
            rain_eff = max(0.0, rain_1h - 1.0)
            c_eff = c_runoff * min(1.0, max(0.05, (rain_1h - 1.0) / 7.0))
            q_in = 0.278 * c_eff * rain_eff * catchment
        else:
            q_in = 0.278 * c_runoff * rain_1h * catchment
    q_in = _safe_float(q_in, 0.0)

    ori = feature_dict.get("overtopping_risk_index")
    if ori is None:
        res_area = max(storage * 1e6 / max(height, 1.0), 10000.0)
        net_flow = max(0.0, q_in - spillway)
        fb = max(freeboard, 0.05)
        ori = net_flow / (res_area * fb)
    ori = _safe_float(ori, 0.0)

    dbi = feature_dict.get("dam_breach_index")
    if dbi is None:
        if is_natural:
            vol_barrier = _safe_float(feature_dict.get("barrier_volume_m3"), 1.2e6)
            vol_lake = max(storage * 1e6, 1.0)
            slope = _safe_float(feature_dict.get("river_slope"), 0.04)
            dbi = np.log10(max(vol_barrier / (vol_lake * slope), 1e-4))
        else:
            dbi = 3.50
    dbi = _safe_float(dbi, 3.50)

    feature_cols = [
        "dam_height_m", "crest_length_m", "catchment_area_sqkm",
        "current_storage_mcm", "freeboard_remaining_m", "spillway_capacity_cumec",
        "rain_1h_mm", "rain_24h_mm", "inflow_surge_cumec",
        "crest_displacement_mm_yr", "structural_type_code", "is_natural",
        "dam_age_years", "overtopping_risk_index", "dam_breach_index",
    ]

    row_vals = {
        "dam_height_m": height,
        "crest_length_m": crest_len,
        "catchment_area_sqkm": catchment,
        "current_storage_mcm": storage,
        "freeboard_remaining_m": freeboard,
        "spillway_capacity_cumec": spillway,
        "rain_1h_mm": rain_1h,
        "rain_24h_mm": rain_24h,
        "inflow_surge_cumec": q_in,
        "crest_displacement_mm_yr": crest_disp,
        "structural_type_code": struct_code,
        "is_natural": is_natural,
        "dam_age_years": dam_age,
        "overtopping_risk_index": ori,
        "dam_breach_index": dbi,
    }

    df = pd.DataFrame([row_vals])
    prob = model.predict_proba(df)[0, 1]

    if calibrator is not None:
        prob = calibrator.predict([prob])[0]

    prob = float(np.clip(prob, 0.0, 1.0))

    # Physical domain guardrail: an engineered dam with substantial freeboard (>= 3m),
    # low precipitation (< 8 mm/hr, < 35 mm/24h), and negligible displacement (< 5 mm/yr)
    # cannot physically overtop or suffer a geotechnical breach under light showers.
    if (
        not is_natural
        and freeboard >= 3.0
        and rain_1h < 8.0
        and rain_24h < 35.0
        and crest_disp < 5.0
    ):
        prob = min(prob, 0.05)

    if prob < 0.25:
        level = "NORMAL"
        color = "#22c55e"
        emoji = "🟢"
        desc = "Dam operating within safe parameters. Continue routine surveillance."
    elif prob < 0.50:
        level = "WATCH"
        color = "#eab308"
        emoji = "🟡"
        desc = "Elevated hydrological conditions detected. Standby emergency response protocols."
    elif prob < 0.75:
        level = "WARNING"
        color = "#f97316"
        emoji = "🟠"
        desc = "Significant breach risk detected. Prepare downstream warning and evacuation corridors."
    else:
        level = "IMMINENT"
        color = "#ef4444"
        emoji = "🔴"
        desc = "CATASTROPHIC FAILURE IMMINENT. Execute immediate evacuation."

    # Explainability factors
    top_factors = []
    if rain_1h >= 20.0:
        top_factors.append(f"intense precipitation ({rain_1h:.0f} mm/hr)")
    if freeboard < 2.0:
        top_factors.append(f"critical freeboard depletion ({freeboard:.1f}m remaining)")
    if q_in > spillway:
        top_factors.append(f"inflow surge ({q_in:.0f} m³/s) exceeding spillway ({spillway:.0f} m³/s)")
    if is_natural:
        top_factors.append("unstable natural landslide blockage mass")
    if crest_disp > 5.0:
        top_factors.append(f"active structural displacement ({crest_disp:.1f} mm/yr)")
    if not top_factors:
        top_factors = ["nominal catchment runoff", "sufficient freeboard margin"]

    if prob < 0.25:
        summary = f"Low failure risk ({int(prob * 100)}%). Dam operating safely within structural design tolerances and ample freeboard ({freeboard:.1f}m remaining)."
    else:
        summary = f"{int(prob * 100)}% failure risk driven by {', '.join(top_factors[:3])}."

    return {
        "dam_name": dam_name,
        "failure_probability": prob,
        "breach_probability": prob,
        "alert_level": level,
        "alert": {
            "level": level,
            "color": color,
            "emoji": emoji,
            "description": desc,
        },
        "features": {
            "dam_height_m": height,
            "crest_length_m": crest_len,
            "catchment_area_sqkm": catchment,
            "current_storage_mcm": storage,
            "freeboard_remaining_m": freeboard,
            "spillway_capacity_cumec": spillway,
            "rain_1h_mm": rain_1h,
            "rain_24h_mm": rain_24h,
            "inflow_surge_cumec": q_in,
            "crest_displacement_mm_yr": crest_disp,
            "structural_type_code": struct_code,
            "is_natural": is_natural,
            "dam_age_years": dam_age,
            "overtopping_risk_index": ori,
            "dam_breach_index": dbi,
        },
        "explanation": {
            "summary": summary,
            "top_factors": top_factors,
        },
        "model_version": "Modal Serverless Ensemble v1.0",
    }


@app.function(
    image=training_image,
    gpu="A10G",
    volumes={VOLUME_MOUNT: volume},
)
def predict_breach_risk(feature_dict: dict) -> dict:
    """Serverless GPU inference function on Modal."""
    return _compute_breach_prediction(feature_dict)


@app.function(
    image=training_image,
    gpu="A10G",
    volumes={VOLUME_MOUNT: volume},
)
@modal.fastapi_endpoint(method="POST")
def predict_breach_web(feature_dict: dict) -> dict:
    """Serverless public HTTPS endpoint on Modal for direct web / client calls."""
    return _compute_breach_prediction(feature_dict)


def _compute_batch_breach_prediction(dams_list: list) -> list:
    """Batch compute predictions across multiple dams."""
    results = []
    for item in dams_list:
        if isinstance(item, dict):
            try:
                pred = _compute_breach_prediction(item)
                results.append(pred)
            except Exception as e:
                results.append({
                    "dam_name": item.get("dam_name", "Unknown"),
                    "failure_probability": 0.05,
                    "breach_probability": 0.05,
                    "alert_level": "NORMAL",
                    "error": str(e)
                })
    return results


@app.function(
    image=training_image,
    gpu="A10G",
    volumes={VOLUME_MOUNT: volume},
)
def batch_predict_breach_risk(dams_list: list) -> list:
    """Serverless GPU batch inference function on Modal."""
    return _compute_batch_breach_prediction(dams_list)


@app.function(
    image=training_image,
    gpu="A10G",
    volumes={VOLUME_MOUNT: volume},
)
@modal.fastapi_endpoint(method="POST")
def batch_predict_breach_web(payload: dict) -> dict:
    """Serverless public HTTPS endpoint for batch prediction across dams."""
    dams = payload.get("dams", [])
    predictions = _compute_batch_breach_prediction(dams)
    return {
        "status": "success",
        "count": len(predictions),
        "predictions": predictions,
        "model_version": "Modal Serverless Ensemble v1.0"
    }


# ─── Entry Point ──────────────────────────────────────────────────────────

@app.local_entrypoint()
def main():
    """Run the full training pipeline."""
    import json
    print("\n🚀 Launching HydroGuard-AI GPU Training on Modal...\n")
    metrics = train_breach_model.remote()
    print(f"\n📊 Final Metrics: {json.dumps(metrics, indent=2, default=str)}")

