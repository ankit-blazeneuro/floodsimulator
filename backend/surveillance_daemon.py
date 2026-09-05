"""
HydroGuard-AI: 24/7 Autonomous Weather Surveillance Daemon

Runs continuously in the background independent of any GUI application:
- Polls Open-Meteo across Indian dam clusters
- Detects slow (3h+), moderate (2h+), and heavy (1h+) precipitation
- Compiles dam breach failure probabilities on Modal serverless GPU in batch
- Updates backend/models/active_surveillance.json
"""

import sys
import os
import time
import asyncio
import logging

BACKEND_DIR = os.path.dirname(os.path.abspath(__file__))
if BACKEND_DIR not in sys.path:
    sys.path.insert(0, BACKEND_DIR)

from src.weather_surveillance import surveillance_engine
from src.predictor import DamBreachPredictor

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s"
)
logger = logging.getLogger("surveillance_daemon")


async def main():
    logger.info("🌊 HydroGuard-AI Weather Surveillance Daemon Started.")
    logger.info("Monitored Registry: %d Indian Dams", len(surveillance_engine.dams))
    logger.info("Surveillance Trigger Criteria:")
    logger.info("  1. Heavy Rain (1h+): >= 15.0 mm/hr")
    logger.info("  2. Moderate Rain (2h+): >= 7.5 mm/hr for 2h (or >= 15mm)")
    logger.info("  3. Slow Rain (3h+): >= 2.0 mm/hr for 3h (or >= 7.5mm)")
    logger.info("Compute Backend: Modal.com Serverless GPU")

    # Initialize local fallback
    surveillance_engine.local_predictor = DamBreachPredictor(model_dir=os.path.join(BACKEND_DIR, "models"))

    while True:
        try:
            logger.info("🛰️  Starting nationwide Open-Meteo scan...")
            results = await surveillance_engine.scan_all_dams()
            imminent = sum(1 for r in results if r.get("alert_level") == "IMMINENT")
            warning = sum(1 for r in results if r.get("alert_level") == "WARNING")
            watch = sum(1 for r in results if r.get("alert_level") == "WATCH")
            normal = sum(1 for r in results if r.get("alert_level") == "NORMAL")
            logger.info("✅ Scan complete. %d dams under surveillance (🔴 Imminent: %d, 🟠 Warning: %d, 🟡 Watch: %d, 🟢 Normal: %d)",
                        len(results), imminent, warning, watch, normal)
        except Exception as e:
            logger.error("Error during surveillance cycle: %s", e)

        # Sleep 5 minutes before next meteorological scan
        await asyncio.sleep(300)


if __name__ == "__main__":
    asyncio.run(main())
