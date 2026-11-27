#include "AmbiMicPairDecoder.hpp"
#include "SphericalCoordinates.hpp"

#include <algorithm>
#include <cmath>


namespace sp4t {


const NamedMicPattern NAMED_MIC_PATTERNS[] = {
	{0.f, "omni", "OMNI"},
	{0.3f, "subcardioid", "SUB"},
	{0.5f, "cardioid", "CARD"},
	{0.634f, "supercardioid", "SUPR"},
	{0.75f, "hypercardioid", "HYPR"},
	{1.f, "figure-8", "FIG8"},
};
const int NUM_NAMED_MIC_PATTERNS = (int) (sizeof(NAMED_MIC_PATTERNS) / sizeof(NAMED_MIC_PATTERNS[0]));


const NamedMicPattern& nearestMicPattern(float pattern, bool* exact) {
	int nearest = 0;
	for (int i = 1; i < NUM_NAMED_MIC_PATTERNS; i++) {
		if (std::fabs(pattern - NAMED_MIC_PATTERNS[i].pattern)
		    < std::fabs(pattern - NAMED_MIC_PATTERNS[nearest].pattern))
			nearest = i;
	}
	if (exact)
		*exact = std::fabs(pattern - NAMED_MIC_PATTERNS[nearest].pattern) < 5e-4f;
	return NAMED_MIC_PATTERNS[nearest];
}


AmbiMicPairDecoder::AmbiMicPairDecoder(int blockSize) : blockSize(std::max(blockSize, 1)) {}


void AmbiMicPairDecoder::reset() {
	dirty = true;
	samplesUntilUpdate = 0;
	jumpToTarget = true;
}


void AmbiMicPairDecoder::setMode(int newMode) {
	newMode = std::min(std::max(newMode, 0), (int) MODES_LEN - 1);
	if (newMode == mode)
		return;
	mode = newMode;
	dirty = true;
}


void AmbiMicPairDecoder::setPattern(float newPattern) {
	newPattern = std::min(std::max(newPattern, 0.f), 1.f);
	if (newPattern == pattern)
		return;
	pattern = newPattern;
	dirty = true;
}


void AmbiMicPairDecoder::setSpread(float newSpread) {
	newSpread = std::min(std::max(newSpread, 0.f), 90.f);
	if (newSpread == spread)
		return;
	spread = newSpread;
	dirty = true;
}


void AmbiMicPairDecoder::setLevelCompensation(bool enabled) {
	if (enabled == levelCompensation)
		return;
	levelCompensation = enabled;
	dirty = true;
}


void AmbiMicPairDecoder::setNumInputChannels(int channels) {
	if (channels == numInputChannels)
		return;
	numInputChannels = channels;
	dirty = true;
}


void AmbiMicPairDecoder::updateGains() {
	if (dirty) {
		std::fill(targetGains, targetGains + NUM_CHANNELS * NUM_SH, 0.f);

		if (numInputChannels >= NUM_SH) {
			float d = pattern;
			float a = 1.f - d;
			float cosSpread = std::cos(spread * DEG2RAD);
			float sinSpread = std::sin(spread * DEG2RAD);

			// Gains of the left microphone on W, Y and X; the right one is its
			// mirror image, i.e. the same with the Y gain negated. The Z
			// channel never takes part: both microphones lie flat.
			//
			// `power` is the diffuse-field power of one output relative to an
			// omni microphone: in SN3D the W channel carries power 1 and each
			// first-order channel 1/3, and they are uncorrelated.
			float w, y, x, power;
			if (mode == MODE_MS) {
				// A mid microphone facing front and a figure-8 facing left,
				// balanced by the spread.
				w = cosSpread * a;
				x = cosSpread * d;
				y = sinSpread;
				power = cosSpread * cosSpread * (a * a + d * d / 3.f) + sinSpread * sinSpread / 3.f;
			}
			else {
				// The same pattern turned by the spread on either side of the
				// front axis.
				w = a;
				x = d * cosSpread;
				y = d * sinSpread;
				power = a * a + d * d / 3.f;
			}

			// The power stays at or above 1/3 (a figure-8), so it is never
			// close to zero.
			float scale = levelCompensation ? 1.f / std::sqrt(power) : 1.f;
			targetGains[0] = w * scale;
			targetGains[1] = y * scale;
			targetGains[3] = x * scale;
			targetGains[NUM_SH + 0] = w * scale;
			targetGains[NUM_SH + 1] = -y * scale;
			targetGains[NUM_SH + 3] = x * scale;
		}

		dirty = false;
	}

	float invBlockSize = 1.f / blockSize;
	for (int k = 0; k < NUM_CHANNELS * NUM_SH; k++) {
		if (jumpToTarget) {
			gains[k] = targetGains[k];
			gainSteps[k] = 0.f;
		}
		else {
			gainSteps[k] = (targetGains[k] - gains[k]) * invBlockSize;
		}
	}
	jumpToTarget = false;

	samplesUntilUpdate = blockSize;
}


void AmbiMicPairDecoder::process(const float* in, float* out) {
	if (samplesUntilUpdate <= 0)
		updateGains();

	float left = 0.f;
	float right = 0.f;
	for (int j = 0; j < NUM_SH; j++) {
		left += gains[j] * in[j];
		right += gains[NUM_SH + j] * in[j];
		gains[j] += gainSteps[j];
		gains[NUM_SH + j] += gainSteps[NUM_SH + j];
	}
	out[0] = left;
	out[1] = right;

	samplesUntilUpdate--;
}


} // namespace sp4t
