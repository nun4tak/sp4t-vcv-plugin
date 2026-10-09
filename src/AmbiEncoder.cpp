#include "AmbiEncoder.hpp"

#include <algorithm>
#include <cmath>

#include "saf.h"


namespace sp4t {


namespace {

float wrapDegrees(float angle) {
	angle = std::fmod(angle + 180.f, 360.f);
	if (angle < 0.f)
		angle += 360.f;
	return angle - 180.f;
}

} // namespace


AmbiEncoder::AmbiEncoder(int maxNumSources, int blockSize)
	: maxNumSources(std::max(maxNumSources, 1)),
	  blockSize(std::max(blockSize, 1)),
	  directions(2 * this->maxNumSources, 0.f),
	  dirty(this->maxNumSources, 1),
	  targetGains(MAX_NUM_SH * this->maxNumSources, 0.f),
	  gains(MAX_NUM_SH * this->maxNumSources, 0.f),
	  gainSteps(MAX_NUM_SH * this->maxNumSources, 0.f) {}


void AmbiEncoder::reset() {
	std::fill(dirty.begin(), dirty.end(), 1);
	samplesUntilUpdate = 0;
	// TODO: reset should probably also avoid audio clicks in some/all cases.
	// - setOrder: I guess the first channels still emit uninterrupted audio
	//   with VCV rack when the number of polyphonic channel changes?
	// - setNumSources: existing sources continue to emit audio
	jumpToTarget = true;
}


void AmbiEncoder::setOrder(int newOrder) {
	newOrder = std::min(std::max(newOrder, 1), (int) MAX_ORDER);
	if (newOrder == order)
		return;
	order = newOrder;
	numSH = (order + 1) * (order + 1);
	reset();
}


void AmbiEncoder::setNumSources(int newNumSources) {
	newNumSources = std::min(std::max(newNumSources, 1), maxNumSources);
	if (newNumSources == numSources)
		return;
	numSources = newNumSources;
	setPostScaling(postScaling);
	reset();
}


void AmbiEncoder::setPostScaling(bool enabled) {
	postScaling = enabled;
	scale = postScaling ? 1.f / std::sqrt((float) numSources) : 1.f;
}


void AmbiEncoder::setSourceDirection(int index, float azimuth, float elevation) {
	if (index < 0 || index >= maxNumSources)
		return;

	azimuth = wrapDegrees(azimuth);
	elevation = std::min(std::max(elevation, -90.f), 90.f);

	float* direction = &directions[2 * index];
	if (direction[0] != azimuth || direction[1] != elevation) {
		direction[0] = azimuth;
		direction[1] = elevation;
		dirty[index] = 1;
	}
}


void AmbiEncoder::updateGains() {
	// recalulate SHs (only if encoding direction has changed)
	float sh[MAX_NUM_SH];
	for (int i = 0; i < numSources; i++) {
		if (!dirty[i])
			continue;
		// ACN/N3D gains (no heap allocation for a single direction), converted to SN3D.
		getRSH_recur(order, &directions[2 * i], 1, sh);
		convertHOANormConvention(sh, order, 1, HOA_NORM_N3D, HOA_NORM_SN3D);
		for (int j = 0; j < numSH; j++)
			targetGains[j * maxNumSources + i] = sh[j];
		dirty[i] = 0;
	}

	// apply linear fade over `blockSize` number of samples
	// unless jump to target is enabled
	// (no effect if new `targetGains` are unchanged)
	float invBlockSize = 1.f / blockSize;
	for (int j = 0; j < numSH; j++) {
		for (int i = 0; i < numSources; i++) {
			int k = j * maxNumSources + i;
			if (jumpToTarget) {
				gains[k] = targetGains[k];
				gainSteps[k] = 0.f;
			}
			else {
				gainSteps[k] = (targetGains[k] - gains[k]) * invBlockSize;
			}
		}
	}
	jumpToTarget = false;

	// TODO: (benbovy) move this just after calling
	// updateGains in the process method (makes better sense)
	samplesUntilUpdate = blockSize;
}


void AmbiEncoder::process(const float* in, float* out) {
	if (samplesUntilUpdate <= 0)
		updateGains();

	for (int j = 0; j < numSH; j++) {
		float sum = 0.f;
		for (int i = 0; i < numSources; i++) {
			int k = j * maxNumSources + i;
			sum += gains[k] * in[i];
			gains[k] += gainSteps[k];
		}
		out[j] = scale * sum;
	}
	samplesUntilUpdate--;
}


} // namespace sp4t
