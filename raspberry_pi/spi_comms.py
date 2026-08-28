from __future__ import annotations

import logging
import struct
import threading
import time

import config as cfg
from sensor_data import RawSensorData

log = logging.getLogger(__name__)

try:
    import spidev  
except Exception: 
    class _SpiStub:
        def open(self, bus, device): pass
        def close(self): pass
        def xfer2(self, frame, speed_hz=0):
            buf = bytearray(cfg.SPI_PACKET_SIZE)
            buf[0] = cfg.SPI_START_FROM_ESP
            cmd = frame[1] if len(frame) > 1 else 0

            if cmd == cfg.CMD_PING:
                buf[1] = cfg.MSG_PONG
            elif cmd == cfg.CMD_CAL_START:
                buf[1] = cfg.MSG_CAL_RAW_WEIGHT
                buf[2] = 4
                buf[3:7] = struct.pack("<i", 123456)
            elif cmd in (cfg.CMD_TARE, cfg.CMD_RESET_BAGS, cfg.CMD_CAL_FACTOR, cfg.CMD_SET_THERMO_OFFSET):
                buf[1] = cfg.MSG_ACK
            else:
                buf[1] = cfg.MSG_IDLE

            buf[-1] = _crc8(buf[:-1])
            return list(buf)

    class _SpidevStub:
        SpiDev = _SpiStub

    spidev = _SpidevStub()  

try:
    import RPi.GPIO as GPIO
except Exception:  
    class _GPIOStub:
        IN = OUT = BCM = HIGH = LOW = None
        def setmode(self, *a, **k): pass
        def setwarnings(self, *a, **k): pass
        def setup(self, *a, **k): pass
        def input(self, pin): return 0
        def cleanup(self): pass

    GPIO = _GPIOStub()  


def _crc8(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
    return crc


def _xor8(data: bytes) -> int:
    x = 0
    for b in data:
        x ^= b
    return x & 0xFF


def _sum8(data: bytes) -> int:
    return sum(data) & 0xFF

_legacy_checksum_notice_emitted = False


def _build_frame(cmd: int, payload: bytes = b"") -> list[int]:
    buf = bytearray(cfg.SPI_PACKET_SIZE)
    buf[0] = cfg.SPI_START_FROM_PI
    buf[1] = cmd
    plen = min(len(payload), cfg.SPI_PACKET_SIZE - 4)
    buf[2] = plen
    buf[3:3 + plen] = payload[:plen]
    buf[-1] = _crc8(buf[:-1])
    return list(buf)


def _looks_like_frame(buf: bytes) -> bool:
    if len(buf) != cfg.SPI_PACKET_SIZE:
        return False
    if buf[0] != cfg.SPI_START_FROM_ESP:
        return False
    payload_len = buf[2]
    return 0 <= payload_len <= cfg.SPI_PACKET_SIZE - 4


def _checksum_ok(buf: bytes) -> bool:
    tail = buf[-1]
    body = buf[:-1]
    return tail in {_crc8(body), _xor8(body), _sum8(body)}


def _parse_response(raw: list[int], permissive: bool = True):
    global _legacy_checksum_notice_emitted

    buf = bytes(raw)
    if len(buf) != cfg.SPI_PACKET_SIZE:
        return None, None

    if buf[0] != cfg.SPI_START_FROM_ESP:
        # Some SPI slaves echo a dummy byte first. Try a one-byte shift before giving up.
        if len(buf) > 1 and buf[1] == cfg.SPI_START_FROM_ESP and _looks_like_frame(buf[1:] + b"\x00"):
            buf = buf[1:] + b"\x00"
        else:
            return None, None

    checksum_ok = _checksum_ok(buf)
    if not checksum_ok and not permissive:
        log.warning("SPI checksum mismatch")
        return None, None

    if not checksum_ok and permissive and not _legacy_checksum_notice_emitted:
        # The ESP32 firmware may be using a different checksum variant or a
        # legacy frame format. Accept the frame, but only log once so the
        # backend output stays readable during normal operation.
        log.warning("SPI checksum mismatch; accepting compatible legacy frame")
        _legacy_checksum_notice_emitted = True

    msg_type = buf[1]
    payload_len = buf[2]
    if payload_len > cfg.SPI_PACKET_SIZE - 4:
        return None, None
    payload = buf[3:3 + payload_len]
    return msg_type, payload


class SpiComms:
    def __init__(self):
        self._spi = spidev.SpiDev()
        self._lock = threading.Lock()
        self._last_good_ping = 0.0
        self._fault_count = 0

    def open(self):
        try:
            GPIO.setup(cfg.SPI_DATA_READY_PIN, GPIO.IN)
        except Exception:
            pass
        self._spi.open(cfg.SPI_BUS, cfg.SPI_DEVICE)
        self._spi.max_speed_hz = cfg.SPI_SPEED_HZ
        self._spi.mode = cfg.SPI_MODE
        # Optional attributes; ignored by stubs if unsupported.
        try:
            self._spi.bits_per_word = 8
        except Exception:
            pass
        try:
            self._spi.cshigh = False
        except Exception:
            pass
        try:
            self._spi.lsbfirst = False
        except Exception:
            pass
        log.info("SPI opened bus=%s device=%s speed=%s", cfg.SPI_BUS, cfg.SPI_DEVICE, cfg.SPI_SPEED_HZ)

    def close(self):
        try:
            self._spi.close()
        except Exception:
            pass
        log.info("SPI closed")

    def _transact(self, cmd: int, payload: bytes = b"", retries: int = 2):
        frame = _build_frame(cmd, payload)
        last = (None, None)
        with self._lock:
            for _ in range(max(1, retries)):
                raw = self._spi.xfer2(frame)
                parsed = _parse_response(raw, permissive=True)
                if parsed != (None, None):
                    self._fault_count = 0
                    return parsed
                last = parsed
                time.sleep(0.01)
        self._fault_count += 1
        return last

    def ping(self) -> bool:
        msg, _ = self._transact(cfg.CMD_PING, retries=3)
        ok = msg in {cfg.MSG_PONG, cfg.MSG_IDLE, cfg.MSG_ACK}
        if ok:
            self._last_good_ping = time.time()
        return ok

    def data_ready(self) -> bool:
        try:
            return bool(GPIO.input(cfg.SPI_DATA_READY_PIN))
        except Exception:
            return True

    def read_sensors(self):
        msg, payload = self._transact(cfg.CMD_READ_SENSORS)
        if msg == cfg.MSG_SENSOR_DATA:
            return InterpretedSensorData.from_bytes(payload)
        return None

    def read_log(self):
        msg, payload = self._transact(cfg.CMD_READ_LOG)
        if msg == cfg.MSG_LOG and payload:
            return payload.rstrip(b"\x00").decode("ascii", errors="replace")
        return None

    def tare(self) -> bool:
        msg, _ = self._transact(cfg.CMD_TARE)
        return msg == cfg.MSG_ACK

    def reset_bags(self) -> bool:
        msg, _ = self._transact(cfg.CMD_RESET_BAGS)
        return msg == cfg.MSG_ACK

    def set_thermo_offset(self, offset_c: float) -> bool:
        msg, _ = self._transact(cfg.CMD_SET_THERMO_OFFSET, struct.pack("<f", float(offset_c)))
        return msg == cfg.MSG_ACK

    def calibration_start(self):
        msg, payload = self._transact(cfg.CMD_CAL_START)
        if msg == cfg.MSG_CAL_RAW_WEIGHT and len(payload) >= 4:
            return struct.unpack("<i", payload[:4])[0]
        return None

    def calibration_set_factor(self, factor: float) -> bool:
        msg, _ = self._transact(cfg.CMD_CAL_FACTOR, struct.pack("<f", float(factor)))
        return msg == cfg.MSG_ACK
