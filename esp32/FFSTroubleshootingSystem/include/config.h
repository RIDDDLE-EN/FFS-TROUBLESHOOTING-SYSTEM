#pragma once

// Temperature and Humidity
#define DHT_PIN		4

// Motor current
#define ACS712_1	34
#define ACS712_2	35

// Motor RPM
#define ENCODER1_CLK	32
#define ENCODER1_DT	33
#define ENCODER2_CLK	25
#define ENCODER2_DT	26

// Bag length
#define LDR_PIN		27

// LoadCell
#define HX_DT		21
#define HX_SCK		22

// Thermocouple
#define TC_PLUS		14
#define TC_CONTINUITY	12

// Knife Vibration
#define MPU_SDA		21
#define MPU_SCL		22

// SPI
#define MOSI		23
#define MISO		19
#define SPI_CLK		18
#define SPI_CS		5
#define SPI_DATA_READY	17

// LDR Thresholds
#define LDR_BLOCKED_THRESHOLD 	1000
#define LDR_UNBLOCKED_THRESHOLD	2000

// ACS712 30A 
#define VREF		3.3f
#define ADC_RESOLUTION	4095.0f
#define ZERO_CURRENT_V	1.65f
#define SENSITIVITY	0.066f

// Rotary encoder
#define ENCODER_PPR	20
#define ENCODER_STOPPED_US	500000UL

// Thermocouple 
#define MV_PER_DEGC	0.041f
#define ADC_TO_MV(adc)	((adc) * (3300.0f / 4095.0f))

// Motor Fault Thresholds
#define STALL_CURRENT_A	1.0f
#define STALL_RPM	10.0f
#define MIN_RPM		5.0f

// Weight stability band
#define WEIGHT_STABLE_BAND_G	5.0f

// Roll Centering Tolerances
#define ROLL_CENTER_TOLERANCE_CM	1.0f

// MPU
#define MPU_ADDR	0x68

// Environmental Thresholds
#define MIN_TEMP	20.0f
#define WARN_TEMP	22.0f
#define HIGH_TEMP	35.0f
#define MAX_TEMP	38.0f

#define MIN_HUM		35.0f
#define WARN_HUM	40.0f
#define HIGH_HUM	70.0f
#define MAX_HUM		75.0f

// MPU FFT
#define SAMPLE_RATE	400
#define FFT_SAMPLES	1024
#define EXCESSIVE_RMS_G	2.0f
#define HARMONIC_RATIO	0.15f
#define KNIFE_CAM_DRIVEN	1

// Task Timing
#define SENSOR_READ_PERIOD_MS	10

// SPI PROTOCOL
#define SPI_PACKET_SIZE		256
#define SPI_START_FROM_PI	0xAA
#define SPI_START_FROM_ESP	0xBB

// Commands Pi -> ESP
#define CMD_PING		0x01
#define CMD_READ_SENSORS	0x02
#define CMD_READ_LOG		0x03
#define CMD_CAL_START		0x04
#define CMD_CAL_FACTOR		0x05
#define CMD_TARE		0x06
#define CMD_RESET_BAGS		0x07
#define CMD_SET_THERMO_OFFSET	0x08

// Responses ESP32 -> PI
#define MSG_PONG	0x81
#define MSG_SENSOR_DATA	0x82
#define MSG_LOG		0x83
#define MSG_CAL_RAW_WEIGHT	0x84
#define MSG_ACK		0x85
#define MSG_NACK	0x86
#define MSG_IDLE	0x87

// Logger
#define LOG_QUEUE_DEPTH	24
#define LOG_MAX_MSG_LEN	120
