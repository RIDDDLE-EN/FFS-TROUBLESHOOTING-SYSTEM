#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "config.h"
#include "calibration.h"

struct ThermoData {
	float seal_temp;
	bool  tc_connected;
	bool  tc_ok;
};

class ThermocoupleModule {
	public:
		float read_tc(ThermoData &output, uint8_t tc_pin, CalibrationMdoule &cal);

	private:
		float filteredTcAdc = 0.0f;
		int32_t tc_adc, tc_cont;
};
