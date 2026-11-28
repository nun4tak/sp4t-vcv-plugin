#include "AmbiSpeakerDecoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "saf.h"


namespace sp4t {


namespace {

/** Points of the t-design the normalization factors are averaged over, as in
 * ambi_dec.c: plane waves fired from every direction of a degree-30 t-design. */
const int NUM_GRID_DIRS = 480;

/** A layout is taken as horizontal when its speakers are all within a degree
 * or so of the equator, the test ambi_dec.c makes. */
const float FLAT_LAYOUT_ELEV_SUM = 5.f;

/** Quality factor of both halves of a 4th-order Linkwitz-Riley crossover, and
 * of the all-pass their outputs sum to. */
const float LR4_Q = 0.70710678f;

int numSHof(int order) {
	return (order + 1) * (order + 1);
}

float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

int clampi(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

/** Below this, a gain is taken as having reached its target. Interpolating by
 * a step of the remaining difference converges but never lands exactly, and
 * the matrix would then be nudged on every sample for nothing. */
const float GAIN_EPSILON = 1e-9f;

} // namespace


bool AmbiSpeakerDecoder::Options::operator==(const Options& other) const {
	if (sampleRate != other.sampleRate || numChannels != other.numChannels || method != other.method
	    || highMaxRE != other.highMaxRE || dualBand != other.dualBand || highOrder != other.highOrder)
		return false;
	// The low band only exists while the crossover is on.
	if (dualBand && (crossover != other.crossover || lowOrder != other.lowOrder || lowMaxRE != other.lowMaxRE))
		return false;
	for (int c = 0; c < numChannels; c++) {
		if (isSpeaker[c] != other.isSpeaker[c])
			return false;
		if (isSpeaker[c] && (azimuth[c] != other.azimuth[c] || elevation[c] != other.elevation[c]))
			return false;
	}
	return true;
}


AmbiSpeakerDecoder::AmbiSpeakerDecoder(int blockSize)
	: blockSize(std::max(blockSize, 1)),
	  highGains(MAX_CHANNELS * MAX_NUM_SH, 0.f),
	  highTarget(MAX_CHANNELS * MAX_NUM_SH, 0.f),
	  highSteps(MAX_CHANNELS * MAX_NUM_SH, 0.f),
	  lowGains(MAX_CHANNELS * MAX_NUM_SH, 0.f),
	  lowTarget(MAX_CHANNELS * MAX_NUM_SH, 0.f),
	  lowSteps(MAX_CHANNELS * MAX_NUM_SH, 0.f) {
	for (int n = 0; n < MAX_ORDER; n++) {
		for (int s = 0; s < 2; s++) {
			sets[s].high[n].assign(MAX_CHANNELS * MAX_NUM_SH, 0.f);
			sets[s].low[n].assign(MAX_CHANNELS * MAX_NUM_SH, 0.f);
		}
	}
}


void AmbiSpeakerDecoder::reset() {
	jumpToTarget = true;
	samplesUntilUpdate = 0;
	for (int c = 0; c < MAX_NUM_SH; c++) {
		lowPass[0][c].reset();
		lowPass[1][c].reset();
		allPass[c].reset();
	}
}


void AmbiSpeakerDecoder::setOrder(int newOrder) {
	newOrder = clampi(newOrder, MIN_ORDER, MAX_ORDER);
	if (newOrder == order)
		return;
	order = newOrder;
	numSH = numSHof(order);
	// The matrices of every order are published together, so a new one is
	// already there: only the interpolation has to be restarted.
	samplesUntilUpdate = 0;
}


/** Coefficients of a 2nd-order all-pass, in the bilinear form Rack's biquads
 * use: the numerator is the reversed denominator. */
void AmbiSpeakerDecoder::setFilters(float sampleRate, float frequency) {
	if (sampleRate == filterSampleRate && frequency == crossoverFreq)
		return;
	filterSampleRate = sampleRate;
	crossoverFreq = frequency;

	float f = clampf(frequency / sampleRate, 1e-4f, 0.45f);
	float k = std::tan((float) M_PI * f);
	float norm = 1.f / (1.f + k / LR4_Q + k * k);
	// Denominator shared by the two Butterworth low-pass sections and the
	// all-pass, which is the same filter with its numerator reversed.
	float a1 = 2.f * (k * k - 1.f) * norm;
	float a2 = (1.f - k / LR4_Q + k * k) * norm;
	float lpB0 = k * k * norm;

	for (int c = 0; c < MAX_NUM_SH; c++) {
		for (int s = 0; s < 2; s++) {
			Biquad& lp = lowPass[s][c];
			lp.b0 = lp.b2 = lpB0;
			lp.b1 = 2.f * lpB0;
			lp.a1 = a1;
			lp.a2 = a2;
		}
		Biquad& ap = allPass[c];
		ap.b0 = a2;
		ap.b1 = a1;
		ap.b2 = 1.f;
		ap.a1 = a1;
		ap.a2 = a2;
	}
}


void AmbiSpeakerDecoder::build(const Options& options) {
	int writeSet = 1 - std::max(activeSet.load(std::memory_order_acquire), 0);
	MatrixSet& set = sets[writeSet];

	set.numChannels = clampi(options.numChannels, 0, MAX_CHANNELS);
	set.sampleRate = options.sampleRate;
	set.crossoverFreq = options.crossover;
	set.crossover = false;
	set.layoutOrder = 0;
	for (int n = 0; n < MAX_ORDER; n++) {
		std::fill(set.high[n].begin(), set.high[n].end(), 0.f);
		std::fill(set.low[n].begin(), set.low[n].end(), 0.f);
	}

	// The speakers to decode to, and the channel each one feeds.
	int channelOfSpeaker[MAX_CHANNELS];
	float dirs[(MAX_CHANNELS + 2) * 2];
	int numSpeakers = 0;
	float elevSum = 0.f;
	for (int c = 0; c < set.numChannels; c++) {
		if (!options.isSpeaker[c])
			continue;
		channelOfSpeaker[numSpeakers] = c;
		dirs[numSpeakers * 2 + 0] = options.azimuth[c];
		dirs[numSpeakers * 2 + 1] = options.elevation[c];
		elevSum += std::fabs(options.elevation[c]);
		numSpeakers++;
	}

	if (numSpeakers == 0) {
		set.generation = ++generation;
		activeSet.store(writeSet, std::memory_order_release);
		return;
	}

	// How sharply the layout can be decoded: 2N+1 speakers for order N on a
	// horizontal layout, (N+1)^2 on a 3D one.
	bool flat = elevSum < FLAT_LAYOUT_ELEV_SUM || numSpeakers < 4;
	int maxOrder = flat ? (numSpeakers - 1) / 2 : (int) std::floor(std::sqrt((float) numSpeakers)) - 1;
	maxOrder = clampi(maxOrder, MIN_ORDER, MAX_ORDER);
	set.layoutOrder = maxOrder;

	// AllRAD triangulates the layout, which a horizontal one makes degenerate:
	// two virtual speakers at the poles fix it, and their rows are dropped once
	// the matrix is designed (as ambi_dec.c does).
	int numDesign = numSpeakers;
	if (flat && options.method == METHOD_ALLRAD) {
		dirs[numDesign * 2 + 0] = 0.f;
		dirs[numDesign * 2 + 1] = -90.f;
		dirs[numDesign * 2 + 2] = 0.f;
		dirs[numDesign * 2 + 3] = 90.f;
		numDesign += 2;
	}

	LOUDSPEAKER_AMBI_DECODER_METHODS safMethod = LOUDSPEAKER_DECODER_ALLRAD;
	if (options.method == METHOD_EPAD)
		safMethod = LOUDSPEAKER_DECODER_EPAD;
	else if (options.method == METHOD_SAD)
		safMethod = LOUDSPEAKER_DECODER_SAD;

	// Designed once at the highest order the layout supports, then truncated
	// for the lower ones, as ambi_dec.c does.
	int designSH = numSHof(maxOrder);
	std::vector<float> master(numDesign * designSH, 0.f);
	getLoudspeakerDecoderMtx(dirs, numDesign, safMethod, maxOrder, 0, master.data());

	// SN3D to N3D, folded into the gains rather than converting every sample.
	float sn3dScale[MAX_NUM_SH];
	for (int j = 0; j < MAX_NUM_SH; j++)
		sn3dScale[j] = std::sqrt(2.f * std::floor(std::sqrt((float) j)) + 1.f);

	std::vector<float> truncated(numDesign * MAX_NUM_SH, 0.f);
	float sh[MAX_NUM_SH];

	// One matrix per decoding order, amplitude and energy preserving.
	std::vector<float> ampMtx[MAX_ORDER], energyMtx[MAX_ORDER];
	for (int n = 1; n <= maxOrder; n++) {
		int nSH = numSHof(n);
		for (int i = 0; i < numDesign; i++)
			for (int j = 0; j < nSH; j++)
				truncated[i * nSH + j] = master[i * designSH + j];

		// The normalization factors come from the unweighted matrix, as in
		// ambi_dec.c: a plane wave is fired from every direction of the grid
		// and the amplitude and energy it decodes to are averaged.
		float ampSum = 0.f, energySum = 0.f;
		const float* grid = &__Tdesign_degree_30_dirs_deg[0][0];
		for (int g = 0; g < NUM_GRID_DIRS; g++) {
			float dir[2] = {grid[g * 2 + 0], grid[g * 2 + 1]};
			getRSH_recur(n, dir, 1, sh);
			float amp = 0.f, energy = 0.f;
			for (int i = 0; i < numDesign; i++) {
				float g_i = 0.f;
				for (int j = 0; j < nSH; j++)
					g_i += truncated[i * nSH + j] * sh[j];
				amp += g_i;
				energy += g_i * g_i;
			}
			ampSum += amp;
			energySum += energy;
		}
		float ampNorm = 1.f / (ampSum / NUM_GRID_DIRS + 2.23e-6f);
		float energyNorm = std::sqrt(1.f / (energySum / NUM_GRID_DIRS + 2.23e-6f));

		// max-rE weighting, a gain per order applied to the columns. Each band
		// is weighted on its own: it trades localisation sharpness for a
		// steadier image, which is not always wanted at both ends.
		float a_n[MAX_NUM_SH];
		getMaxREweights(n, 0, a_n);

		// Scattered back to the channels the speakers feed, the virtual ones
		// dropped along the way.
		ampMtx[n - 1].assign(MAX_CHANNELS * MAX_NUM_SH, 0.f);
		energyMtx[n - 1].assign(MAX_CHANNELS * MAX_NUM_SH, 0.f);
		for (int i = 0; i < numSpeakers; i++) {
			int c = channelOfSpeaker[i];
			for (int j = 0; j < nSH; j++) {
				float g = truncated[i * nSH + j] * sn3dScale[j];
				ampMtx[n - 1][c * MAX_NUM_SH + j] = g * ampNorm * (options.lowMaxRE ? a_n[j] : 1.f);
				energyMtx[n - 1][c * MAX_NUM_SH + j] = g * energyNorm * (options.highMaxRE ? a_n[j] : 1.f);
			}
		}
	}

	// The matrix each input order decodes with: the band orders capped by what
	// the input carries and by what the layout supports.
	for (int in = MIN_ORDER; in <= MAX_ORDER; in++) {
		int hi = clampi(std::min(options.highOrder, in), MIN_ORDER, maxOrder);
		int lo = clampi(std::min(options.lowOrder, in), MIN_ORDER, maxOrder);
		if (!options.dualBand) {
			set.high[in - 1] = energyMtx[hi - 1];
			continue;
		}
		set.high[in - 1] = energyMtx[hi - 1];
		// What the low-passed signal adds on top of the high-band matrix.
		for (int k = 0; k < MAX_CHANNELS * MAX_NUM_SH; k++)
			set.low[in - 1][k] = ampMtx[lo - 1][k] - energyMtx[hi - 1][k];
	}

	// Decoding both bands with the same matrix is one matrix product.
	if (options.dualBand) {
		for (int in = MIN_ORDER; in <= MAX_ORDER && !set.crossover; in++) {
			for (int k = 0; k < MAX_CHANNELS * MAX_NUM_SH; k++) {
				if (set.low[in - 1][k] != 0.f) {
					set.crossover = true;
					break;
				}
			}
		}
	}

	set.generation = ++generation;
	activeSet.store(writeSet, std::memory_order_release);
}


namespace {

/** Step of one sample toward `target`, snapping the value home once what is
 * left of the way is too small to be worth interpolating. */
float step(float& value, float target, float invBlockSize) {
	float remaining = target - value;
	if (std::fabs(remaining) < GAIN_EPSILON) {
		value = target;
		return 0.f;
	}
	return remaining * invBlockSize;
}

} // namespace


void AmbiSpeakerDecoder::updateGains() {
	int active = activeSet.load(std::memory_order_acquire);
	if (active < 0) {
		samplesUntilUpdate = blockSize;
		return;
	}
	const MatrixSet& set = sets[active];

	if (set.generation != usedGeneration) {
		usedGeneration = set.generation;
		numChannels = set.numChannels;
		layoutOrder = set.layoutOrder;
		if (set.crossover)
			setFilters(set.sampleRate, set.crossoverFreq);
		apMixTarget = set.crossover ? 1.f : 0.f;
	}

	const std::vector<float>& high = set.high[order - 1];
	const std::vector<float>& low = set.low[order - 1];
	std::copy(high.begin(), high.end(), highTarget.begin());
	std::copy(low.begin(), low.end(), lowTarget.begin());

	float invBlockSize = 1.f / blockSize;
	ramping = false;
	for (int k = 0; k < MAX_CHANNELS * MAX_NUM_SH; k++) {
		if (jumpToTarget) {
			highGains[k] = highTarget[k];
			lowGains[k] = lowTarget[k];
			highSteps[k] = lowSteps[k] = 0.f;
			continue;
		}
		highSteps[k] = step(highGains[k], highTarget[k], invBlockSize);
		lowSteps[k] = step(lowGains[k], lowTarget[k], invBlockSize);
		if (highSteps[k] != 0.f || lowSteps[k] != 0.f)
			ramping = true;
	}

	if (jumpToTarget) {
		apMix = apMixTarget;
		apMixStep = 0.f;
	}
	else {
		apMixStep = step(apMix, apMixTarget, invBlockSize);
		if (apMixStep != 0.f)
			ramping = true;
	}
	// The filters keep running until the all-pass has been faded out, so that
	// switching the crossover off does not jump the phase of the output.
	filtersActive = apMix > 0.f || apMixTarget > 0.f;

	jumpToTarget = false;
	samplesUntilUpdate = blockSize;
}


void AmbiSpeakerDecoder::process(const float* in, float* out) {
	if (samplesUntilUpdate <= 0)
		updateGains();
	samplesUntilUpdate--;

	std::memset(out, 0, MAX_CHANNELS * sizeof(float));
	if (numChannels == 0)
		return;

	float hiIn[MAX_NUM_SH];
	float lpIn[MAX_NUM_SH];
	if (filtersActive) {
		for (int j = 0; j < numSH; j++) {
			float ap = allPass[j].process(in[j]);
			lpIn[j] = lowPass[1][j].process(lowPass[0][j].process(in[j]));
			hiIn[j] = in[j] + apMix * (ap - in[j]);
		}
	}
	else {
		for (int j = 0; j < numSH; j++) {
			hiIn[j] = in[j];
			lpIn[j] = 0.f;
		}
	}

	for (int c = 0; c < numChannels; c++) {
		const float* hg = &highGains[c * MAX_NUM_SH];
		float acc = 0.f;
		for (int j = 0; j < numSH; j++)
			acc += hg[j] * hiIn[j];
		if (filtersActive) {
			const float* lg = &lowGains[c * MAX_NUM_SH];
			for (int j = 0; j < numSH; j++)
				acc += lg[j] * lpIn[j];
		}
		out[c] = acc;
	}

	if (ramping) {
		for (int c = 0; c < numChannels; c++) {
			for (int j = 0; j < numSH; j++) {
				int k = c * MAX_NUM_SH + j;
				highGains[k] += highSteps[k];
				lowGains[k] += lowSteps[k];
			}
		}
		apMix += apMixStep;
	}
}


} // namespace sp4t
