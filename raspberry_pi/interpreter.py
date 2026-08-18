from __future__ import annotations

from sensor_data import InterpretedSensorData
import config as cfg


def classify_motor(current_a: float, rpm: float) -> dict:
    if current_a >= cfg.MOTOR_JAM_CURRENT_A and rpm <= 0.1:
        return {"code": 3, "name": "JAMMED", "class": "critical"}
    if rpm < cfg.MOTOR_TENSION_RPM and current_a < cfg.MOTOR_JAM_CURRENT_A:
        return {"code": 2, "name": "Tension / Load", "class": "warning"}
    if rpm > 50 and current_a < 3.0:
        return {"code": 1, "name": "Normal", "class": "ok"}
    if current_a < 0.5 and rpm <= 0.1:
        return {"code": 0, "name": "Rest", "class": "ok"}
    return {"code": 1, "name": "Normal", "class": "ok"}


def interpret(s: InterpretedSensorData) -> dict:
    if not s:
        return {}

    feed_current = max(0.0, s.current1)
    feed_rpm = max(0.0, s.rpm1)

    return {
        "motor1": {
            "current": round(feed_current, 2),
            "rpm": round(feed_rpm, 1),
            "state": classify_motor(feed_current, feed_rpm),
            "running": bool(s.motor1_running),
        },
        "motor2": {
            "current": 0.0,
            "rpm": 0.0,
            "state": {"code": 0, "name": "Removed", "class": "secondary"},
            "running": False,
        },
        "weight": {
            "kg": round(s.weight_grams / 1000.0, 3),
            "calibrated": bool(s.loadcell_ok),
            "stable": bool(s.weight_stable),
        },
        "temperature": {
            "ambient": round(s.ambient_temp, 1),
            "humidity": round(s.ambient_hum, 1),
            "thermocouple": round(s.seal_temp, 1),
            "tc_connected": bool(s.tc_connected),
            "fan_required": False,
        },
        "roll": {
            "diff_cm": round(s.roll_center_offset_cm, 2),
            "centered": bool(s.roll_centered),
        },
        "vibration": {
            "rms_g": round(s.vibration_rms_g, 3),
            "peak_g": round(s.vibration_peak_g, 3),
            "dominant_freq_hz": round(s.vibration_freq_hz, 1),
        },
        "bags_counted": int(s.bags_counted),
        "bag_detected": bool(s.bag_detected),
        "bag_length_cm": round(s.bag_length_cm, 1),
        "env_status": s.env_status_name(),
        "env_sensor_ok": bool(s.env_sensor_ok),
        "feed_mode": "single_motor_stepwise",
    }
