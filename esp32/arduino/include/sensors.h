#pragma once
#include <Arduino.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "motors.h"
#include "vibration.h"
#include "calibration.h"
#include "loadcell.h"
#include "bag.h"
#include "roll.h"
#include "config.h"
#include "spi_comms.h"
#include "logger.h"
#include "environment.h"

typedef uint8_t EnvStatus;
#define ENV_NORMAL		0
#define ENV_LOW_TEMP		1
#define ENV_HIGH_TEMP		2
#define ENV_LOW_HUM		3
#define ENV_HIGH_HUM		4
#define ENV_CRITICAL_TEMP	5
#define ENV_CRITICAL_HUM	6
#define ENV_SENSOR_FAULT	7

struct __attribute__((packed)) InterpretedSensorData {
	uint32_t timestamp_ms;

	// Environmental
	float ambient_temp;
	float ambient_hum;
	bool  env_sensor_ok;
	EnvStatus env_status;

	// Motor 1
	float current1;
	float rpm1;
	bool  motor1_running;

	// Motor 2
	float current2;
	float rpm2;
	bool  motor2_running;

	// Load cell
	float weight_grams;
	bool  loadcell_ok;

	// Roll centering
	float roll_center_offset_cm;
	bool  roll_centered;
	bool  ultrasonic_ok;

	// Bag Detection
	uint32_t bags_counted;
	float    bag_length_cm;

	// Thermocouple
	float seal_temp;
	bool  tc_connected;
	bool  thermocouple_ok;

	// Vibration
	float impact_amplitude;
	float knife_frequency;
	bool  is_updated;

	// Calibration handshake
	bool    calibration_active;
	int32_t cal_raw_weight;
};

extern QueueHandle_t queueProcessedToSPI;

void sensorsInit();
void dataProcessingTask(void *pvParameters);
