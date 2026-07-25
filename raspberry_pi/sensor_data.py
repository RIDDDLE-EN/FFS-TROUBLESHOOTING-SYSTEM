from __future__ import annotations

from dataclasses import dataclass, field
import struct
from typing import ClassVar

def _u16(data: bytes, idx: int) -> int:
    return struct.unpack_from("<H", data, idx)[0]

def _i16(data: bytes, idx: int) -> int:
    return struct.unpac_from("<h", data, idx)[0]

@dataclass
class InterpretedSensorData:
    timestamp_ms: int = 0
    current1: float = 0.0
    rpm1: float = 0.0
    motor1_running: bool = False
    weight_grams: float = 0.0
    loadcell_ok: bool = False
    weight_stable: bool = False
    ambient_temp: float = 0.0
    ambient_hum: float = 0.0
    seal_temp: float = 0.0
    tc_connected: bool = False
    roll_center_offset_cm: float = 0.0
    roll_centered: bool = True
    vibration_rms_g: float = 0.0
    vibration_peak_g: float = 0.0
    vibration_freq_hz: float = 0.0
    bags_counted: int = 0
    bag_detected: bool = False
    bag_length_cm: float = 0.0
    env_sensor_ok: bool = True
    env_status_code: int = 0
    raw_flags: int = 0

    _MIN_SIZE: ClassVar[int] = 30

    @classmethod
    def from_bytes(cls, payload: bytes) -> "InterpretedSensorData":
        if not payload or len(payload) < cls._MIN_SIZE:
            return cls()
        p = payload.ljust(48, b"\x00")
        tx = struct.unpack_from("<I", p, 0)[0]
        current1 = _u16(p, 4) / 100.0
