from __future__ import annotations
import logging, time

log = logging.getLogger("motors")

_feed_speed = 60
_fan_on = False
_stepper_pos_mm = 0.0

class PackagingCycle:
    def __init__(self, spi):
        self.spi = spi
        self._running = False

    def start(self):
        self._running = True
        log.info("PackagingCycle armed")

    def stop(self):
        self._running = False
        log.info("PackagingCycle stopped")

    def is_running(self):
        return self._running

    def pulse_feed(self, pulses=1, on_s=None, off_s=None):
        log.info("Feed batch pulse x%d", pulses)

def motors_init():
    log.info("Single-feed motor, stepper and fan initialized")

def motors_cleanup():
    log.info("Motors cleaned up")

def set_feed_speed(speed: int):
    global _feed_speed
    _feed_speed = int(speed)

def feed_batch(pulses=1, on_s=None, off_s=None):
    log.info("Feed motor batch executed x%d", pulses)

def fan_on():
    global _fan_on
    _fan_on = True
    log.info("Fan Turned on")

def fan_off():
    global _fan_on
    _fan_on = False
    log.info("Fan turned off")

def fan_enabled():
    return _fan_on

def auto_control_environment(temp: float, hum: float, env_ok: bool):
    if not env_ok:
        return False
    if temp >=31.0 or hum >= 72.0:
        fan_on()
        return True
    if temp <= 28.5 and hum <= 65.0:
        fan_off()
        return False
    return _fan_on

def auto_center_roll(offset_cm: float, centered: bool, ultrasonic_ok: bool):
    global _stepper_pos_mm
    if not ultrasonic_ok:
        return 0.0
    if centered:
        return 0.0
    step_mm = max(-10.0, min(10.0, -offset_cm * 8.0))
    _stepper_pos_mm += step_mm
    log.info("Stepper recentering by %.2f mm", step_mm)
    return step_mm

def retract_all():
    log.info("Retracting outputs")

def stepper_get_position_mm():
    return _stepper_pos_mm
