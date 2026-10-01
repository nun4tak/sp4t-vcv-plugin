#include "AmbiStereoDecoder.hpp"
#include "SphericalCoordinates.hpp"

#include <algorithm>
#include <cmath>

#include "saf.h"


namespace sp4t {


AmbiStereoDecoder::AmbiStereoDecoder(int blockSize)
	: blockSize(std::max(blockSize, 1)),
	  maxREWeights(MAX_ORDER * MAX_NUM_SH, 0.f),
	  sn3dScale(MAX_NUM_SH, 0.f),
	  targetGains(NUM_CHANNELS * MAX_NUM_SH, 0.f),
	  gains(NUM_CHANNELS * MAX_NUM_SH, 0.f),
	  gainSteps(NUM_CHANNELS * MAX_NUM_SH, 0.f) {
	// getMaxREweights() allocates, so the weights are computed once here.
	for (int n = 1; n <= MAX_ORDER; n++)
		getMaxREweights(n, 0, &maxREWeights[(n - 1) * MAX_NUM_SH]);

	for (int j = 0; j < MAX_NUM_SH; j++) {
		int n = (int) std::floor(std::sqrt((float) j));
		sn3dScale[j] = std::sqrt(2.f * n + 1.f);
	}
}


void AmbiStereoDecoder::reset() {
	dirty = true;
	samplesUntilUpdate = 0;
	jumpToTarget = true;
}


void AmbiStereoDecoder::setOrder(int newOrder) {
	newOrder = std::min(std::max(newOrder, 1), (int) MAX_ORDER);
	if (newOrder == order)
		return;
	order = newOrder;
	numSH = (order + 1) * (order + 1);
	dirty = true;
}


void AmbiStereoDecoder::setSpread(float newSpread) {
	newSpread = std::min(std::max(newSpread, 0.f), 90.f);
	if (newSpread == spread)
		return;
	spread = newSpread;
	dirty = true;
}


void AmbiStereoDecoder::setCrudeMode(bool enabled) {
	if (enabled == crudeMode)
		return;
	crudeMode = enabled;
	dirty = true;
}


void AmbiStereoDecoder::updateGains() {
	int previousNumGains = numGains;
	numGains = crudeMode ? 2 : numSH;

	if (dirty) {
		std::fill(targetGains.begin(), targetGains.end(), 0.f);

		if (crudeMode) {
			// Mid-side decoding from the omni and left-right channels.
			float sinSpread = std::sin(spread * DEG2RAD);
			targetGains[0] = 0.5f;
			targetGains[1] = 0.5f * sinSpread;
			targetGains[MAX_NUM_SH] = 0.5f;
			targetGains[MAX_NUM_SH + 1] = -0.5f * sinSpread;
		}
		else {
			const float* a_n = &maxREWeights[(order - 1) * MAX_NUM_SH];
			for (int i = 0; i < NUM_CHANNELS; i++) {
				float direction[2] = {i == 0 ? spread : -spread, 0.f};
				// ACN/N3D spherical harmonics of the speaker direction.
				float sh[MAX_NUM_SH];
				getRSH_recur(order, direction, 1, sh);

				// Gain of the beam in its own direction: dividing by it keeps
				// the output of a source at a speaker direction at unity.
				float beamGain = 0.f;
				for (int j = 0; j < numSH; j++)
					beamGain += sh[j] * sh[j] * a_n[j];

				for (int j = 0; j < numSH; j++)
					targetGains[i * MAX_NUM_SH + j] = sh[j] * a_n[j] * sn3dScale[j] / beamGain;
			}
		}

		// Fade out the channels that the new gains no longer use (never beyond
		// the channels of the input signal).
		if (!jumpToTarget)
			numGains = std::min(std::max(numGains, previousNumGains), numSH);
		dirty = false;
	}

	float invBlockSize = 1.f / blockSize;
	for (int k = 0; k < NUM_CHANNELS * MAX_NUM_SH; k++) {
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


void AmbiStereoDecoder::process(const float* in, float* out) {
	if (samplesUntilUpdate <= 0)
		updateGains();

	float left = 0.f;
	float right = 0.f;
	// setOrder() takes effect at once, before the gains are updated.
	int channels = std::min(numGains, numSH);
	for (int j = 0; j < channels; j++) {
		left += gains[j] * in[j];
		right += gains[MAX_NUM_SH + j] * in[j];
		gains[j] += gainSteps[j];
		gains[MAX_NUM_SH + j] += gainSteps[MAX_NUM_SH + j];
	}
	out[0] = left;
	out[1] = right;

	samplesUntilUpdate--;
}


} // namespace sp4t
