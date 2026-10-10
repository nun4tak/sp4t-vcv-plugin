#include "VelvetDecorrelator.hpp"

#include <algorithm>
#include <cmath>
#include <random>


namespace sp4t {


namespace {

/** Impulses per second. Dense enough that the magnitude response is flat and
 * the smearing is heard as space rather than as a flutter, sparse enough that
 * a sample costs a few dozen additions. */
const float DENSITY = 1500.f;

/** How far an impulse may land from where it is due, as a share of the
 * interval between two of them. Less than the whole of it, so that two
 * impulses never fall on the same sample. */
const float JITTER = 0.9f;

/** Length of a sequence, in seconds: how far the energy of a sample is spread. */
const float LENGTH = 0.03f;

/** Glide of the dry/wet mix, in mix units per second, as in Decorrelator. */
const float AMOUNT_GLIDE_RATE = 1.f / 0.005f;

/** Drawn from a fixed seed, so that a patch sounds the same every time. */
const unsigned SEED = 0x5eedu;

} // namespace


void VelvetDecorrelator::setSampleRate(float newSampleRate) {
	if (newSampleRate == sampleRate || newSampleRate <= 0.f)
		return;
	sampleRate = newSampleRate;

	const float interval = sampleRate / DENSITY;
	numTaps = std::max((int) std::round(LENGTH * DENSITY), 1);

	// A power of two, so that reading the line back wraps with a mask.
	lineLength = 1;
	while (lineLength < (int) std::ceil(numTaps * interval) + 1)
		lineLength *= 2;
	lineMask = lineLength - 1;

	std::mt19937 random(SEED);
	std::uniform_real_distribution<float> position(0.f, JITTER);
	std::bernoulli_distribution negative(0.5);

	taps.assign((size_t) MAX_NUM_CHANNELS * numTaps, 0);
	for (int c = 0; c < MAX_NUM_CHANNELS; c++) {
		for (int t = 0; t < numTaps; t++) {
			// One impulse per interval, landing anywhere in it but the first
			// sample, which would be the dry signal coming through.
			int offset = 1 + (int) ((t + position(random)) * interval);
			taps[(size_t) c * numTaps + t] = negative(random) ? -offset : offset;
		}
	}

	lines.assign((size_t) MAX_NUM_CHANNELS * lineLength, 0.f);
	bass.assign(MAX_NUM_CHANNELS, 0.f);
	writePos = 0;

	setBassSplit(bassSplit);
	setCompensateLevel(compensateLevel);
}


void VelvetDecorrelator::reset() {
	std::fill(lines.begin(), lines.end(), 0.f);
	std::fill(bass.begin(), bass.end(), 0.f);
	writePos = 0;
	amount = amountTarget;
	updateMix();
}


void VelvetDecorrelator::setNumChannels(int newNumChannels) {
	numChannels = std::min(std::max(newNumChannels, 0), (int) MAX_NUM_CHANNELS);
	setCompensateLevel(compensateLevel);
}


void VelvetDecorrelator::setAmount(float newAmount) {
	amountTarget = std::min(std::max(newAmount, 0.f), 1.f);
}


void VelvetDecorrelator::setCompensateLevel(bool enabled) {
	compensateLevel = enabled;
	// The impulses are incoherent, so their sum grows as their square root.
	float flatten = numTaps > 0 ? 1.f / std::sqrt((float) numTaps) : 1.f;
	float channels = enabled && numChannels > 0 ? 0.75f * std::sqrt((float) numChannels) : 1.f;
	tapGain = flatten * channels;
}


void VelvetDecorrelator::setBassSplit(float frequency) {
	bassSplit = std::max(frequency, 0.f);
	// One pole, and the high band is what is left of the signal: the two add
	// back up to it exactly, whatever the frequency.
	bassCoeff = bassSplit > 0.f && sampleRate > 0.f ? 1.f - std::exp(-2.f * float(M_PI) * bassSplit / sampleRate) : 0.f;
}


void VelvetDecorrelator::updateMix() {
	float angle = amount * 0.5f * float(M_PI);
	dryMix = std::cos(angle);
	wetMix = std::sin(angle);
}


void VelvetDecorrelator::process(const float* in, float* out) {
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

	for (int c = 0; c < numChannels; c++) {
		float* line = &lines[(size_t) c * lineLength];
		const int* tap = &taps[(size_t) c * numTaps];

		// The bass goes straight to the output; only what is above the split
		// is spread out.
		bass[c] += bassCoeff * (in[c] - bass[c]);
		float high = in[c] - bass[c];
		line[writePos] = high;

		float spread = 0.f;
		for (int t = 0; t < numTaps; t++) {
			int offset = tap[t];
			float sample = line[(writePos - std::abs(offset)) & lineMask];
			spread += offset < 0 ? -sample : sample;
		}

		out[c] = bass[c] + dryMix * high + wetMix * tapGain * spread;
	}

	writePos = (writePos + 1) & lineMask;
}


} // namespace sp4t
