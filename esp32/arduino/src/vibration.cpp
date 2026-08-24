#include "vibration.h"
#include "logger.h"
#include <cmath>

void VibrationModule::init(MPU6050 &mpuInstance) {
	mpu = &mpuInstance;
	LOG("[VIBRAION] MPU6050 Initialized");
}

bool VibrationModule::processImpact(int16_t ax, int16_t ay, int16_t az, jawDiagnostics &outResult) {
	float rawMagnitude = sqrtf(((float)ax*ax + (float)ay*ay + (float)az*az));
	float gForce = rawMagnitude / ACCEL_SCALE_FACTOR;

	vReal[sampleIdx] = gForce;
	vImag[sampleIdx] = 0.0f;
	sampleIdx++;

	if (sampleIdx >= FFT_SAMPLES) {
		float peakImpact = 0.0f;
		float dcOffset = 0.0f;
		
		for (uint16_t  i = 0; i < FFT_SAMPLES; i++) {
			dcOffset += vReal[i];
		}
		dcOffset /= (float)FFT_SAMPLES;

		for (uint16_t i = 0; i < FFT_SAMPLES; i++) {
			vReal[i] -= dcOffset;
			if (fabsf(vReal[i]) > peakImpact) {
				peakImpact = fabsf(vReal[i]);
			}
		}

		ArduinoFFT<float> FFT(vReal, vImag, FFT_SAMPLES, SAMPLE_RATE);
		FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
		FFT.compute(FFT_FORWARD);
		FFT.complexToMagnitude();

		float peakFreq = 0.0f;
		float maxMag = 0.0f;

		uint16_t nyquistLimit = FFT_SAMPLES / 2;

		for (uint16_t i = MIN_FREQ_BIN; i < nyquistLimit; i++) {
			if (vReal[i] > maxMag) {
				maxMag = vReal[i];
				peakFreq = (float)i * SAMPLE_RATE / (float)FFT_SAMPLES;
			}
		}

		outResult.impact_amplitude = peakImpact;
		outResult.knife_frequency = peakFreq;
		outResult.is_updated = true;

		sampleIdx = 0;
		return true;
	}

	outResult.is_updated = false;
	return false;
}
