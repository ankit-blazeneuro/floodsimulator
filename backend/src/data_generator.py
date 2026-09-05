"""
HydroGuard-AI: Physics-Informed Synthetic Training Data Generator

Generates realistic dam breach / non-breach training samples using
domain formulations from hydrology literature:
    - Rational Method for peak inflow (Q_in = 0.278 * C * I * A)
    - Overtopping Risk Index (ORI)
    - Stefanelli Dam Breach Index (DBI)
    - Physics-based breach labeling with stochastic noise

Produces ~5,000 samples with ~15-20% breach events (class imbalanced).
"""

import os
import json
import numpy as np
import pandas as pd
from pathlib import Path


# ─── Physical Constants & Distribution Parameters ─────────────────────────

# Structural types and their properties
STRUCTURAL_TYPES = {
    "Earthfill":         {"code": 0, "runoff_c": 0.55, "breach_base": 0.12, "weight": 0.30},
    "Rockfill":          {"code": 1, "runoff_c": 0.50, "breach_base": 0.08, "weight": 0.10},
    "Gravity Concrete":  {"code": 2, "runoff_c": 0.50, "breach_base": 0.05, "weight": 0.25},
    "Masonry":           {"code": 3, "runoff_c": 0.52, "breach_base": 0.10, "weight": 0.10},
    "Landslide Debris":  {"code": 4, "runoff_c": 0.75, "breach_base": 0.35, "weight": 0.25},
}


class SyntheticDataGenerator:
    """
    Physics-informed synthetic dataset generator for dam breach prediction.

    Uses domain formulations to create realistic training data that respects
    hydrological and geotechnical constraints from ICOLD/GLDD literature.
    """

    def __init__(self, n_samples: int = 5000, seed: int = 42):
        self.n_samples = n_samples
        self.rng = np.random.default_rng(seed)

    def _sample_dam_parameters(self) -> pd.DataFrame:
        """Sample realistic dam structural and hydrological parameters from 6,600+ real dams."""
        n = self.n_samples

        # Check if real dam seed is available
        seed_path = Path(__file__).parent.parent / "data" / "india_dams_seed.json"
        if seed_path.exists():
            with open(seed_path, "r", encoding="utf-8") as f:
                seed_data = json.load(f)
            real_dams = seed_data.get("dams", [])
        else:
            real_dams = []

        if len(real_dams) > 0:
            # Sample with replacement from 6,600+ real Indian dams
            sampled_indices = self.rng.choice(len(real_dams), size=n, replace=True)
            sampled_dams = [real_dams[i] for i in sampled_indices]

            height = np.array([d.get("dam_height_m", 25.0) for d in sampled_dams])
            crest_length = np.array([d.get("crest_length_m", 250.0) for d in sampled_dams])
            catchment_area = np.array([d.get("catchment_area_sqkm", 150.0) for d in sampled_dams])
            current_storage = np.array([d.get("current_storage_mcm", 10.0) for d in sampled_dams])
            freeboard = np.array([d.get("freeboard_remaining_m", 3.5) for d in sampled_dams])
            spillway_capacity = np.array([d.get("spillway_capacity_cumec", 500.0) for d in sampled_dams])
            dam_age = np.array([d.get("dam_age_years", 30) for d in sampled_dams])
            crest_displacement = np.array([d.get("crest_displacement_mm_yr", 1.2) for d in sampled_dams])
            struct_codes = np.array([d.get("structural_type_code", 0) for d in sampled_dams])
            struct_types = [d.get("structural_type", "earthen") for d in sampled_dams]
            is_natural = np.array([1 if d.get("is_natural", False) else 0 for d in sampled_dams])

            # Add mild noise to storage, freeboard, and displacement for sample diversity
            current_storage = np.clip(current_storage * self.rng.uniform(0.7, 1.2, size=n), 0.1, 15000)
            freeboard = np.clip(freeboard * self.rng.uniform(0.5, 1.5, size=n), 0.05, 20)
            crest_displacement = np.clip(crest_displacement * self.rng.uniform(0.5, 2.0, size=n), 0.0, 100.0)

            river_slope = np.where(is_natural == 1, self.rng.uniform(0.03, 0.10, size=n), self.rng.uniform(0.001, 0.04, size=n))
            barrier_volume = np.where(is_natural == 1, self.rng.lognormal(mean=13.5, sigma=1.0, size=n), 0.0)
            max_storage = current_storage * self.rng.uniform(1.05, 1.5, size=n)

            return pd.DataFrame({
                "dam_height_m": np.round(height, 2),
                "crest_length_m": np.round(crest_length, 1),
                "catchment_area_sqkm": np.round(catchment_area, 1),
                "current_storage_mcm": np.round(current_storage, 2),
                "freeboard_remaining_m": np.round(freeboard, 2),
                "spillway_capacity_cumec": np.round(spillway_capacity, 1),
                "dam_age_years": dam_age.astype(int),
                "crest_displacement_mm_yr": np.round(crest_displacement, 2),
                "structural_type": struct_types,
                "structural_type_code": struct_codes,
                "is_natural": is_natural,
                "river_slope": np.round(river_slope, 4),
                "barrier_volume_m3": np.round(barrier_volume, 0),
                "max_storage_mcm": np.round(max_storage, 2),
            })

        # Fallback to random distribution if seed not available
        type_names = list(STRUCTURAL_TYPES.keys())
        type_weights = [STRUCTURAL_TYPES[t]["weight"] for t in type_names]
        struct_types = self.rng.choice(type_names, size=n, p=type_weights)
        is_natural = np.array([1 if t == "Landslide Debris" else 0 for t in struct_types])
        struct_codes = np.array([STRUCTURAL_TYPES[t]["code"] for t in struct_types])
        height = np.where(is_natural, self.rng.lognormal(3.2, 0.5, n), self.rng.lognormal(3.8, 0.6, n))
        height = np.clip(height, 5, 300)
        crest_length = np.where(is_natural, self.rng.lognormal(4.8, 0.5, n), self.rng.lognormal(6.5, 0.8, n))
        crest_length = np.clip(crest_length, 50, 6000)
        catchment_area = np.clip(self.rng.lognormal(7.0, 1.5, n), 50, 250000)
        max_storage = np.clip(height * catchment_area * self.rng.uniform(0.0001, 0.005, n), 0.5, 15000)
        current_storage = max_storage * self.rng.beta(5, 2, n)
        freeboard = np.where(is_natural, self.rng.exponential(1.5, n), self.rng.lognormal(1.5, 0.5, n))
        freeboard = np.clip(freeboard, 0.05, 20)
        spillway_capacity = np.where(is_natural, 0.0, self.rng.lognormal(8.5, 1.0, n))
        spillway_capacity = np.clip(spillway_capacity, 0, 60000)
        dam_age = np.where(is_natural, 0, self.rng.integers(1, 130, n))
        crest_displacement = np.where(is_natural, self.rng.lognormal(3.0, 0.8, n), self.rng.lognormal(-0.5, 1.0, n))
        crest_displacement = np.clip(crest_displacement, 0.0, 100.0)
        river_slope = np.where(is_natural, self.rng.uniform(0.03, 0.10, n), self.rng.uniform(0.001, 0.04, n))
        barrier_volume = np.where(is_natural, self.rng.lognormal(13.5, 1.0, n), 0.0)

        return pd.DataFrame({
            "dam_height_m": np.round(height, 2),
            "crest_length_m": np.round(crest_length, 1),
            "catchment_area_sqkm": np.round(catchment_area, 1),
            "current_storage_mcm": np.round(current_storage, 2),
            "freeboard_remaining_m": np.round(freeboard, 2),
            "spillway_capacity_cumec": np.round(spillway_capacity, 1),
            "dam_age_years": dam_age.astype(int),
            "crest_displacement_mm_yr": np.round(crest_displacement, 2),
            "structural_type": struct_types,
            "structural_type_code": struct_codes,
            "is_natural": is_natural,
            "river_slope": np.round(river_slope, 4),
            "barrier_volume_m3": np.round(barrier_volume, 0),
            "max_storage_mcm": np.round(max_storage, 2),
        })

    def _sample_weather_conditions(self) -> pd.DataFrame:
        """Sample realistic rainfall scenarios."""
        n = self.n_samples

        # Mix of normal and extreme rainfall events
        # ~30% are extreme events (cloudburst scenarios)
        is_extreme = self.rng.random(n) < 0.30

        # Rain in last 1 hour (mm)
        rain_1h = np.where(
            is_extreme,
            self.rng.lognormal(mean=3.5, sigma=0.5, size=n),    # ~20-80 mm/hr
            self.rng.exponential(scale=5.0, size=n)               # ~0-20 mm/hr
        )
        rain_1h = np.clip(rain_1h, 0, 150)

        # Rain in last 24 hours (mm): At least as much as 1h
        rain_24h = rain_1h + self.rng.lognormal(mean=2.5, sigma=1.0, size=n)
        rain_24h = np.clip(rain_24h, rain_1h, 500)

        return pd.DataFrame({
            "rain_1h_mm": np.round(rain_1h, 1),
            "rain_24h_mm": np.round(rain_24h, 1),
        })

    def _compute_physics_features(self, df: pd.DataFrame) -> pd.DataFrame:
        """Compute derived physics features: Q_in, ORI, DBI."""

        # ── 1. Peak Inflow Surge via Rational Method ──
        # Q_in = 0.278 * C * I * A_catchment
        c_map = {
            "earthen": 0.55, "earthfill": 0.55, "gravity": 0.50, "gravity concrete": 0.50,
            "rockfill": 0.50, "masonry": 0.52, "barrage": 0.45, "arch": 0.50,
            "landslide debris": 0.75, "natural": 0.75
        }
        runoff_c = df["structural_type"].str.lower().map(c_map).fillna(0.55).values
        df["inflow_surge_cumec"] = np.round(
            0.278 * runoff_c * df["rain_1h_mm"].values * df["catchment_area_sqkm"].values,
            2
        )

        # ── 2. Overtopping Risk Index (ORI) ──
        # ORI = (Q_in - Q_spillway) / (A_res * freeboard)
        # Approximate reservoir surface area from storage and height
        res_area = np.maximum(
            df["current_storage_mcm"].values * 1e6 / np.maximum(df["dam_height_m"].values, 1.0),
            10000.0
        )
        net_flow = df["inflow_surge_cumec"].values - df["spillway_capacity_cumec"].values
        freeboard = np.maximum(df["freeboard_remaining_m"].values, 0.05)
        df["overtopping_risk_index"] = np.round(net_flow / (res_area * freeboard), 8)

        # ── 3. Stefanelli Dam Breach Index (DBI) ──
        # DBI = log10(V_barrier / (V_lake * S))
        # For engineered dams: assign high nominal DBI (3.50)
        vol_lake = df["current_storage_mcm"].values * 1e6
        slope = df["river_slope"].values
        barrier_vol = df["barrier_volume_m3"].values

        dbi = np.where(
            df["is_natural"].values == 1,
            np.log10(np.maximum(barrier_vol / np.maximum(vol_lake * slope, 1.0), 1e-4)),
            3.50
        )
        df["dam_breach_index"] = np.round(dbi, 4)

        return df

    def _generate_breach_labels(self, df: pd.DataFrame) -> pd.Series:
        """
        Generate physics-informed breach labels.

        Breach probability is derived from:
        1. DBI threshold (DBI < 2.75 → unstable for natural dams)
        2. ORI (positive and large → overtopping imminent)
        3. Freeboard depletion
        4. Structural vulnerability (age, displacement)
        5. Stochastic noise for realistic uncertainty
        """
        n = len(df)

        # Base breach probability from structural type
        base_map = {
            "earthen": 0.12, "earthfill": 0.12, "gravity": 0.05, "gravity concrete": 0.05,
            "rockfill": 0.08, "masonry": 0.10, "barrage": 0.05, "arch": 0.05,
            "landslide debris": 0.35, "natural": 0.35
        }
        base_prob = df["structural_type"].str.lower().map(base_map).fillna(0.10).values

        # ── DBI contribution (natural dams) ──
        dbi = df["dam_breach_index"].values
        dbi_risk = np.where(
            df["is_natural"].values == 1,
            np.clip(1.0 - (dbi - 2.0) / 2.0, 0, 1),   # DBI < 2.75 → high risk
            0.0
        )

        # ── ORI contribution ──
        ori = df["overtopping_risk_index"].values
        ori_risk = np.clip(ori * 500.0, 0, 1)   # Scale ORI to [0,1]

        # ── Freeboard contribution ──
        fb = df["freeboard_remaining_m"].values
        fb_risk = np.clip(1.0 - fb / 5.0, 0, 1)    # <1m → high risk

        # ── Displacement contribution ──
        disp = df["crest_displacement_mm_yr"].values
        disp_risk = np.clip(disp / 50.0, 0, 1)

        # ── Age contribution (very old dams are riskier) ──
        age = df["dam_age_years"].values
        age_risk = np.clip(age / 150.0, 0, 0.3)

        # ── Rainfall intensity contribution ──
        rain = df["rain_1h_mm"].values
        rain_risk = np.clip((rain - 20) / 80.0, 0, 1)

        # ── Inflow surge vs spillway capacity ratio ──
        inflow = df["inflow_surge_cumec"].values
        spillway = np.maximum(df["spillway_capacity_cumec"].values, 1.0)
        surge_ratio = np.clip((inflow - spillway) / spillway, 0, 5) / 5.0

        # ── Combined probability ──
        combined = (
            base_prob * 0.10 +
            dbi_risk * 0.25 +
            ori_risk * 0.20 +
            surge_ratio * 0.25 +
            fb_risk * 0.15 +
            disp_risk * 0.10 +
            age_risk * 0.05 +
            rain_risk * 0.15
        )

        # Add stochastic noise
        noise = self.rng.normal(0, 0.05, size=n)
        combined = np.clip(combined + noise, 0, 1)

        # Convert to binary label with top 20% risk threshold
        threshold = float(np.percentile(combined, 80))
        labels = np.where(combined >= threshold, 1, 0)

        return pd.Series(labels, name="breach_label"), pd.Series(
            np.round(combined, 4), name="breach_probability"
        )

    def generate(self) -> pd.DataFrame:
        """Generate the complete synthetic training dataset."""
        print(f"[DataGen] Generating {self.n_samples} synthetic dam breach samples...")

        # Step 1: Sample base parameters
        df_dam = self._sample_dam_parameters()
        df_weather = self._sample_weather_conditions()
        df = pd.concat([df_dam, df_weather], axis=1)

        # Step 2: Compute physics features first
        df = self._compute_physics_features(df)

        # Step 3: Generate labels using computed physics features
        labels, probabilities = self._generate_breach_labels(df)
        df["breach_label"] = labels
        df["breach_probability"] = probabilities

        # Step 4: Select final columns for training
        feature_cols = [
            "dam_height_m", "crest_length_m", "catchment_area_sqkm",
            "current_storage_mcm", "freeboard_remaining_m", "spillway_capacity_cumec",
            "rain_1h_mm", "rain_24h_mm", "inflow_surge_cumec",
            "crest_displacement_mm_yr", "structural_type_code", "is_natural",
            "dam_age_years", "overtopping_risk_index", "dam_breach_index",
            "breach_label", "breach_probability"
        ]
        df_final = df[feature_cols].copy()

        breach_count = df_final["breach_label"].sum()
        total = len(df_final)
        breach_pct = breach_count / total * 100

        print(f"[DataGen] ✅ Generated {total} samples")
        print(f"[DataGen]    Breach events: {breach_count} ({breach_pct:.1f}%)")
        print(f"[DataGen]    Non-breach:    {total - breach_count} ({100-breach_pct:.1f}%)")

        return df_final

    def save(self, df: pd.DataFrame, output_dir: str = "data/synthetic") -> str:
        """Save dataset to CSV."""
        os.makedirs(output_dir, exist_ok=True)
        output_path = os.path.join(output_dir, "training_dataset.csv")
        df.to_csv(output_path, index=False)
        print(f"[DataGen] 💾 Saved to {output_path}")
        return output_path


# ─── CLI Entry Point ──────────────────────────────────────────────────────

if __name__ == "__main__":
    generator = SyntheticDataGenerator(n_samples=5000, seed=42)
    dataset = generator.generate()
    generator.save(dataset)

    # Print sample statistics
    print("\n[DataGen] Feature Statistics:")
    print(dataset.describe().round(3).to_string())
