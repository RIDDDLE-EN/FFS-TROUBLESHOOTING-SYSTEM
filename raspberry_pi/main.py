from __future__ import annotations

import logging
import signal
import sys
import threading
import time

try:
    import RPi.GPIO as GPIO  # type: ignore
except Exception:  # pragma: no cover
    class _GPIOStub:
        BCM = OUT = IN = HIGH = LOW = None
        def setmode(self, *a, **k): pass
        def setwarnings(self, *a, **k): pass
        def cleanup(self): pass
    GPIO = _GPIOStub()  # type: ignore

import config as cfg
from alerts import AlertEngine
from database import Database
from motors import PackagingCycle, motors_cleanup, motors_init
from pi_backend import app, S, _fire_alert, telemetry_loop
from spi_comms import SpiComms

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    datefmt="%H:%M:%S",
)

log = logging.getLogger("main")

spi = SpiComms()
cycle = None
db = None


def _shutdown(signum, frame):
    log.info("Shutdown signal received")
    try:
        if cycle:
            cycle.stop()
    except Exception:
        pass
    try:
        motors_cleanup()
    except Exception:
        pass
    try:
        spi.close()
    except Exception:
        pass
    try:
        GPIO.cleanup()
    except Exception:
        pass
    sys.exit(0)


signal.signal(signal.SIGINT, _shutdown)
signal.signal(signal.SIGTERM, _shutdown)


log.info("=" * 60)
log.info("Starting FFS Raspberry Pi backend")
log.info("=" * 60)

db = Database(cfg.DB_PATH)
alerts = AlertEngine(fire_cb=_fire_alert)

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

spi.open()

log.info("Pinging ESP32...")
for _ in range(5):
    if spi.ping():
        log.info("ESP32 online")
        break
    time.sleep(0.5)
else:
    log.warning("ESP32 ping not confirmed; continuing in compatibility mode")

motors_init()
cycle = PackagingCycle(spi)
cycle.start()

if S.db:
    _emit_log = lambda tag, msg, lvl="INFO": (S.db.add_log(tag, msg, lvl), log.info("[%s] %s",tag.upper(), msg))
    _emit_log("SYSTEM", "Backend initialized via Gunicorn", "INFO")

threading.Thread(target=telemetry_loop, daemon=True, name="TelemetryLoop").start()
