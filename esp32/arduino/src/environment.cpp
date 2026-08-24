#include "environment.h"
#include "logger.h"
#include "config.h"
#include <DHT.h>

static DHT *dhtSensor = nullptr;


void EnvrionmentModule::init() {
	dhtSensor = new DHT(DHT_PIN, DHT11);
	dhtSensor->begin();
	LOG("[ENV] DHT Sensor initialized on pin %d", DHT_PIN);
}

EnvironmentData EnvironmentModule::read() {
	EnvironmentData data;
	data.humidity = dhtSensor->readHumidity();
	data.temperature = dhtSensor->readTemperature();

	if (isnan(data.humidity) || isnan(data.temperature)) {
		data.valid = false;
		LOG("[ENV] ERROR: Failed to read from DHT sensor!");
	} else {
		data.valid = true;
	}
	return data;
}
