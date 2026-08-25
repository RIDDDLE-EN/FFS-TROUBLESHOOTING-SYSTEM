#include "calibration.h"
#include "logger.h"
#include <HX711.h>
#include <Preferences.h>

static HX711 *scale = nullptr;
static Preferences prefs;

SemaphoreHandle_t mutexCalibration = nullptr;

void CalibrationModule::init(HX711 &scaleInstance, uint8_t dt, uint8_t sck) {
	mutexCalibration	= xSemaphoreCreateMutex();
	scale = &scaleInstance;
	scale->begin(dt, sck);
}

void CalibrationModule::setLoadCellFactor(float factor) {
	if (xSemaphoreTake(mutexCalibration, pdMS_TO_TICKS(100)) != pdTRUE) return;
	if (prefs.begin("cal", false)) {
		prefs.putFloat("lc_factor", factor);
		prefs.end();
	}
	scale->set_scale(factor);
	xSemaphoreGive(mutexCalibration);
	LOG("[CAL] Load cell factor set: %.4f", factor);
}

void CalibrationModule::setThermoOffset(float offset) {
	if (xSemaphoreTake(mutexCalibration, pdMS_TO_TICKS(100)) != pdTRUE) return;
	if (prefs.begin("cal", false)) {
		prefs.putFloat("offset", offset);
		prefs.end();
	}
	xSemaphoreGive(mutexCalibration);
	LOG("[CAL] Thermocouple offset set: %.2f C", offset);
}

float CalibrationModule::getLoadCellFactor() {
	if (xSemaphoreTake(mutexCalibration, pdMS_TO_TICKS(100)) != pdTRUE) return 0.0f;
	float loadcellFactor = 0;
	
	if (prefs.begin("cal", true)) {
		loadcellFactor = prefs.getFloat("lc_factor", 0.0f);
		prefs.end();
	}
	xSemaphoreGive(mutexCalibration);
	return loadcellFactor;
}

float CalibrationModule::getThermoOffset() {
	if (xSemaphoreTake(mutexCalibration, pdMS_TO_TICKS(100)) != pdTRUE) return 0.0f;
	float offset = 0;
	
	if (prefs.begin("cal", true)) {
		offset = prefs.getFloat("offset", 0.0f);
		prefs.end();
	}
	xSemaphoreGive(mutexCalibration);
	return offset;
}

void CalibrationModule::tareLoadCell() {
	scale->tare(20);
	LOG("[LOADCELL] Tared");
}

int32_t CalibrationModule::readRawWeightAverage(uint8_t samples) {
	long sum = 0; int count = 0;
	for (uint8_t i = 0; i < samples; i++) {
		if (scale->is_ready()) {
			sum += scale->read(); count++;
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
	int32_t result = count > 0 ? (int32_t)(sum / count) : 0;
	LOG("[CAL] Raw weight average (%d samples): %ld", 
		count, (long)result);
	return result;
}
