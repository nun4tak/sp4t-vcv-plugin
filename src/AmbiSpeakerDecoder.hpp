#pragma once

#include <atomic>
#include <cstdint>
#include <vector>


namespace sp4t {


/** Ambisonic decoder for a layout of up to 16 speakers.
 *
 * Input signals follow the ambiX convention (ACN channel ordering, SN3D
 * normalization); the SN3D to N3D conversion SAF's matrices expect is folded
 * into the decoding gains.
 *
 * Adapted from SAF's `ambi_dec` example, which renders through an afSTFT
 * filterbank: here only SAF's decoder *design* functions are used
 * (getLoudspeakerDecoderMtx(), getMaxREweights()) and the matrices are applied
 * sample per sample, so the decoder has no latency.
 *
 * Designing a matrix is slow (AllRAD triangulates a 480-point t-design) and
 * allocates, so build() must be called from a worker thread; process() outputs
 * silence until the first set of matrices is ready, and keeps using the
 * previous set while a new one is computed. The matrices of every input order
 * are computed in one go, so plugging a cable with a different number of
 * channels takes effect at once.
 *
 * Dual-band decoding splits the input with a 4th-order Linkwitz-Riley
 * crossover. Its low-pass and high-pass outputs sum to an all-pass, so with
 * HP = AP - LP, only three biquads per input channel are needed:
 *
 *     out = D_hi * AP(x) + (D_lo - D_hi) * LP(x)
 *
 * The all-pass phase shift is the same on every channel, so it leaves the
 * decoded image alone. Single-band decoding skips the filters and is one
 * matrix product.
 *
 * As in the other decoders, a new set of matrices is picked up at a block
 * boundary and the gains are interpolated over `blockSize` samples.
 */
struct AmbiSpeakerDecoder {
	enum {
		MIN_ORDER = 1,
		MAX_ORDER = 3,
		MAX_NUM_SH = (MAX_ORDER + 1) * (MAX_ORDER + 1),
		MAX_CHANNELS = 16,
	};

	/** The loudspeaker decoders of SAF's #LOUDSPEAKER_AMBI_DECODER_METHODS
	 * that suit an arbitrary layout. */
	enum Method {
		METHOD_ALLRAD,
		METHOD_EPAD,
		METHOD_SAD,
		NUM_METHODS
	};

	/** Everything the decoding matrices depend on. */
	struct Options {
		float sampleRate = 48000.f;
		/** Channels 0 .. numChannels-1 are in use; the rest stay silent. */
		int numChannels = 0;
		/** Channels carrying a speaker. A used channel that is not a speaker
		 * is an LFE channel, which the decoder leaves silent for its owner to
		 * fill. */
		bool isSpeaker[MAX_CHANNELS] = {};
		float azimuth[MAX_CHANNELS] = {};
		float elevation[MAX_CHANNELS] = {};

		Method method = METHOD_ALLRAD;
		/** max-rE weighting, per band. Single-band decoding uses the high one,
		 * that being the matrix it decodes the whole signal with. */
		bool lowMaxRE = true;
		bool highMaxRE = true;
		bool dualBand = false;
		float crossover = 800.f;
		int lowOrder = MAX_ORDER;
		int highOrder = MAX_ORDER;

		bool operator==(const Options& other) const;
		bool operator!=(const Options& other) const { return !(*this == other); }
	};

	explicit AmbiSpeakerDecoder(int blockSize = 32);

	/** Computes the decoding matrices of every input order. Slow and
	 * allocating: call it from a worker thread, never from the audio one. */
	void build(const Options& options);

	bool isReady() const { return activeSet.load(std::memory_order_acquire) >= 0; }

	/** Clears the gain interpolation and the crossover filters. */
	void reset();

	/** Sets the Ambisonic order of the input signal (clamped to 1..MAX_ORDER). */
	void setOrder(int order);
	int getOrder() const { return order; }
	/** Number of input channels, i.e., (order + 1)^2. */
	int getNumSH() const { return numSH; }

	/** Highest order the speaker layout supports, as the matrices in use were
	 * designed for; 0 while there is no speaker to decode to. */
	int getLayoutOrder() const { return layoutOrder; }

	/** Decodes one sample.
	 *
	 * @param in  getNumSH() input samples
	 * @param out MAX_CHANNELS output samples, one per channel
	 */
	void process(const float* in, float* out);

private:
	/** Direct-form I biquad, the crossover being built of nothing else. */
	struct Biquad {
		float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
		float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;

		void reset() { x1 = x2 = y1 = y2 = 0.f; }

		float process(float x) {
			float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
			x2 = x1;
			x1 = x;
			y2 = y1;
			y1 = y;
			return y;
		}
	};

	/** The decoding matrices of every input order, as published by build(). */
	struct MatrixSet {
		uint32_t generation = 0;
		int numChannels = 0;
		int layoutOrder = 0;
		bool crossover = false;
		float crossoverFreq = 800.f;
		float sampleRate = 48000.f;
		/** Matrix of input order n + 1, FLAT: MAX_CHANNELS x MAX_NUM_SH.
		 * `high` decodes the whole signal when there is no crossover, and
		 * `low` is what the low-passed signal adds on top of it. */
		std::vector<float> high[MAX_ORDER];
		std::vector<float> low[MAX_ORDER];
	};

	/** Double buffer: build() always writes the set process() is not using,
	 * then publishes it with `activeSet` (-1 until the first set is ready). */
	MatrixSet sets[2];
	std::atomic<int> activeSet{-1};
	uint32_t generation = 0;

	int blockSize;
	int order = 1;
	int numSH = 4;
	int numChannels = 0;
	int layoutOrder = 0;

	uint32_t usedGeneration = 0;
	int samplesUntilUpdate = 0;
	bool jumpToTarget = true;
	/** Set while the gains or the all-pass mix are still moving. */
	bool ramping = false;

	/** Decoding gains, FLAT: MAX_CHANNELS x MAX_NUM_SH */
	std::vector<float> highGains, highTarget, highSteps;
	std::vector<float> lowGains, lowTarget, lowSteps;

	/** Crossover, per input channel: two low-pass sections, and the all-pass
	 * their output sums to with the matching high-pass. */
	Biquad lowPass[2][MAX_NUM_SH];
	Biquad allPass[MAX_NUM_SH];
	bool filtersActive = false;
	float crossoverFreq = 0.f;
	float filterSampleRate = 0.f;
	/** Crossfades the high-band input between the raw signal (0) and the
	 * all-pass (1), so that turning the crossover on or off does not click. */
	float apMix = 0.f, apMixTarget = 0.f, apMixStep = 0.f;

	void updateGains();
	void setFilters(float sampleRate, float frequency);
};


} // namespace sp4t
