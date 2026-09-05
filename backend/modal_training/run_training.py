"""
HydroGuard-AI: Training Launcher

Local entrypoint that:
1. Generates synthetic training data (if not exists)
2. Uploads data to Modal Volume
3. Launches GPU training on Modal
"""

import os
import sys
import json

# Add project root to path
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, PROJECT_ROOT)


def main():
    print("=" * 60)
    print("  HydroGuard-AI: Training Pipeline Launcher")
    print("=" * 60)

    # ── Step 1: Generate synthetic data if needed ──
    data_path = os.path.join(PROJECT_ROOT, "data", "synthetic", "training_dataset.csv")

    if not os.path.exists(data_path):
        print("\n[1/3] Generating synthetic training dataset...")
        from src.data_generator import SyntheticDataGenerator
        generator = SyntheticDataGenerator(n_samples=5000, seed=42)
        dataset = generator.generate()
        generator.save(dataset, os.path.join(PROJECT_ROOT, "data", "synthetic"))
    else:
        print(f"\n[1/3] Training data already exists at {data_path}")

    # ── Step 2: Upload data to Modal Volume ──
    print("\n[2/3] Uploading training data to Modal Volume...")
    import modal
    from modal_training.train_gpu import upload_training_data

    with open(data_path, "rb") as f:
        csv_bytes = f.read()

    with modal.enable_output():
        with app.run():
            upload_training_data.remote(csv_bytes)

    # ── Step 3: Launch GPU training ──
    print("\n[3/3] Launching GPU training on Modal...")
    print("       This may take 5-15 minutes depending on GPU availability.\n")

    # Use modal CLI for clean execution
    os.system(f"modal run {os.path.join(PROJECT_ROOT, 'modal_training', 'train_gpu.py')}")

    print("\n✅ Training pipeline complete!")
    print("   Run 'python modal_training/download_model.py' to fetch the trained model.")


if __name__ == "__main__":
    # Simpler approach: just use modal CLI directly
    print("=" * 60)
    print("  HydroGuard-AI: Training Pipeline Launcher")
    print("=" * 60)

    PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_path = os.path.join(PROJECT_ROOT, "data", "synthetic", "training_dataset.csv")

    # Generate data if needed
    if not os.path.exists(data_path):
        print("\n📊 Generating synthetic training dataset...")
        sys.path.insert(0, PROJECT_ROOT)
        from src.data_generator import SyntheticDataGenerator
        generator = SyntheticDataGenerator(n_samples=5000, seed=42)
        dataset = generator.generate()
        generator.save(dataset, os.path.join(PROJECT_ROOT, "data", "synthetic"))

    # Upload to Modal
    print("\n☁️  Uploading training data to Modal Volume...")
    with open(data_path, "rb") as f:
        csv_bytes = f.read()
    print(f"   Data size: {len(csv_bytes)/1024:.1f} KB")

    # Import Modal app for upload
    import modal
    from modal_training.train_gpu import app, upload_training_data

    with modal.enable_output():
        with app.run():
            upload_training_data.remote(csv_bytes)
    print("   ✅ Data uploaded to Modal Volume")

    # Launch training
    print("\n🚀 Launching GPU training...")
    print("   Use: modal run modal_training/train_gpu.py\n")
    train_script = os.path.join(PROJECT_ROOT, "modal_training", "train_gpu.py")
    os.system(f"modal run {train_script}")
