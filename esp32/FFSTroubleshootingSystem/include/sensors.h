#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

typedef uint8_t EnvStatus;
#define ENV_NORMAL	0
#define ENV_LOW_TEMP	1
#define ENV_HIGH_TEMP	2
#define ENV_LOW_HUM	3
#define ENV_HIGH_HUM	4
#define ENV_CRITICAL_TEMP	5
#define ENV_CRITICAL_HUM	6
#define ENV_SENSOR_FAULT	7	

struct RawSensorData {
	uint32_t timestamp_ms;

	// DHT11
	float dht_temp;
	float dht_hum;
	bool  dht_valid;

	// Motors
	uint16_t current1;
	
	uint16_t current2;

	// Load cell
	int32_t hx_raw;

	// Ultrasonics
	float ultra1_cm;
	float ultra2_cm;

	// LDR
	uint16_t ldr_raw;
	bool     ldr_beam_blocked;
	uint32_t ldr_block_duration_us;

	// Thermocouple
	uint16_t tc_adc;
	uint16_t tc_cont;

	// MPU
	float accel_x;
	float accel_y;
	float accel_z;
};

struct VibrationData {
	uint32_t timestamp_ms;
	float    rms_amplitude;
	float    peak_amplitude;
	float    dominant_freq;
	float    freq_magnitude;
};

// Interpreted Sensor Data
struct __attribute__((packed)) InterpretedSensorData {
	uint32_t timestamp_ms;
	
	// Environmental
	float ambient_temp;
	float ambient_hum;
	bool  env_sensor_ok;
	EnvStatus env_status;

	// Motor 1;
	float current1;
	float rpm1;
	bool  motor1_running;

	// Motor 2
	float current2;
	float rpm2;
	bool  motor2_running;

	// Load cell
	float weight_grams;
	bool  weight_stable;
	bool  loadcell_ok;

	// Roll centering
	float roll_center_offset_cm;
	bool  roll_centered;
	bool  ultrasonic_ok;

	// Bag Detection
	uint32_t bags_counted;
	bool     bag_detected;
	float    bag_length_cm;

	// Thermocouple
	float seal_temp;
	bool  tc_connected;
	bool  thermocouple_ok;

	// Vibration
	float vibration_peak_g;
	float vibration_rms_g;
	float vibration_freq_mag;
	float vibration_freq_hz;
	bool  vibration_knife_confirmed;

	// Calibration handshake
	bool    calibration_active;
	int32_t cal_raw_weight;

static_assert(sizeof(InterpretedSensorData) <= 252,
	       	"InterpretedSensorData too large for SPI");

extern QueueHandle_t queueRawToProcessing;
extern QueueHandle_t queueVibrationToProcessing;
extern QueueHandle_t queueProcessedToSPI;

extern SemaphoreHandle_t mutexCalibration;

// Public API
void sensorsInit();

// FreeRTOS tasks
void sensorReadTask(void *parameter);
void vibrationAnalysisTask(void *parameter);
void dataProcessingTask(void *paramter);

// Calibration helpers
void  	setLoadCellFactor(float factor);
float 	getLocadCellFactor();
void  	setThermoOffset(float offset);
float 	getThermoOffset();
void  	tareLoadCell();
int32_t readRawWeightAverage(uint8_t samples);
void    resetBagCounter();
