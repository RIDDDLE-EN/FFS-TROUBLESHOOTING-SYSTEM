from __future__ import annotations

from dataclasses import dataclass
import struct

_FMT = "<I ff BB ff B ff BB ff BB I B ff ff BB ffff BB i"
_SIZE = struct.calcsize(_FMT)

ENV_NORMAL = 0
ENV_LOW_TEMP = 1
ENV_HIGH_TEMP = 2
ENV_LOW_HUM = 3
ENV_HIGH_HUM = 4
ENV_CRITICAL_TEMP = 5
ENV_CRITICAL_HUM = 6
ENV_SENSOR_FAULT = 7

ENV_STATUS_NAMES = {
    ENV_NORMAL: "Normal",
    ENV_LOW_TEMP: "Low Temp",
    ENV_HIGH_TEMP: "High Temp",
    ENV_LOW_HUM: "Low Humidity",
    ENV_HIGH_HUM: "High Humidity",
    ENV_CRITICAL_TEMP: "CRITICAL Temp",
    ENV_CRITICAL_HUM: "CRITICAL Humidity",
    ENV_SENSOR_FAULT: "Sensor Fault",
}

@dataclass
class InterpretedSensorData:
    timestamp_ms: int = 0

    ambient_temp: float = 0.0
    ambient_hum: float = 0.0
    env_sensor_ok: bool = False
    env_status: int = 0

    current1: float = 0.0
    rpm1: float = 0.0
    motor1_running: bool = False

    current2: float = 0.0
    rpm2: float = 0.0
    motor2_running: bool = False

    weight_grams: float = 0.0
    loadcell_ok: bool = False
    weight_stable: bool = False

    roll_center_offset_cm: float = 0.0
    roll_centered: bool = True
    ultrasonic_ok: bool = False

    seal_temp: float = 0.0
    tc_connected: bool = False
    thermocouple_ok: bool = False

    bags_counted: int = 0
    bag_detected: bool = False
    bag_length_cm: float = 0.0

    vibration_rms_g: float = 0.0
    vibration_peak_g: float = 0.0
    vibration_freq_hz: float = 0.0
    vibration_freq_mag: float = 0.0
    vibration_knife_confirmed: bool = False

    calibration_active: bool = False
    cal_raw_weight: int = 0

    @classmethod
    def from_bytes(cls, payload: bytes) -> "InterpretedSensorData":
        if len(data) < _SIZE:
            return cls()

        fields = struct.unpack_from(_FMT, data)
        obj = cls()
        (
            obj.timestamp_ms,
            obj.ambient_temp, obj.ambient_hum,
            obj.env_sensor_ok, obj.env_status,
            obj.current1, obj.rpm1,
            obj.motor1_running,
            obj.current2, obj.rpm2,
            obj.motor2_running,
            obj.weight_grams, obj.weight_stable, obj.loadcell_ok,
            obj.roll_center_offset_cm,
            obj.roll_centered, obj.ultrasonic_ok,
            obj.bags_counted, obj.bag_detected,
            obj.bag_length_cm,
            obj.seal_temp, obj.tc_connected, obj.thermocouple_ok,
            obj.vibration_peak_g, obj.vibration_rms_g,
            obj.vibration_freq_mag, obj.vibration_freq_hz,
            obj.vibration_knife_confirmed,
            obj.calibration_active,
            obj.cal_raw_weight,
        ) = fields
        
        obj.env_sensor_ok = bool(obj.env_sensor_ok)
        obj.motor1_running = bool(obj.motor1_running)
        obj.motor2_running = bool(obj.motor2_running)
        obj.weight_stable = bool(obj.weight_stable)
        obj.loadcell_ok = bool(obj.loadcell_ok)
        obj.roll_centered = bool(obj.roll_centered)
        obj.ultrasonic_ok = bool(obj.ultrasonic_ok)
        obj.bag_detected = bool(obj.bag_detected)
        obj.tc_connected = bool(obj.tc_connected)
        obj.thermocouple_ok = bool(obj.thermocouple_ok)
        obj.vibration_knife_confirmed = bool(obj.vibration_knife_confirmed)
        obj.calibration_active = bool(obj.calibration_active)
        return obj

    def env_status_name(self) -> str:
        return ENV_STATUS_NAMES.get(self.env_status, "Unknown")

    def to_dict(self) -> dict:
        return {
            "timestamp_ms": self.timestamp_ms,
            "ambient_temp": self.ambient_temp,
            "ambient_hum": self.ambient_hum,
            "env_sensor_ok": self.env_sensor_ok,
            "env_status": self.env_status,
            "env_status_name": self.env_status_name,
            "current1": self.current1,
            "rpm1": self.rpm1,
            "motor1_running": self.motor1_running,
            "current2": self.current2,
            "rpm2": self.rpm2,
            "motor2_running": self.motor2_running,
            "weight_grams": self.weight_grams,
            "weight_stable": self.weight_stable,
            "loadcell_ok": self.loadcell_ok,
            "roll_center_offset_cm": self.roll_center_offset_cm,
            "roll_centered": self.roll_centered,
            "ultrasonic_ok": self.ultrasonic_ok,
            "bags_counted": self.bags_counted,
            "bag_detected": self.bag_detected,
            "bag_length_cm": self.bag_length_cm,
            "seal_temp": self.seal_temp,
            "tc_connected": self.tc_connected,
            "thermocouple_ok": self.thermocouple_ok,
            "vibration_peak_g": self.vibration_peak_g,
            "vibration_rms_g": self.vibration_rms_g,
            "vibration_freq_mag": self.vibration_freq_mag,
            "vibration_freq_hz": self.vibration_freq_hz,
            "vibration_knife_confirmed": self.vibration_knife_confirmed,
            "calibration_active": self.calibration_active,
            "cal_raw_weight": self.cal_raw_weight,
        }
