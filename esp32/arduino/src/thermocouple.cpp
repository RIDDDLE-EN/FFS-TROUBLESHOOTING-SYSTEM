#include "thermocouple.h"
#include <cmath>

float ThermocoupleModule::read_tc(ThermoData &output, uint8_t tc_pin, CalibrationModule &cal) {
	tc_adc = analogRead(tc_pin);
	tc_cont = analogRead(TC_CONTINUITY);
	output.tc_connected = (tc_cont > 2000);

	filteredTcAdc = (0.1f * tc_adc) + (0.9f * filteredTcAdc);
	float tc_mv = ADC_TO_MV(filteredTcAdc);
	if (output.tc_connected) {
		output.seal_temp = (tc_mv / MV_PER_DEGC) + cal.getThermoOffset();
		output.thermocouple_ok = true;
	} else {
		output.seal_temp = 0.0f;
		output.thermocouple_ok = false;
	}
	return output.seal_temp;
}
