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

			// 1. Detection high-pass filter (~1.2kHz, detection path only)
			float detect = x - detectHpfState;
			detectHpfState += detectHpfCoeff * detect;
			detect = x - detectHpfState;

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

			// 4. Transient envelope (always positive)
			transientEnv += attack * sensitivity;
			transientEnv *= decayCoeff;

			// 5. Audio-path HPF at ~2kHz to isolate bright/pick content
			float lpf = audioLpfState + audioHpfCoeff * (x - audioLpfState);
			audioLpfState = lpf;
			float bright = x - lpf;

			// 6. Modulate bright content during transients
			//    tanhf(transientEnv) is 0..1, pickAmount is -1..1
			//    positive pickAmount: boost bright during transients
			//    negative pickAmount: cut bright during transients (max 50%)
			float mod = tanhf(transientEnv) * pickAmount;

			float brightGain;
			if (mod >= 0.0f)
				brightGain = mod;            // boost: 0 to ~1.0 (doubles bright)
			else
				brightGain = mod * 0.5f;     // cut: 0 to -0.5 (halves bright)

			buffer[i] = x + brightGain * bright;
		}
	}

private:
	void recalcCoefficients() {
		releaseCoeff = expf(-1.0f / (releaseTime * (float)sampleRate));
		decayCoeff = expf(-1.0f / (decayTime * (float)sampleRate));
		detectHpfCoeff = 1.0f - expf(-2.0f * (float)M_PI * detectHpfFreq / (float)sampleRate);
		audioHpfCoeff = 1.0f - expf(-2.0f * (float)M_PI * audioHpfFreq / (float)sampleRate);
	}

	double sampleRate = 48000.0;

	// Internal parameters
	float sensitivity = 6.0f;
	float decayTime = 0.015f;      // 15 ms
	float releaseTime = 0.010f;    // 10 ms
	float detectHpfFreq = 1200.0f; // Hz - detection path
	float audioHpfFreq = 2000.0f;  // Hz - audio path crossover

	// Coefficients
	float releaseCoeff = 0.0f;
	float decayCoeff = 0.0f;
	float detectHpfCoeff = 0.0f;
	float audioHpfCoeff = 0.0f;

	// Detection state
	float detectHpfState = 0.0f;
	float envelope = 0.0f;
	float prevEnvelope = 0.0f;
	float transientEnv = 0.0f;

	// Audio path state
	float audioLpfState = 0.0f;
};
