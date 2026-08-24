#pragma once
#include "stdint.h"

struct RollData {
	float roll_center_offset_cm;
	bool  roll_centered;
	bool  ultrasonic_ok;
};

class RollModule {
	public:
		void init(uint8_t trig1, uint8_t echo1, uint8_t trig2, uint8_t echo2);
		void calculateOffset();
}
