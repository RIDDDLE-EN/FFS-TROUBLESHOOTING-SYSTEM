#pragma once
#include <stdint.h>
#include "config.h"
#include <Arduino.h>

struct MotorData {
	float current1;
	float current2;
	float rpm1;
	float rpm2;
	bool motor1_running;
	bool motor2_running;
};

class MotorModule {
	public:
		void init();
		void calibrateCurrentOffsets();
		MotorData update();

		static void IRAM_ATTR encoder1_ISR();
		static void IRAM_ATTR encoder2_ISR();
	
	private:
		uint8_t d1 = ENCODER1_DT, d2 = ENCODER2_DT;
		uint8_t c1 = ENCODER1_CLK, c2 = ENCODER2_CLK;
		uint8_t curr1 = ACS712_1, curr2 = ACS712_2;

		float zeroAdc1 = 2047.0f;
		float zeroAdc2 = 2047.0f;
		float filteredCurr1 = 0.0f;
		float filteredCurr2 = 0.0f;

		float calculateRPM(struct EncoderState &enc);
};
