#pragma once
#include <stdint.h>
#include <HX711.h>
#include "calibration.h"
#include "config.h"

struct LoadCellData {
	float weight_grams;
	bool  loadcell_ok;
	bool  weight_stable;
};

class LoadCellModule {
	public:
		void init(HX711 &scaleInstance, CalibrationModule &calibration);
		void setFactor(float factor);
		void updateAndGetFiltered(LoadCellData &output);
	
	private:
		HX711 *scale = nullptr;
		float loadCellFactor = 0.0f;
		float emaWeight = 0.0f;

		static const uint8_t MEDIAN_WINDOW = 5;
		int32_t rawHistory[MEDIAN_WINDOW] = {0};
		uint8_t historyIdx = 0;
};
