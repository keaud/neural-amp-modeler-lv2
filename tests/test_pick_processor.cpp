#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/pick_processor.h"

static constexpr float EPSILON = 1e-6f;
static constexpr double SAMPLE_RATE = 48000.0;

// Helper: generate a buffer of silence
static std::vector<float> silence(uint32_t n) {
	return std::vector<float>(n, 0.0f);
}

// Helper: generate a constant-amplitude signal
static std::vector<float> dc_signal(uint32_t n, float amplitude) {
	return std::vector<float>(n, amplitude);
}

// Helper: generate a single sharp transient (impulse at sample 0, then silence)
static std::vector<float> impulse(uint32_t n, float amplitude) {
	std::vector<float> buf(n, 0.0f);
	buf[0] = amplitude;
	return buf;
}

// Helper: generate a transient followed by sustain
// Simulates a guitar pick: sharp rise then sustained amplitude
static std::vector<float> pick_and_sustain(uint32_t attack_samples, uint32_t sustain_samples,
                                            float peak, float sustain_level) {
	std::vector<float> buf(attack_samples + sustain_samples, 0.0f);
	// Linear ramp up
	for (uint32_t i = 0; i < attack_samples; i++) {
		buf[i] = peak * (float)(i + 1) / (float)attack_samples;
	}
	// Sustain
	for (uint32_t i = 0; i < sustain_samples; i++) {
		buf[attack_samples + i] = sustain_level;
	}
	return buf;
}

// Helper: compute RMS of a buffer region
static float rms(const float* buf, uint32_t start, uint32_t count) {
	float sum = 0.0f;
	for (uint32_t i = 0; i < count; i++) {
		float s = buf[start + i];
		sum += s * s;
	}
	return sqrtf(sum / count);
}

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
	do { \
		tests_run++; \
		printf("  TEST: %s ... ", #name); \
	} while(0)

#define PASS() \
	do { \
		tests_passed++; \
		printf("PASS\n"); \
	} while(0)

#define FAIL(msg) \
	do { \
		printf("FAIL: %s\n", msg); \
	} while(0)

// === Tests ===

// pick=0 should produce no change (passthrough)
void test_zero_pick_is_passthrough() {
	TEST(zero_pick_is_passthrough);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	auto input = pick_and_sustain(10, 200, 0.8f, 0.5f);
	auto expected = input; // copy before processing

	proc.process(input.data(), (uint32_t)input.size(), 0.0f);

	bool match = true;
	for (size_t i = 0; i < input.size(); i++) {
		if (fabsf(input[i] - expected[i]) > EPSILON) {
			match = false;
			break;
		}
	}

	if (match) PASS();
	else FAIL("Output differs from input when pick=0");
}

// Positive pick should boost the attack region
void test_positive_pick_boosts_attack() {
	TEST(positive_pick_boosts_attack);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	auto input = pick_and_sustain(10, 2000, 0.8f, 0.3f);
	auto original = input;

	proc.process(input.data(), (uint32_t)input.size(), 1.0f);

	// The attack region (first ~50 samples after transient) should have higher energy
	float attack_rms_original = rms(original.data(), 0, 50);
	float attack_rms_processed = rms(input.data(), 0, 50);

	if (attack_rms_processed > attack_rms_original) PASS();
	else FAIL("Positive pick did not boost attack energy");
}

// Negative pick should reduce the attack region
void test_negative_pick_softens_attack() {
	TEST(negative_pick_softens_attack);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	auto input = pick_and_sustain(10, 2000, 0.8f, 0.3f);
	auto original = input;

	proc.process(input.data(), (uint32_t)input.size(), -1.0f);

	// The attack region should have lower energy
	float attack_rms_original = rms(original.data(), 0, 50);
	float attack_rms_processed = rms(input.data(), 0, 50);

	if (attack_rms_processed < attack_rms_original) PASS();
	else FAIL("Negative pick did not soften attack energy");
}

// Sustain region should remain mostly unchanged with positive pick
void test_sustain_unchanged_positive_pick() {
	TEST(sustain_unchanged_positive_pick);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	// Use a longer signal so transient envelope fully decays
	auto input = pick_and_sustain(10, 4000, 0.8f, 0.3f);
	auto original = input;

	proc.process(input.data(), (uint32_t)input.size(), 1.0f);

	// Check sustain region well after the attack (last 2000 samples)
	uint32_t sustain_start = (uint32_t)input.size() - 2000;
	float sustain_rms_original = rms(original.data(), sustain_start, 2000);
	float sustain_rms_processed = rms(input.data(), sustain_start, 2000);

	// Should be within 5% of original
	float ratio = sustain_rms_processed / sustain_rms_original;
	if (ratio > 0.95f && ratio < 1.05f) PASS();
	else {
		char msg[128];
		snprintf(msg, sizeof(msg), "Sustain RMS ratio %.4f outside [0.95, 1.05]", ratio);
		FAIL(msg);
	}
}

// Sustain region should remain mostly unchanged with negative pick
void test_sustain_unchanged_negative_pick() {
	TEST(sustain_unchanged_negative_pick);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	auto input = pick_and_sustain(10, 4000, 0.8f, 0.3f);
	auto original = input;

	proc.process(input.data(), (uint32_t)input.size(), -1.0f);

	uint32_t sustain_start = (uint32_t)input.size() - 2000;
	float sustain_rms_original = rms(original.data(), sustain_start, 2000);
	float sustain_rms_processed = rms(input.data(), sustain_start, 2000);

	float ratio = sustain_rms_processed / sustain_rms_original;
	if (ratio > 0.95f && ratio < 1.05f) PASS();
	else {
		char msg[128];
		snprintf(msg, sizeof(msg), "Sustain RMS ratio %.4f outside [0.95, 1.05]", ratio);
		FAIL(msg);
	}
}

// Silence should pass through unchanged regardless of pick value
void test_silence_unchanged() {
	TEST(silence_unchanged);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	auto input = silence(1024);
	proc.process(input.data(), (uint32_t)input.size(), 1.0f);

	bool all_zero = true;
	for (size_t i = 0; i < input.size(); i++) {
		if (fabsf(input[i]) > EPSILON) {
			all_zero = false;
			break;
		}
	}

	if (all_zero) PASS();
	else FAIL("Silence was modified");
}

// Impulse with positive pick should produce gain > 1 near the impulse
void test_impulse_positive_pick_gain() {
	TEST(impulse_positive_pick_gain);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	// Use a high-frequency impulse that passes the detection HPF
	uint32_t n = 2048;
	std::vector<float> input(n, 0.0f);
	// Create a brief burst of high-frequency content (alternating samples)
	for (uint32_t i = 0; i < 8; i++) {
		input[i] = (i % 2 == 0) ? 0.5f : -0.5f;
	}
	auto original = input;

	proc.process(input.data(), n, 1.0f);

	// Check that at least some samples in the transient region are boosted
	bool any_boosted = false;
	for (uint32_t i = 0; i < 100; i++) {
		if (fabsf(input[i]) > fabsf(original[i]) + EPSILON) {
			any_boosted = true;
			break;
		}
	}

	if (any_boosted) PASS();
	else FAIL("No samples were boosted by positive pick on impulse");
}

// gain = 1 + tanh(transient_state): verify gain is always positive (no signal inversion)
void test_no_signal_inversion() {
	TEST(no_signal_inversion);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	auto input = pick_and_sustain(5, 500, 1.0f, 0.5f);
	auto original = input;

	proc.process(input.data(), (uint32_t)input.size(), -1.0f);

	// With negative pick, gain = 1 + tanh(negative) which is in (0, 1)
	// So sign of each sample should match original
	bool sign_ok = true;
	for (size_t i = 0; i < input.size(); i++) {
		if (original[i] != 0.0f) {
			// Same sign check
			if ((input[i] > 0) != (original[i] > 0)) {
				sign_ok = false;
				break;
			}
		}
	}

	if (sign_ok) PASS();
	else FAIL("Signal polarity was inverted");
}

// Verify gain is bounded: gain = 1 + tanh(x) is in (0, 2)
void test_gain_bounded() {
	TEST(gain_bounded);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	// Extreme transient
	auto input = pick_and_sustain(2, 500, 1.0f, 0.5f);
	auto original = input;

	proc.process(input.data(), (uint32_t)input.size(), 1.0f);

	bool bounded = true;
	for (size_t i = 0; i < input.size(); i++) {
		if (original[i] != 0.0f) {
			float gain = input[i] / original[i];
			// gain should be in (0, 2) since gain = 1 + tanh(x)
			if (gain < -EPSILON || gain > 2.0f + EPSILON) {
				char msg[128];
				snprintf(msg, sizeof(msg), "Gain %.4f at sample %zu out of bounds", gain, i);
				bounded = false;
				break;
			}
		}
	}

	if (bounded) PASS();
	else FAIL("Gain exceeded theoretical bounds");
}

// Multiple calls should not accumulate state in a way that causes instability
void test_no_state_blowup() {
	TEST(no_state_blowup);

	PickProcessor proc;
	proc.setSampleRate(SAMPLE_RATE);

	bool stable = true;
	for (int round = 0; round < 100; round++) {
		auto input = pick_and_sustain(5, 200, 0.8f, 0.4f);
		proc.process(input.data(), (uint32_t)input.size(), 1.0f);

		for (size_t i = 0; i < input.size(); i++) {
			if (!std::isfinite(input[i])) {
				stable = false;
				break;
			}
		}
		if (!stable) break;
	}

	if (stable) PASS();
	else FAIL("Output became non-finite after repeated processing");
}

// Different sample rates should produce valid output
void test_different_sample_rates() {
	TEST(different_sample_rates);

	double rates[] = {22050.0, 44100.0, 48000.0, 96000.0, 192000.0};
	bool ok = true;

	for (double rate : rates) {
		PickProcessor proc;
		proc.setSampleRate(rate);

		auto input = pick_and_sustain(10, 500, 0.8f, 0.3f);
		proc.process(input.data(), (uint32_t)input.size(), 0.5f);

		for (size_t i = 0; i < input.size(); i++) {
			if (!std::isfinite(input[i])) {
				ok = false;
				break;
			}
		}
		if (!ok) break;
	}

	if (ok) PASS();
	else FAIL("Non-finite output at some sample rate");
}

// Verify pick=-1 and pick=+1 produce opposite effects on attack energy
void test_symmetry() {
	TEST(symmetry);

	auto base_signal = pick_and_sustain(10, 2000, 0.8f, 0.3f);

	PickProcessor proc_pos;
	proc_pos.setSampleRate(SAMPLE_RATE);
	auto pos_input = base_signal;
	proc_pos.process(pos_input.data(), (uint32_t)pos_input.size(), 1.0f);

	PickProcessor proc_neg;
	proc_neg.setSampleRate(SAMPLE_RATE);
	auto neg_input = base_signal;
	proc_neg.process(neg_input.data(), (uint32_t)neg_input.size(), -1.0f);

	float attack_rms_original = rms(base_signal.data(), 0, 50);
	float attack_rms_pos = rms(pos_input.data(), 0, 50);
	float attack_rms_neg = rms(neg_input.data(), 0, 50);

	// Positive should boost above original, negative should reduce below original
	if (attack_rms_pos > attack_rms_original && attack_rms_neg < attack_rms_original) PASS();
	else FAIL("Positive and negative pick don't have opposite effects");
}

int main() {
	printf("Running PickProcessor tests...\n\n");

	test_zero_pick_is_passthrough();
	test_positive_pick_boosts_attack();
	test_negative_pick_softens_attack();
	test_sustain_unchanged_positive_pick();
	test_sustain_unchanged_negative_pick();
	test_silence_unchanged();
	test_impulse_positive_pick_gain();
	test_no_signal_inversion();
	test_gain_bounded();
	test_no_state_blowup();
	test_different_sample_rates();
	test_symmetry();

	printf("\n%d/%d tests passed.\n", tests_passed, tests_run);

	return (tests_passed == tests_run) ? 0 : 1;
}
