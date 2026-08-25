#pragma once
#include <Arduino.h>

struct EnvironmentData {
	float temperature;
	float humidity;
	bool valid;
};

typedef uint8_t EnvStatus;
#define ENV_NORMAL		0
#define ENV_LOW_TEMP		1
#define ENV_HIGH_TEMP		2
#define ENV_LOW_HUM		3
#define ENV_HIGH_HUM		4
#define ENV_CRITICAL_TEMP	5
#define ENV_CRITICAL_HUM	6
#define ENV_SENSOR_FAULT	7


class EnvironmentModule {
	public:
		void init();
		EnvironmentData read(EnvironmentData &output);
		static EnvStatus evaluateEnvironment(EnvironmentData &output);
};
