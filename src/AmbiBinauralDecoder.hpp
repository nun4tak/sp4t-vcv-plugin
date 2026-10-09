#pragma once

#include <atomic>
#include <cstdint>
#include <vector>


namespace sp4t {


/** Binaural (headphone) Ambisonic decoder.
 *
 * Input signals follow the ambiX convention (ACN channel ordering, SN3D
 * normalization). Adapted from SAF's `ambi_bin` example, which renders in the
 * frequency domain through an afSTFT filterbank: here the per-bin decoding
 * matrices are turned into a pair of FIR filters per input channel
 * (SAF's getBinauralAmbiDecoderFilters(), inlined so that the truncation EQ
 * can be applied per bin), which convolve the input sample by sample. Only
 * SAF's default HRIR set is used, resampled to the host sample rate.
 *
 * The decoding filters of every order are computed in one go so that changing
 * the order, i.e. plugging a cable with a different number of channels, takes
 * effect at once. Computing them takes about a second and allocates, so
 * build() must be called from a worker thread; process() outputs silence until
 * the first set of filters is ready, and keeps using the previous set while a
 * new one is being computed.
 *
 * The filters come out of the inverse FFT circular, and only the MAX_FILTER_LEN
 * taps holding the most energy are kept. The decoders that preserve the phase
 * of the HRTFs (LS, LSDIFFEQ, SPR) are causal and add no latency; the ones that
 * rework it (TA, MAGLS) are centred on tap 0 and their non-causal part becomes
 * the latency of the decoder, which getLatency() reports.
 */
struct AmbiBinauralDecoder {
	enum {
		MIN_ORDER = 1,
		MAX_ORDER = 3,
		MAX_NUM_SH = (MAX_ORDER + 1) * (MAX_ORDER + 1),
		NUM_CHANNELS = 2,
		/** Longest decoding filter, in taps. The HRIRs are 256 taps at 48 kHz,
		 * so the cap only bites above 192 kHz, where they get truncated. */
		MAX_FILTER_LEN = 1024,
	};

	/** The binaural decoders of SAF's #BINAURAL_AMBI_DECODER_METHODS. */
	enum Method {
		METHOD_LS,
		METHOD_LSDIFFEQ,
		METHOD_SPR,
		METHOD_TA,
		METHOD_MAGLS,
		NUM_METHODS
	};

	/** Everything the decoding filters depend on. */
	struct Options {
		float sampleRate = 48000.f;
		Method method = METHOD_LS;
		bool maxRE = true;
		bool diffuseMatching = false;
		/** Compensates the high-frequency roll-off of the order truncation.
		 * Only used by METHOD_LS, the other decoders handle it themselves. */
		bool truncationEQ = true;

		bool operator==(const Options& other) const {
			return sampleRate == other.sampleRate && method == other.method && maxRE == other.maxRE
			       && diffuseMatching == other.diffuseMatching && truncationEQ == other.truncationEQ;
		}
		bool operator!=(const Options& other) const { return !(*this == other); }
	};

	AmbiBinauralDecoder();

	/** Computes the decoding filters of every order. Slow and allocating:
	 * call it from a worker thread, never from the audio thread. */
	void build(const Options& options);

	bool isReady() const { return activeSet.load(std::memory_order_acquire) >= 0; }

	/** Clears the delay lines. */
	void reset();

	/** Sets the Ambisonic order of the input signal (clamped to 1..MAX_ORDER). */
	void setOrder(int order);
	int getOrder() const { return order; }
	/** Number of input channels, i.e., (order + 1)^2. */
	int getNumSH() const { return numSH; }

	/** Latency of the decoding filters of the current order, in samples. */
	int getLatency() const;

	/** Decodes one sample.
	 *
	 * @param in  getNumSH() input samples
	 * @param out the left and right output samples
	 */
	void process(const float* in, float* out);

private:
	/** The decoding filters of every order, as published by build(). */
	struct FilterSet {
		uint32_t generation = 0;
		int filterLen = 0;
		int latency[MAX_ORDER] = {0, 0, 0};
		/** Filters of order n + 1, FLAT: NUM_CHANNELS x numSH x filterLen */
		std::vector<float> taps[MAX_ORDER];
	};

	/** Double buffer: build() always writes the set process() is not using,
	 * then publishes it with `activeSet` (-1 until the first set is ready). */
	FilterSet sets[2];
	std::atomic<int> activeSet{-1};
	uint32_t generation = 0;

	int order = 1;
	int numSH = 4;
	uint32_t usedGeneration = 0;
	int filterLen = 0;
	/** Distance between two delay lines, i.e., twice the filter length. */
	int delayStride = 0;
	int writePos = 0;
	/** The input samples of every channel, MAX_NUM_SH x (2 x filterLen). Each
	 * line holds two copies of the samples so that the convolution reads them
	 * contiguously whatever the write position. */
	std::vector<float> delayLines;

	void clearDelayLines();
};


} // namespace sp4t
