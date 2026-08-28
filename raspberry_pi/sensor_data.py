"""Decode the ESP32 sensor payload into Python objects.

The ESP32 can keep its existing payload shape. The Pi only uses the values
that matter for the simplified machine:
- feed motor telemetry
- DHT environment values
- roll centering offset
- vibration
- bag counting
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

SENSOR_STRUCT_FMT = "<Iff?Bff?ff?f??f??Iff??ff??i"
SENSOR_STRUCT_SIZE = struct.calcsize(SENSOR_STRUCT_FMT)

@dataclass
class RawSensorData:
    timestamp_ms: int

    ambient_temp: float
    ambient_hum: float
    env_sensor_ok: bool
    env_status: int

    current1: float
    rpm1: float
    motor1_running: bool

    current2: float
    rpm2: float
    motor2_running: bool

    weight_grams: float
    loadcell_ok: bool

    roll_center_offset_cm: float
    roll_centered: bool
    ultrasonic_ok: bool

    bags_counted: int
    bag_length_cm: float

    seal_temp: float
    tc_connected: bool
    thermocouple_ok: bool

    impact_amplitude: float
    knife_frequency: float
    vibration_updated: bool

    calibration_active: bool
    cal_raw_weight: int

    @classmethod
    def unpack(cls, buffer: bytes) -> Optional["RawSensorData"]:
        if not buffer or len(buffer) < SENSOR_STRUCT_SIZE:
            return None
        fields = struct.unpack(SENSOR_STRUCT_FMT, buffer[:SENSOR_STRUCT_SIZE])
        return cls(*fields)

    @classmethod
    def from_bytes(cls, buffer: bytes) -> Optional["RawSensorData"]:
        return cls.unpack(buffer)
