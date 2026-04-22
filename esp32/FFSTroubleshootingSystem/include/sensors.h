#pragma once
#include <Arduino.h>

// 
// Raw Sensor Data - ESP32 sends numbers only, Pi interprets
//

struct RawSensorData {
	uint32_t timestamp;

	// DHT11
	float dht_temp;
	float dht_hum;
	bool  dht_valid;

	// Motor 1
	uint16_t adc_current1;
	int32_t  encoder1_pulses;
	uint32_t encoder1_last_us;

	// Motor 2
	uint16_t adc_current2;
	int32_t  encoder2_pulses;
	uint32_t encoder2_last_us;

	// Load Cell
	int32_t hx711_raw;
	float   hx711_cal_factor;

	// Roll Centering 
	float ultra1_cm;
	float ultra2_cm;

	// Bag Length 
	uint16_t ldr_raw;
	uint32_t ldr_block_start_us;
	uint32_t ldr_last_block_us;

	// Thermocouple
	uint16_t tc_adc;
	uint16_t tc_cont_adc;
	float    tc_offset;

	// Vibration
	float accel_x;
	float accel_y;
	float accel_z;
};

// 
// Sensor API
//

void sesnorsInit();
RawSensorData sensorsRead();

// Calibration Factor Storage (called by pi via SPI command)
void  setLoadCellFactor(float factor);
float getLoadCellFactor();

void  setThermoOffset(float offset);
float getThermoOffset();
