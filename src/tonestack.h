#pragma once

#include <cmath>
#include <cstdint>

class ToneStack {
public:
	void setSampleRate(double rate) {
		sampleRate = rate;
		// Force coefficient recalculation
		prevBassDb = -999.0f;
	}

	void update(float bassDb, float bassFreq, float midDb, float midFreq, float trebleDb, float trebleFreq,
	            float hpfFreqHz = 20.0f, float lpfFreqHz = 20000.0f) {
		if (bassDb == prevBassDb && bassFreq == prevBassFreq &&
		    midDb == prevMidDb && midFreq == prevMidFreq &&
		    trebleDb == prevTrebleDb && trebleFreq == prevTrebleFreq &&
		    hpfFreqHz == prevHpfFreq && lpfFreqHz == prevLpfFreq)
			return;

		prevBassDb = bassDb;
		prevBassFreq = bassFreq;
		prevMidDb = midDb;
		prevMidFreq = midFreq;
		prevTrebleDb = trebleDb;
		prevTrebleFreq = trebleFreq;

		calcLowShelf(lowShelf, bassFreq, bassDb);
		calcPeaking(mid, midFreq, midDb, 0.7);
		calcHighShelf(highShelf, trebleFreq, trebleDb);

		if (hpfFreqHz != prevHpfFreq) {
			prevHpfFreq = hpfFreqHz;
			calcHighpass(hpf, hpfFreqHz);
		}
		if (lpfFreqHz != prevLpfFreq) {
			prevLpfFreq = lpfFreqHz;
			calcLowpass(lpf, lpfFreqHz);
		}
	}

	void process(float* buffer, uint32_t n_samples) {
		for (uint32_t i = 0; i < n_samples; i++) {
			float x = buffer[i];
			x = processBiquad(hpf, x);
			x = processBiquad(lowShelf, x);
			x = processBiquad(mid, x);
			x = processBiquad(highShelf, x);
			x = processBiquad(lpf, x);
			buffer[i] = x;
		}
	}

private:
	struct Biquad {
		// Coefficients (normalized: a0 = 1)
		double b0 = 1.0, b1 = 0.0, b2 = 0.0;
		double a1 = 0.0, a2 = 0.0;
		// State (transposed direct form II)
		double z1 = 0.0, z2 = 0.0;
	};

	static float processBiquad(Biquad& bq, float in) {
		double x = static_cast<double>(in);
		double y = bq.b0 * x + bq.z1;
		bq.z1 = bq.b1 * x - bq.a1 * y + bq.z2;
		bq.z2 = bq.b2 * x - bq.a2 * y;
		return static_cast<float>(y);
	}

	void calcLowShelf(Biquad& bq, double freq, double gainDb) {
		double A = pow(10.0, gainDb / 40.0);
		double w0 = 2.0 * M_PI * freq / sampleRate;
		double cosw0 = cos(w0);
		double sinw0 = sin(w0);
		double alpha = sinw0 / (2.0 * 0.707);
		double twoSqrtAalpha = 2.0 * sqrt(A) * alpha;

		double a0 = (A + 1.0) + (A - 1.0) * cosw0 + twoSqrtAalpha;
		bq.b0 = (A * ((A + 1.0) - (A - 1.0) * cosw0 + twoSqrtAalpha)) / a0;
		bq.b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0)) / a0;
		bq.b2 = (A * ((A + 1.0) - (A - 1.0) * cosw0 - twoSqrtAalpha)) / a0;
		bq.a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cosw0)) / a0;
		bq.a2 = ((A + 1.0) + (A - 1.0) * cosw0 - twoSqrtAalpha) / a0;
	}

	void calcPeaking(Biquad& bq, double freq, double gainDb, double Q) {
		double A = pow(10.0, gainDb / 40.0);
		double w0 = 2.0 * M_PI * freq / sampleRate;
		double cosw0 = cos(w0);
		double sinw0 = sin(w0);
		double alpha = sinw0 / (2.0 * Q);

		double a0 = 1.0 + alpha / A;
		bq.b0 = (1.0 + alpha * A) / a0;
		bq.b1 = (-2.0 * cosw0) / a0;
		bq.b2 = (1.0 - alpha * A) / a0;
		bq.a1 = (-2.0 * cosw0) / a0;
		bq.a2 = (1.0 - alpha / A) / a0;
	}

	void calcHighpass(Biquad& bq, double freq) {
		double w0 = 2.0 * M_PI * freq / sampleRate;
		double cosw0 = cos(w0);
		double sinw0 = sin(w0);
		double alpha = sinw0 / (2.0 * 0.7071); // Butterworth Q

		double a0 = 1.0 + alpha;
		bq.b0 = ((1.0 + cosw0) / 2.0) / a0;
		bq.b1 = (-(1.0 + cosw0)) / a0;
		bq.b2 = ((1.0 + cosw0) / 2.0) / a0;
		bq.a1 = (-2.0 * cosw0) / a0;
		bq.a2 = (1.0 - alpha) / a0;
	}

	void calcLowpass(Biquad& bq, double freq) {
		double w0 = 2.0 * M_PI * freq / sampleRate;
		double cosw0 = cos(w0);
		double sinw0 = sin(w0);
		double alpha = sinw0 / (2.0 * 0.7071); // Butterworth Q

		double a0 = 1.0 + alpha;
		bq.b0 = ((1.0 - cosw0) / 2.0) / a0;
		bq.b1 = (1.0 - cosw0) / a0;
		bq.b2 = ((1.0 - cosw0) / 2.0) / a0;
		bq.a1 = (-2.0 * cosw0) / a0;
		bq.a2 = (1.0 - alpha) / a0;
	}

	void calcHighShelf(Biquad& bq, double freq, double gainDb) {
		double A = pow(10.0, gainDb / 40.0);
		double w0 = 2.0 * M_PI * freq / sampleRate;
		double cosw0 = cos(w0);
		double sinw0 = sin(w0);
		double alpha = sinw0 / (2.0 * 0.707);
		double twoSqrtAalpha = 2.0 * sqrt(A) * alpha;

		double a0 = (A + 1.0) - (A - 1.0) * cosw0 + twoSqrtAalpha;
		bq.b0 = (A * ((A + 1.0) + (A - 1.0) * cosw0 + twoSqrtAalpha)) / a0;
		bq.b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0)) / a0;
		bq.b2 = (A * ((A + 1.0) + (A - 1.0) * cosw0 - twoSqrtAalpha)) / a0;
		bq.a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosw0)) / a0;
		bq.a2 = ((A + 1.0) - (A - 1.0) * cosw0 - twoSqrtAalpha) / a0;
	}

	double sampleRate = 48000.0;

	Biquad hpf;
	Biquad lowShelf;
	Biquad mid;
	Biquad highShelf;
	Biquad lpf;

	// Previous parameter values for change detection
	float prevBassDb = -999.0f, prevBassFreq = -1.0f;
	float prevMidDb = -999.0f, prevMidFreq = -1.0f;
	float prevTrebleDb = -999.0f, prevTrebleFreq = -1.0f;
	float prevHpfFreq = -1.0f;
	float prevLpfFreq = -1.0f;
};
