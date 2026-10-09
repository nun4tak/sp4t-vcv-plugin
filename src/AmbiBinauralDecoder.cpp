#include "AmbiBinauralDecoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "saf.h"


namespace sp4t {


// SAF defines NUM_EARS as 2, which AmbiBinauralDecoder::NUM_CHANNELS mirrors.
static_assert(AmbiBinauralDecoder::NUM_CHANNELS == NUM_EARS, "unexpected number of ears");

namespace {

BINAURAL_AMBI_DECODER_METHODS safMethod(AmbiBinauralDecoder::Method method) {
	switch (method) {
		case AmbiBinauralDecoder::METHOD_LS: return BINAURAL_DECODER_LS;
		case AmbiBinauralDecoder::METHOD_LSDIFFEQ: return BINAURAL_DECODER_LSDIFFEQ;
		case AmbiBinauralDecoder::METHOD_SPR: return BINAURAL_DECODER_SPR;
		case AmbiBinauralDecoder::METHOD_TA: return BINAURAL_DECODER_TA;
		default: return BINAURAL_DECODER_MAGLS;
	}
}

int nextPow2(int n) {
	int p = 1;
	while (p < n)
		p *= 2;
	return p;
}

/** ambi_bin's truncation EQ: a per-bin gain compensating the high-frequency
 * roll-off that truncating the Ambisonic order (and the max-rE tapering)
 * introduces in the diffuse field. */
void applyTruncationEQ(int order, int nSH, bool maxRE, const float* freqVector, int nBins, float_complex* decMtx) {
	const int targetOrder = 42;      // the diffuse field it is equalised to
	const float softThreshold = 9.f; // dB, boost soft-clipped above it
	const double radius = 0.085;     // head radius, m
	const double soundSpeed = 343.0;

	std::vector<double> kr(nBins);
	for (int k = 0; k < nBins; k++)
		kr[k] = 2.0 * SAF_PId / soundSpeed * freqVector[k] * radius;

	std::vector<float> w_n(order + 1, 1.f);
	if (maxRE) {
		std::vector<float> maxRECoeffs(order + 1, 0.f);
		beamWeightsMaxEV(order, maxRECoeffs.data());
		for (int n = 0; n <= order; n++)
			w_n[n] = maxRECoeffs[n] / std::sqrt((2.f * n + 1.f) / (4.f * SAF_PI));
		// descending, so that the n = 0 weight is normalized last
		for (int n = order; n >= 0; n--)
			w_n[n] /= w_n[0];
	}

	std::vector<float> gain(nBins, 1.f);
	truncationEQ(w_n.data(), order, targetOrder, kr.data(), nBins, softThreshold, gain.data());

	for (int k = 0; k < nBins; k++)
		for (int i = 0; i < NUM_EARS * nSH; i++)
			decMtx[k * NUM_EARS * nSH + i] = crmulf(decMtx[k * NUM_EARS * nSH + i], gain[k]);
}

} // namespace


AmbiBinauralDecoder::AmbiBinauralDecoder() : delayLines((size_t) MAX_NUM_SH * 2 * MAX_FILTER_LEN, 0.f) {}


void AmbiBinauralDecoder::build(const Options& options) {
	const int nDirs = __default_N_hrir_dirs;
	int fs = std::max((int) std::lround(options.sampleRate), 1);

	std::vector<float> dirs((const float*) __default_hrir_dirs_deg, (const float*) __default_hrir_dirs_deg + nDirs * 2);

	// Default HRIR set, resampled to the host sample rate when needed.
	int hrirLen = __default_hrir_len;
	std::vector<float> hrirs((const float*) __default_hrirs,
	                         (const float*) __default_hrirs + (size_t) nDirs * NUM_EARS * hrirLen);
	if (fs != __default_hrir_fs) {
		float* resampled = NULL;
		int resampledLen = 0;
		resampleHRIRs(hrirs.data(), nDirs, hrirLen, __default_hrir_fs, fs, 1, &resampled, &resampledLen);
		hrirLen = resampledLen;
		hrirs.assign(resampled, resampled + (size_t) nDirs * NUM_EARS * hrirLen);
		free(resampled);
	}

	// Twice the HRIR length keeps the HRTFs free of time aliasing, and half of
	// the inverse FFT is long enough to hold the decoding filters (see the
	// energy window below).
	const int filterLen = std::min(nextPow2(hrirLen), (int) MAX_FILTER_LEN);
	const int fftSize = 2 * filterLen;
	const int nBins = fftSize / 2 + 1;

	// Integration weights of the measurement grid, in the [azimuth,
	// inclination] convention calculateGridWeights() expects.
	std::vector<float> dirsRad(dirs);
	for (float& angle : dirsRad)
		angle *= SAF_PI / 180.f;
	sphElev2incl(dirsRad.data(), nDirs, 0, dirsRad.data());
	std::vector<float> weights(nDirs, 0.f);
	float* weightsPtr = calculateGridWeights(dirsRad.data(), nDirs, -1, weights.data()) >= 1 ? weights.data() : NULL;

	std::vector<float> itds(nDirs, 0.f);
	estimateITDs(hrirs.data(), nDirs, hrirLen, fs, itds.data());

	std::vector<float_complex> hrtfs((size_t) nBins * NUM_EARS * nDirs);
	HRIRs2HRTFs(hrirs.data(), nDirs, hrirLen, fftSize, hrtfs.data());
	// Diffuse-field equalisation of the HRTFs, ambi_bin's default pre-processing.
	diffuseFieldEqualiseHRTFs(nDirs, itds.data(), NULL, nBins, weightsPtr, 1, 0, hrtfs.data());

	std::vector<float> freqVector(nBins, 0.f);
	getUniformFreqVector(fftSize, (float) fs, freqVector.data());

	FilterSet& set = sets[activeSet.load(std::memory_order_relaxed) == 0 ? 1 : 0];
	set.filterLen = filterLen;

	void* fft = NULL;
	saf_rfft_create(&fft, fftSize);
	std::vector<float_complex> decMtx;
	std::vector<float_complex> bins(nBins);
	std::vector<float> ir((size_t) NUM_EARS * MAX_NUM_SH * fftSize);
	std::vector<double> energy(fftSize);

	for (int n = MIN_ORDER; n <= MAX_ORDER; n++) {
		const int nSH = (n + 1) * (n + 1);
		decMtx.assign((size_t) nBins * NUM_EARS * nSH, cmplxf(0.f, 0.f));
		getBinauralAmbiDecoderMtx(hrtfs.data(),
		                          dirs.data(),
		                          nDirs,
		                          nBins,
		                          safMethod(options.method),
		                          n,
		                          freqVector.data(),
		                          itds.data(),
		                          weightsPtr,
		                          options.diffuseMatching ? 1 : 0,
		                          options.maxRE ? 1 : 0,
		                          decMtx.data());

		if (options.truncationEQ && options.method == METHOD_LS)
			applyTruncationEQ(n, nSH, options.maxRE, freqVector.data(), nBins, decMtx.data());

		// Inverse FFT of the decoding matrices, into one filter per ear and
		// input channel, and the energy of all of them at every tap.
		std::fill(energy.begin(), energy.end(), 0.0);
		for (int ear = 0; ear < NUM_EARS; ear++) {
			for (int j = 0; j < nSH; j++) {
				for (int k = 0; k < nBins; k++)
					bins[k] = decMtx[(size_t) k * NUM_EARS * nSH + ear * nSH + j];
				float* filter = &ir[((size_t) ear * nSH + j) * fftSize];
				saf_rfft_backward(fft, bins.data(), filter);
				for (int k = 0; k < fftSize; k++)
					energy[k] += (double) filter[k] * filter[k];
			}
		}

		// The filters are circular: keep the filterLen consecutive taps, wrapping
		// around, that hold the most energy. Taps before the first one are the
		// non-causal part of the filters, i.e. the latency of the decoder.
		double window = 0.0;
		for (int k = 0; k < filterLen; k++)
			window += energy[k];
		double best = -1.0;
		int start = 0;
		for (int p = 0; p < fftSize; p++) {
			if (window > best) {
				best = window;
				start = p;
			}
			window += energy[(p + filterLen) % fftSize] - energy[p];
		}
		set.latency[n - 1] = start > fftSize / 2 ? fftSize - start : 0;

		std::vector<float>& taps = set.taps[n - 1];
		taps.assign((size_t) NUM_EARS * nSH * filterLen, 0.f);
		for (int i = 0; i < NUM_EARS * nSH; i++)
			for (int k = 0; k < filterLen; k++)
				taps[(size_t) i * filterLen + k] = ir[(size_t) i * fftSize + (start + k) % fftSize];
	}

	saf_rfft_destroy(&fft);

	set.generation = ++generation;
	activeSet.store(&set == &sets[0] ? 0 : 1, std::memory_order_release);
}


void AmbiBinauralDecoder::reset() {
	clearDelayLines();
}


void AmbiBinauralDecoder::setOrder(int newOrder) {
	newOrder = std::min(std::max(newOrder, (int) MIN_ORDER), (int) MAX_ORDER);
	if (newOrder == order)
		return;
	order = newOrder;
	numSH = (order + 1) * (order + 1);
	// The delay lines of the channels the new order adds hold stale samples.
	clearDelayLines();
}


int AmbiBinauralDecoder::getLatency() const {
	int index = activeSet.load(std::memory_order_acquire);
	return index < 0 ? 0 : sets[index].latency[order - 1];
}


void AmbiBinauralDecoder::clearDelayLines() {
	std::fill(delayLines.begin(), delayLines.end(), 0.f);
	writePos = 0;
}


void AmbiBinauralDecoder::process(const float* in, float* out) {
	int index = activeSet.load(std::memory_order_acquire);
	if (index < 0) {
		out[0] = out[1] = 0.f;
		return;
	}

	const FilterSet& set = sets[index];
	if (set.generation != usedGeneration) {
		usedGeneration = set.generation;
		filterLen = set.filterLen;
		delayStride = 2 * filterLen;
		clearDelayLines();
	}

	const int len = filterLen;
	float* lines = delayLines.data();
	for (int j = 0; j < numSH; j++) {
		float* line = lines + (size_t) j * delayStride + writePos;
		line[0] = in[j];
		line[len] = in[j];
	}

	const float* tapsLeft = set.taps[order - 1].data();
	const float* tapsRight = tapsLeft + (size_t) numSH * len;
	float left = 0.f;
	float right = 0.f;
	for (int j = 0; j < numSH; j++) {
		const float* line = lines + (size_t) j * delayStride + writePos;
		const float* filterLeft = tapsLeft + (size_t) j * len;
		const float* filterRight = tapsRight + (size_t) j * len;
		for (int k = 0; k < len; k++) {
			left += filterLeft[k] * line[k];
			right += filterRight[k] * line[k];
		}
	}
	out[0] = left;
	out[1] = right;

	if (--writePos < 0)
		writePos = len - 1;
}


} // namespace sp4t
