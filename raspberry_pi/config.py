# config.py — Raspberry Pi backend configuration
# BCM numbering is used everywhere.

# Network/API
BACKEND_HOST = "0.0.0.0"
BACKEND_PORT = 5001
API_KEY = "ffs-troubleshooting-v1-2026"
DB_PATH = "ffts.db"
DB_WRITE_EVERY = 20
DB_PURGE_DAYS = 30

# SPI (Pi is master, ESP32 is slave)
SPI_BUS = 0
SPI_DEVICE = 0
SPI_SPEED_HZ = 1_000_000
SPI_MODE = 0
SPI_PACKET_SIZE = 256
SPI_POLL_HZ = 20
SPI_DATA_READY_PIN = 17

# Single feeding motor (L298N or similar H-bridge)
FEED_EN, FEED_IN1, FEED_IN2 = 12, 5, 6
FEED_MOTOR_PWM_FREQ = 1000
FEED_MOTOR_DEFAULT_DC = 70

# Fan output (relay or transistor driver)
FAN_PIN = 4
FAN_ACTIVE_HIGH = True

# Stepper motor for roll centering / rack & pinion
STEPPER_STEP, STEPPER_DIR, STEPPER_EN = 27, 18, 11
STEPPER_STEPS_PER_REV = 200
STEPPER_MICROSTEP = 16
STEPPER_MM_PER_REV = 8.0
STEPPER_STEP_DELAY_S = 0.0005
STEPPER_MAX_MM = 50.0

# Roll-centering PID tuning
ROLL_TOLERANCE_CM = 0.25
ROLL_PID_KP = 1.25     # mm per cm error
ROLL_PID_KI = 0.02
ROLL_PID_KD = 0.08
ROLL_PID_MAX_MM = 3.0
ROLL_PID_INTERVAL_S = 0.5

# Environment / fan thresholds
TEMP_FAN_ON_C = 32.0
TEMP_FAN_OFF_C = 29.0
HUM_FAN_ON_PCT = 80.0
HUM_FAN_OFF_PCT = 70.0

# Packaging/feeding behavior
FEED_PULSE_ON_S = 0.35
FEED_PULSE_OFF_S = 0.20
FEED_BATCH_PULSES = 1
FEED_DEFAULT_SPEED = 70

# Existing cycle timings kept for compatibility
CYCLE_FEED_ADVANCE_S = 1.2
CYCLE_SETTLE_S = 0.1
CYCLE_STEPPER_SETTLE_S = 0.2
BAG_LENGTH_TOLERANCE_CM = 0.5

# Alert thresholds
MOTOR_JAM_CURRENT_A = 4.0
MOTOR_TENSION_RPM = 20
VIB_WARN_G = 1.5
VIB_CRIT_G = 2.5
ROLL_MISALIGN_CM = 1.5
IDLE_WARN_MINUTES = 5

COOLDOWN = {
    "m1_jam": 30,
    "machine_idle": 60,
    "env_crit": 30,
    "roll_off": 60,
    "vib_warn": 120,
    "vib_crit": 60,
    "env_warn": 120,
}

# SPI protocol constants
SPI_START_FROM_PI = 0xAA
SPI_START_FROM_ESP = 0xBB
CMD_PING = 0x01
CMD_READ_SENSORS = 0x02
CMD_READ_LOG = 0x03
CMD_CAL_START = 0x04
CMD_CAL_FACTOR = 0x05
CMD_TARE = 0x06
CMD_RESET_BAGS = 0x07
CMD_SET_THERMO_OFFSET = 0x08

MSG_PONG = 0x81
MSG_SENSOR_DATA = 0x82
MSG_LOG = 0x83
MSG_CAL_RAW_WEIGHT = 0x84
MSG_ACK = 0x85
MSG_NACK = 0x86
MSG_IDLE = 0x87
