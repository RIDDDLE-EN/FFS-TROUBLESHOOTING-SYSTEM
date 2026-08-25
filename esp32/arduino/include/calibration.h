#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <HX711.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t mutexCalibration;

class CalibrationModule {
	public:
		void init(HX711 &scaleInstance, uint8_t dt, uint8_t sck);
		void setLoadCellFactor(float factor);
		void setThermoOffset(float offset);
		float getThermoOffset();
		float getLoadCellFactor();
		void tareLoadCell();
		int32_t readRawWeightAverage(uint8_t samples);
};
