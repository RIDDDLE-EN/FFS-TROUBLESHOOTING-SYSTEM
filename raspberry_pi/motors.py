from __future__ import annotations

import logging
import threading
import time
from dataclasses import dataclass

import config as cfg

try:
    import RPi.GPIO as GPIO  
except Exception:  
    class _PWM:
        def __init__(self, pin, freq):
            self.pin = pin
            self.freq = freq
            self.duty = 0

        def start(self, duty):
            self.duty = duty

        def ChangeDutyCycle(self, duty):
            self.duty = duty

        def stop(self):
            self.duty = 0

    class _GPIOStub:
        BCM = OUT = IN = HIGH = LOW = None
        PUD_UP = PUD_DOWN = None

        def setmode(self, *a, **k): pass
        def setwarnings(self, *a, **k): pass
        def setup(self, *a, **k): pass
        def output(self, *a, **k): pass
        def input(self, *a, **k): return 0
        def cleanup(self, *a, **k): pass
        def PWM(self, pin, freq): return _PWM(pin, freq)

    GPIO = _GPIOStub()  

log = logging.getLogger(__name__)

_pwm_feed = None
_fan_enabled = False
_feed_duty = cfg.FEED_MOTOR_DEFAULT_DC
_stepper_pos_mm = 0.0
_stepper_lock = threading.Lock()
_pid_integral = 0.0
_pid_prev_error = 0.0
_pid_last_ts = 0.0
_pid_last_move_ts = 0.0


def motors_init():
    global _pwm_feed

    for pin in (cfg.FEED_EN, cfg.FEED_IN1, cfg.FEED_IN2, cfg.FAN_PIN,
                cfg.STEPPER_STEP, cfg.STEPPER_DIR, cfg.STEPPER_EN):
        try:
            GPIO.setup(pin, GPIO.OUT, initial=GPIO.LOW)
        except TypeError:
            GPIO.setup(pin, GPIO.OUT)

    _pwm_feed = GPIO.PWM(cfg.FEED_EN, cfg.FEED_MOTOR_PWM_FREQ)
    _pwm_feed.start(0)

    GPIO.output(cfg.STEPPER_EN, GPIO.HIGH)
    fan_off()
    feed_off()
    log.info("Single-feed motor, stepper and fan initialized")


def motors_cleanup():
    try:
        feed_off()
        fan_off()
        GPIO.output(cfg.STEPPER_EN, GPIO.HIGH)
        if _pwm_feed:
            _pwm_feed.stop()
    finally:
        log.info("Motors cleaned up")


def set_feed_duty(duty: int):
    global _feed_duty
    _feed_duty = max(0, min(100, int(duty)))
    log.info("Feed duty set to %d%%", _feed_duty)


def set_feed_speed(duty: int):
    set_feed_duty(duty)


def feed_on():
    GPIO.output(cfg.FEED_IN1, GPIO.HIGH)
    GPIO.output(cfg.FEED_IN2, GPIO.LOW)
    if _pwm_feed:
        _pwm_feed.ChangeDutyCycle(_feed_duty)
    log.debug("Feed motor ON (%d%%)", _feed_duty)


def feed_off():
    if _pwm_feed:
        _pwm_feed.ChangeDutyCycle(0)
    GPIO.output(cfg.FEED_IN1, GPIO.LOW)
    GPIO.output(cfg.FEED_IN2, GPIO.LOW)
    log.debug("Feed motor OFF")


def feed_pulse(on_s: float | None = None, off_s: float | None = None):
    on_s = cfg.FEED_PULSE_ON_S if on_s is None else max(0.01, float(on_s))
    off_s = cfg.FEED_PULSE_OFF_S if off_s is None else max(0.0, float(off_s))
    feed_on()
    time.sleep(on_s)
    feed_off()
    time.sleep(off_s)


def feed_batch(pulses: int = 1, on_s: float | None = None, off_s: float | None = None):
    pulses = max(1, int(pulses))
    for _ in range(pulses):
        feed_pulse(on_s=on_s, off_s=off_s)


def fan_on():
    global _fan_enabled
    level = GPIO.HIGH if cfg.FAN_ACTIVE_HIGH else GPIO.LOW
    GPIO.output(cfg.FAN_PIN, level)
    _fan_enabled = True
    log.debug("Fan ON")


def fan_off():
    global _fan_enabled
    level = GPIO.LOW if cfg.FAN_ACTIVE_HIGH else GPIO.HIGH
    GPIO.output(cfg.FAN_PIN, level)
    _fan_enabled = False
    log.debug("Fan OFF")


def fan_enabled() -> bool:
    return _fan_enabled


def auto_control_environment(temp_c: float, hum_pct: float, sensor_ok: bool = True):
    if not sensor_ok:
        fan_on()
        return True

    if temp_c >= cfg.TEMP_FAN_ON_C or hum_pct >= cfg.HUM_FAN_ON_PCT:
        fan_on()
    elif temp_c <= cfg.TEMP_FAN_OFF_C and hum_pct <= cfg.HUM_FAN_OFF_PCT:
        fan_off()
    return fan_enabled()


def _steps_per_mm() -> float:
    return (cfg.STEPPER_STEPS_PER_REV * cfg.STEPPER_MICROSTEP) / cfg.STEPPER_MM_PER_REV


def stepper_move_mm(delta_mm: float):
    global _stepper_pos_mm

    delta_mm = float(delta_mm)
    if abs(delta_mm) < 1e-6:
        return

    with _stepper_lock:
        target = max(-cfg.STEPPER_MAX_MM, min(cfg.STEPPER_MAX_MM, _stepper_pos_mm + delta_mm))
        actual_delta = target - _stepper_pos_mm
        if abs(actual_delta) < 0.01:
            return

        direction = GPIO.HIGH if actual_delta > 0 else GPIO.LOW
        GPIO.output(cfg.STEPPER_DIR, direction)
        GPIO.output(cfg.STEPPER_EN, GPIO.LOW)

        steps = max(1, int(round(abs(actual_delta) * _steps_per_mm())))
        half = cfg.STEPPER_STEP_DELAY_S / 2.0
        for _ in range(steps):
            GPIO.output(cfg.STEPPER_STEP, GPIO.HIGH)
            time.sleep(half)
            GPIO.output(cfg.STEPPER_STEP, GPIO.LOW)
            time.sleep(half)

        GPIO.output(cfg.STEPPER_EN, GPIO.HIGH)
        _stepper_pos_mm = target
        log.info("Stepper moved %.2f mm -> %.2f mm", actual_delta, _stepper_pos_mm)

def stepper_get_position() -> float:
    return _stepper_pos_mm

def auto_center_roll(offset_cm: float, centered: bool = False, ultrasonic_ok: bool = True):
    global _pid_integral, _pid_prev_error, _pid_last_ts, _pid_last_move_ts

    if not ultrasonic_ok:
        return 0.0

    now = time.monotonic()
    if _pid_last_ts <= 0.0:
        _pid_last_ts = now
        _pid_prev_error = 0.0
        _pid_integral = 0.0

    dt = max(1e-3, now - _pid-last_ts)
    _pid_last_ts = now

    error_cm = float(offset_cm)
    if centered and abs(error_cm) <= cfg.ROLL_TOLERANCE_CM:
        _pid_integral = 0.0
        _pid_prev_error = error_cm
        return 0.0

    if abs(error_cm) <= cfg.ROLL_TORANCE_CM:
        return 0.0

    _pid_integral += error_cm * dt
    _pid_integral = max(-10.0, min(10.0, _pid_integral))
    derivative = (error_cm - pid_prev_error) / dt
    _pid_prev_error = error_cm

    mm = (
        (cfg.ROLL_PID_KP * error_cm)
        + (cfg.ROLL_PID_KI * _pid_integral)
        + (cfg.ROLL_PID_KD * derivative)
    )
    mm = max(-cfg.ROLL_PID_MAX_MM, min(cfg.ROLL_PID_MAX_MM, mm))

    if now - _pid_last_move_ts >= cfg.ROLL_PID_INTERVAL_S:
        _pid_last_move_ts = now
        stepper_move_mm(mm)

    return mm

def retract_all():
    feed_off()
    fan_off()
    GPIO.output(cfg.STEPPER_EN, GPIO.HIGH)

def _feed_on():
    feed_on()

def _feed_off():
    feed_off()

def set_feed_duty_cycle(duty: int):
    set_feed_ducy(duty)

def stepper_move(delta_mm: float):
    stepper_move_mm(delta_mm)

@dataclass
class PackagingCycle:
    spi: object | None = None

    def __post_init__(self):
        self._running = False
        self._lock = threading.Lock()
        self._last_pulse_ts = 0.0

    def start(self):
        self._running = True
        log.info("PackagingCycle armed")

    def stop(self):
        self._running = False
        retract_all()
        log.info("PackagingCycle stopped")

    def is_running(self) -> bool:
        return self._running

    def pulse_feed(self, pulses: int = cfg.FEED_BATCH_PULSES, on_s: float | None = None, off_s: float | None = None):
        with self._lock:
            feed_batch(pulses=pulses, on_s=on_s, off_s=off_s)
            self._last_pulse_ts = time.time()

    def feed_step(self):
        self.pulse_feed(1)

    def last_action_ts(self) -> float:
        return self._last_pulse_ts
