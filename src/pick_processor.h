#pragma once

#include <cmath>
#include <cstdint>

class PickProcessor {
public:
	void setSampleRate(double rate) {
		sampleRate = rate;
		recalcCoefficients();
	}

	void process(float* buffer, uint32_t n_samples, float pickAmount) {
		if (pickAmount == 0.0f)
			return;

		for (uint32_t i = 0; i < n_samples; i++) {
			float x = buffer[i];

			// 1. Detection high-pass filter (1-pole HPF on detection path only)
			float detect = x - hpfState;
			hpfState += hpfCoeff * detect;
			detect = x - hpfState;

			// 2. Envelope follower
			float absDetect = fabsf(detect);
			if (absDetect > envelope)
				envelope = absDetect;
			else
				envelope *= releaseCoeff;

			// 3. Attack detection (positive slope of envelope)
			float attack = envelope - prevEnvelope;
			if (attack < 0.0f)
				attack = 0.0f;
			prevEnvelope = envelope;

			// 4. Attack strength
			float attackStrength = attack * sensitivity;

			// 5. Transient envelope generation
			transientState += attackStrength * pickAmount;
			transientState *= decayCoeff;

			// 6. Gain calculation (soft limiter)
			float gain = 1.0f + tanhf(transientState);

			// 7. Output
			buffer[i] = x * gain;
		}
	}

private:
	void recalcCoefficients() {
		releaseCoeff = expf(-1.0f / (releaseTime * (float)sampleRate));
		decayCoeff = expf(-1.0f / (decayTime * (float)sampleRate));
		hpfCoeff = 1.0f - expf(-2.0f * (float)M_PI * detectHpfFreq / (float)sampleRate);
	}

	double sampleRate = 48000.0;

	// Internal parameters (defaults from spec)
	float sensitivity = 6.0f;
	float decayTime = 0.015f;      // 15 ms
	float releaseTime = 0.010f;    // 10 ms
	float detectHpfFreq = 1200.0f; // Hz

	// Coefficients
	float releaseCoeff = 0.0f;
	float decayCoeff = 0.0f;
	float hpfCoeff = 0.0f;

	// State
	float hpfState = 0.0f;
	float envelope = 0.0f;
	float prevEnvelope = 0.0f;
	float transientState = 0.0f;
};
