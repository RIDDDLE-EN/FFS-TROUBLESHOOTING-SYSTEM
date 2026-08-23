#pragma once
#include <stdint.h>
#include "config.h"

class LoadCellModule {
	public:
		void init();
		void setFactor(float factor);
		float getFactor() const;
		void tare(uint8_t times=20);
		int32_t readRawAverage(uint8_t samples);
		float updateAndGetFiltered();
	
	private:
		uint8_t dtPin = HX_DT, sckPin = HX_SCK;
		float loadCellFactor = 0.0f;
		float emaWeight = 0.0f;
		static const uint8_t MEDIAN_WINDOW = 5;
		int32_t rawHistory[MEDIAN_WINDOW] = {0};
		uint8_t historyIdx = 0;
};
