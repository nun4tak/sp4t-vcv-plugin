#include "Decorrelator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "saf.h"


namespace sp4t {


namespace {

/** Lattice all-pass filter orders and the bands they apply to, from SAF's
 * `decorrelator` example: 20th order up to 600 Hz, 15th to 2.4 kHz, 6th to
 * 4 kHz, 3rd to 12 kHz and delays only above. */
const int APF_ORDERS[4] = {20, 15, 6, 3};
const float APF_CUTOFFS[4] = {600.f, 2.4e3f, 4.0e3f, 12e3f};

/** Longest static delay the bands may use, in hops. */
const int MAX_TF_DELAY = 8;

/** Energy compensation of the all-pass filters, as in the example. */
const float ENERGY_COMP_COEFF = 0.75f;

/** Attack and release of the transient detector, as in the example. */
const float DUCKER_ALPHA = 0.95f;
const float DUCKER_BETA = 0.995f;

/** Glide of the dry/wet mix, in mix units per second (5 ms, as the fades of
 * the other modules: enough to keep a modulated amount free of zipper). */
const float AMOUNT_GLIDE_RATE = 1.f / 0.005f;

} // namespace


Decorrelator::~Decorrelator() {
	destroy();
}


void Decorrelator::destroy() {
	if (hDucker)
		transientDucker_destroy(&hDucker);
	if (hDecor)
		latticeDecorrelator_destroy(&hDecor);
	if (hSTFT)
		afSTFT_destroy(&hSTFT);
	free(inputTD);
	free(outputTD);
	free(inputTF);
	free(outputTF);
	free(residualTF);
	free(transientTF);
	inputTD = outputTD = nullptr;
	inputTF = outputTF = residualTF = transientTF = nullptr;
}


void Decorrelator::setSampleRate(float newSampleRate) {
	if (newSampleRate == sampleRate && isReady())
		return;
	destroy();
	sampleRate = newSampleRate;

	afSTFT_create(&hSTFT, MAX_NUM_CHANNELS, MAX_NUM_CHANNELS, HOP_SIZE, 0, 1, AFSTFT_BANDS_CH_TIME);
	freqVector.resize(NUM_BANDS);
	afSTFT_getCentreFreqs(hSTFT, sampleRate, NUM_BANDS, freqVector.data());

	latticeDecorrelator_create(&hDecor,
	                           sampleRate,
	                           HOP_SIZE,
	                           freqVector.data(),
	                           NUM_BANDS,
	                           MAX_NUM_CHANNELS,
	                           (int*) APF_ORDERS,
	                           (float*) APF_CUTOFFS,
	                           4,
	                           MAX_TF_DELAY,
	                           0,
	                           ENERGY_COMP_COEFF);
	transientDucker_create(&hDucker, MAX_NUM_CHANNELS, NUM_BANDS);

	inputTD = (float**) calloc2d(MAX_NUM_CHANNELS, HOP_SIZE, sizeof(float));
	outputTD = (float**) calloc2d(MAX_NUM_CHANNELS, HOP_SIZE, sizeof(float));
	inputTF = (void***) calloc3d(NUM_BANDS, MAX_NUM_CHANNELS, 1, sizeof(float_complex));
	outputTF = (void***) calloc3d(NUM_BANDS, MAX_NUM_CHANNELS, 1, sizeof(float_complex));
	residualTF = (void***) calloc3d(NUM_BANDS, MAX_NUM_CHANNELS, 1, sizeof(float_complex));
	transientTF = (void***) calloc3d(NUM_BANDS, MAX_NUM_CHANNELS, 1, sizeof(float_complex));

	latency = HOP_SIZE + afSTFT_getProcDelay(hSTFT);
	dryLines.assign((size_t) MAX_NUM_CHANNELS * latency, 0.f);
	hopPos = 0;
	dryPos = 0;
	amount = amountTarget;
	updateMix();
}


void Decorrelator::reset() {
	if (!isReady())
		return;
	afSTFT_clearBuffers(hSTFT);
	latticeDecorrelator_reset(hDecor);
	memset(FLATTEN2D(inputTD), 0, MAX_NUM_CHANNELS * HOP_SIZE * sizeof(float));
	memset(FLATTEN2D(outputTD), 0, MAX_NUM_CHANNELS * HOP_SIZE * sizeof(float));
	std::fill(dryLines.begin(), dryLines.end(), 0.f);
	hopPos = 0;
	dryPos = 0;
	amount = amountTarget;
	updateMix();
	// The transient detectors have no reset of their own. They hold two
	// envelopes per band and channel, which decay on their own, and a signal
	// starting from silence reads as a transient either way.
}


void Decorrelator::setNumChannels(int newNumChannels) {
	numChannels = std::min(std::max(newNumChannels, 0), (int) MAX_NUM_CHANNELS);
	setCompensateLevel(compensateLevel);
}


void Decorrelator::setAmount(float newAmount) {
	amountTarget = std::min(std::max(newAmount, 0.f), 1.f);
}


/** Constant-power crossfade: the dry and the decorrelated signal are largely
 * incoherent, so their gains follow a quarter of a cosine period and the level
 * stays flat instead of dipping by 3 dB halfway. */
void Decorrelator::updateMix() {
	float angle = amount * 0.5f * float(M_PI);
	dryMix = std::cos(angle);
	wetMix = std::sin(angle);
}


void Decorrelator::setCompensateLevel(bool enabled) {
	compensateLevel = enabled;
	// Coherent channels sum to n times one of them, incoherent ones only to
	// sqrt(n), so the decorrelated signals are scaled by n / sqrt(n) to sum to
	// the same level. The 0.75 factor is the one the SAF example applies. Only
	// what went through the decorrelator is scaled: bypassed transients still
	// sum coherently.
	compGain = enabled && numChannels > 0 ? 0.75f * std::sqrt((float) numChannels) : 1.f;
}


void Decorrelator::setBypassTransients(bool enabled) {
	bypassTransients = enabled;
}


void Decorrelator::runHop() {
	afSTFT_forward_knownDimensions(hSTFT, inputTD, HOP_SIZE, MAX_NUM_CHANNELS, 1, (float_complex***) inputTF);

	// Only the residual left by the ducker is decorrelated; the transients it
	// took out are added back below.
	if (bypassTransients) {
		transientDucker_apply(hDucker,
		                      (float_complex***) inputTF,
		                      1,
		                      DUCKER_ALPHA,
		                      DUCKER_BETA,
		                      (float_complex***) residualTF,
		                      (float_complex***) transientTF);
		latticeDecorrelator_apply(hDecor, (float_complex***) residualTF, 1, (float_complex***) outputTF);
	}
	else {
		latticeDecorrelator_apply(hDecor, (float_complex***) inputTF, 1, (float_complex***) outputTF);
	}

	// The bands hold one time slot of every channel, as real and imaginary
	// parts next to each other.
	if (compGain != 1.f || bypassTransients) {
		const int bandLen = 2 * MAX_NUM_CHANNELS;
		for (int band = 0; band < NUM_BANDS; band++) {
			float* wet = (float*) FLATTEN2D(((float_complex***) outputTF)[band]);
			if (compGain != 1.f) {
				for (int i = 0; i < bandLen; i++)
					wet[i] *= compGain;
			}
			if (bypassTransients) {
				const float* transient = (float*) FLATTEN2D(((float_complex***) transientTF)[band]);
				for (int i = 0; i < bandLen; i++)
					wet[i] += transient[i];
			}
		}
	}

	afSTFT_backward_knownDimensions(hSTFT, (float_complex***) outputTF, HOP_SIZE, MAX_NUM_CHANNELS, 1, outputTD);
}


void Decorrelator::process(const float* in, float* out) {
	if (!isReady()) {
		for (int c = 0; c < numChannels; c++)
			out[c] = in[c];
		return;
	}

	if (amount != amountTarget) {
		float step = AMOUNT_GLIDE_RATE / sampleRate;
		amount =
			amount < amountTarget ? std::fmin(amount + step, amountTarget) : std::fmax(amount - step, amountTarget);
		updateMix();
	}

	// The channels the input does not carry are fed silence, so that what is
	// left in their all-pass filters and in their dry line rings out instead of
	// coming back when they are used again.
	for (int c = 0; c < MAX_NUM_CHANNELS; c++) {
		float sample = c < numChannels ? in[c] : 0.f;
		inputTD[c][hopPos] = sample;

		float* dryLine = &dryLines[(size_t) c * latency];
		if (c < numChannels)
			out[c] = dryMix * dryLine[dryPos] + wetMix * outputTD[c][hopPos];
		dryLine[dryPos] = sample;
	}

	if (++dryPos == latency)
		dryPos = 0;
	if (++hopPos == HOP_SIZE) {
		hopPos = 0;
		runHop();
	}
}


} // namespace sp4t
