from __future__ import annotations
import time
from dataclasses import dataclass, field
from typing import Callable, Any

import config as cfg

@dataclass
class AlertState:
    last_fire: dict[str, float] = field(default_factory=dict)

    def allowed(self, key: str) -> bool:
        now = time.time()
        cooldown = cfg.COOLDOWN.get(key, 60)
        last = self.last_fire.get(key, 0.0)
        if now - last >= cooldown:
            self.last_fire[key] = now
            return True
        return False

class AlertEngine:
    def __init__(self, fire_cb: Callable[[str, str, str, str], Any] | None = None):
        self.fire_cb = fire_cb
        self.state = AlertState()

    def _fire(self, key: str, severity: str, category: str, message: str):
        if self.state.allowed(key) and self.fire_cb:
            self.fire_cb(key, severity, category, message)

    def evaluate(self, data: dict):
        temp = float(data.get("temperature", {}).get("ambient", 0.0))
        hum = float(data.get("temperature", {}).get("humidity", 0.0))
        env_ok = bool(data.get("env_sensor_ok", True))
        roll = data.get("roll", {})
        centered = bool(roll.get("centered", True))
        diff_cm = abs(float(roll.get("diff_cm", 0.0)))
        vib = data.get("vibration", {})
        peak = float(vib.get("peak_g", 0.0))
        motor = data.get("motor1", {})
        current = float(motor.get("current", 0.0))
        rpm = float(motor.get("rpm", 0.0))

        if not env_ok:
            self._fire("env_cirt", "CRITICAL", "ENV", "DHT sensor fault detected.")
            return

        if temp >= cfg.TEMP_FAN_OK_C or hum >= cfg.HUM_FAN_ON_PCT:
            self._fire(
                "env_warn",
                "WARNING",
                "ENV",
                F"Environment out of range: {temp:.1f}C / {hum:.1f}%",
            )

        if diff_cm >= cfg.ROLL_MISALIGN_CM or not centered:
            self._fire(
                "roll_off",
                "WARNING",
                "ROLL",
                F"Roll is off-center by {diff_cm:.2f} cm.",
            )

        if current >= cfg.MOTOR_JAM_CURRENT_A and rpm <= 0.1:
            self._fire("m1_jam", "CRITICAL", "MOTOR", "Feeding motor appears Jammed.")

        if peak >= cfg.VIB_CRIT_G:
            self._fire("vib_crit", "CRITICAL", "VIBRATION", f"High vibration detected: {peak:.2f} g.")
        elif peak >= cfg.VIB_WARN_G:
            self._fire("vib_warn", "WARNING", "VIBRATION", f"Vibration rising: {peak:.2f} g.")
