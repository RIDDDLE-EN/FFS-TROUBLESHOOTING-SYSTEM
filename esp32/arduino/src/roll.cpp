#include "config.h"
#include "roll.h"
#include "logger.h"
#include <cmath>
#include <Ultrasonic.h>

static Ultrasonic *ultra1 = nullptr;
static Ultrasonic *ultra2 = nullptr;

void RollModule::init(uint8_t trig1, uint8_t echo1, uint8_t trig2, uint8_t echo2) {
	ultra1 = new Ultrasonic(trig1, echo1);
	ultra2 = new Ultrasonic(trig2, echo2);
	LOG("[ROLL] Roll module initialized");
}

void RollModule::calculateOffset() {
	RollData output;
	float ultra1_cm = ultra1->read();
	float ultra2_cm = ultra2->read();

	output.ultrasonic_ok = (ultra1_cm > 0.0f && ultra2_cm > 0.0f);
	if (output.ultrasonic_ok) {
		output.roll_center_offset_cm = ultra1_cm - ultra2_cm;
		output.roll_centered = fabsf(ouput.roll_center_offset_cm) < ROLL_CENTER_TOLERANCE_CM;
	}
	
	String str_ok = output.ultrasonic_ok ? "yes" : "no";
	String str_centered = output.roll_centered ? "yes" : "no";

	LOG("[ROLL] Sensor ok? %s | Centered? %s | Offset = %.2f cm", 
		str_ok.c_str(), str_centered.c_str(), output.roll_center_offset_cm);
}
