#include "environment.h"
#include "logger.h"
#include "config.h"
#include "freertos/timers.h"
#include <DHT.h>

static DHT *dhtSensor = nullptr;
static TimerHandle_t dhtTimer = nullptr;
static EnvironmentData cachedEnvData = {0.0f, 0.0f, false};

void dhtTimerCallback(TimerHandle_t xTimer) {
	EnvironmentData freshData;
	freshData.humidity = dhtSensor->readHumidity();
	freshData.temperature = dhtSensor->readTemperature();
	freshData.valid = !(isnan(freshData.humidity) || isnan(freshData.temperature));
	cachedEnvData = freshData;
}

void EnvironmentModule::init() {
	dhtSensor = new DHT(DHT_PIN, DHT11);
	dhtSensor->begin();
	LOG("[ENV] DHT Sensor initialized on pin %d", DHT_PIN);

	dhtTimer = xTimerCreate("DHTTimer", pdMS_TO_TICKS(2000), pdTRUE, nullptr, dhtTimerCallback);
	xTimerStart(dhtTimer,0);
}

EnvironmentData EnvironmentModule::read(EnvironmentData &output) {
	output = cachedEnvData;
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
