#include "environment.h"
#include "logger.h"
#include "config.h"
#include <DHT.h>

static DHT *dhtSensor = nullptr;


void EnvironmentModule::init() {
	dhtSensor = new DHT(DHT_PIN, DHT11);
	dhtSensor->begin();
	LOG("[ENV] DHT Sensor initialized on pin %d", DHT_PIN);
}

EnvironmentData EnvironmentModule::read(EnvironmentData &output) {
	output.humidity = dhtSensor->readHumidity();
	output.temperature = dhtSensor->readTemperature();

	if (isnan(output.humidity) || isnan(output.temperature)) {
		output.valid = false;
		LOG("[ENV] ERROR: Failed to read from DHT sensor!");
	} else {
		output.valid = true;
	}
	return output;
}

EnvStatus EnvironmentModule::evaluateEnvironment(EnvironmentData &output) {
	float temp = output.temperature;
	float hum  = output.humidity;
	if (!output.valid)		return ENV_SENSOR_FAULT;
	if (temp < MIN_TEMP ||
		temp > MAX_TEMP)	return ENV_CRITICAL_TEMP;
	if (hum < MIN_HUM ||
		hum > MAX_HUM)		return ENV_CRITICAL_HUM;
	if (temp < WARN_TEMP)		return ENV_LOW_TEMP;
	if (temp > HIGH_TEMP)		return ENV_HIGH_TEMP;
	if (hum < WARN_HUM)		return ENV_LOW_HUM;
	if (hum > HIGH_HUM)		return ENV_HIGH_HUM;
	return ENV_NORMAL;
}
