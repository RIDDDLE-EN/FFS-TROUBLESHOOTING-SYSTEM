#pragma once
#include <stdint.h>
#include <MPU6050.h>
#include "config.h"

struct JawDiagnostics {
	float impact_amplitude;
	float knife_frequency;
	bool is_updated;
};

class VibrationModule {
	public:
		void init(MPU6050 &mpuInstance);
		vool processImpact(int16_t ax, int16_t ay, int16_t az, JawDiagnostics &outResult);

	private:
		MPU6050 *mpu = nullptr;
		float vReal[FFT_SAMPLES];
		float vImag[FFT_SAMPLES];
		uint16_t sampleIdx = 0;
};

