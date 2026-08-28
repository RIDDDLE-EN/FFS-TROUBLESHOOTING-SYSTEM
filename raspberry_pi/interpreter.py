from __future__ import annotations

from sensor_data import RawSensorData
import config as cfg


def classify_motor(current_a: float, rpm: float) -> dict:
    if current_a < 0.5 and rpm <= 0.1:
        return {"code": 0, "name": "Rest", "class": "ok"}
    if current_a >= cfg.MOTOR_JAM_CURRENT_A and rpm <= 0.1:
        return {"code": 3, "name": "JAMMED", "class": "critical"}
    if rpm < cfg.MOTOR_TENSION_RPM and current_a < cfg.MOTOR_JAM_CURRENT_A:
        return {"code": 2, "name": "Tension / Load", "class": "warning"}
    if rpm > 50 and current_a < 3.0:
        return {"code": 1, "name": "Normal", "class": "ok"}
    return {"code": 1, "name": "Normal", "class": "ok"}

ENV_STATUS_MAP = {
    0: "NORMAL",
    1: "LOW_TEMP",
    2: "HIGH_TEMP",
    3: "LOW_HUM",
    4: "HIGH_HUM",
    5: "CRITICAL_TEMP",
    6: "CRITICAL_HUM",
    7: "SENSOR_FAULT",
}


def interpret(raw: RawSensorData) -> dict:
    return {
        "timestamp_ms": raw.timestamp_ms,
        "env_sensor_ok": raw.env_sensor_ok,

        "motor1": {
            "current": round(raw.current1, 2),
            "rpm": round(raw.rpm1, 2),
            "state": classify_motor(raw.current1, raw.rpm1),
            "running": raw.motor1_running,
        },
        "motor2": {
            "current": round(raw.current2, 2),
            "rpm": round(raw.rpm2, 2),
            "state": classify_motor(raw.current2, raw.rpm2),
            "running": raw.motor2_running,
        },
        "loadcell": {
            "weight_g": round(raw.weight_grams, 2),
            "loadcell_ok": raw.loadcell_ok,
        },
        "temperature": {
            "ambient": round(raw.ambient_temp, 1),
            "humidity": round(raw.ambient_hum, 1),
            "status_code": raw.env_status,
            "status_name": ENV_STATUS_MAP.get(raw.env_status, "UNKNOWN"),
            "thermocouple": round(raw.seal_temp, 1),
            "tc_connected": raw.tc_connected,
            "thermocouple_ok": raw.thermocouple_ok,
        },
        "roll": {
            "diff_cm": round(raw.roll_center_offset_cm, 2),
            "centered": raw.roll_centered,
            "ultrasonic_ok": raw.ultrasonic_ok,
        },
        "vibration": {
            "impact_amplitude": round(raw.impact_amplitude, 2),
            "knife_frequency": round(raw.knife_frequency, 2),
            "is_updated": raw.vibration_updated,
        },
        "bag": {
            "bags_counted": int(raw.bags_counted),
            "bag_length_cm": round(raw.bag_length_cm, 2),
        },
        "calibration": {
            "active": raw.calibration_active,
            "raw_weight": raw.cal_raw_weight,
        },
    }
