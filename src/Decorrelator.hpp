#pragma once

#include <vector>


namespace sp4t {


/** Multi-channel signal decorrelator.
 *
 * Adapted from SAF's `decorrelator` example: the channels go through an afSTFT
 * filterbank, where randomised time-frequency delays and cascaded lattice
 * all-pass filters make each of them incoherent with the others, and the result
 * is crossfaded with the original signal at constant power.
 *
 * The example processes frames of samples; here the samples are buffered into
 * one hop and the hop is transformed as a single time slot, which adds a hop to
 * the delay of the filterbank itself. The dry signal is mixed in on the output
 * side, through a delay line of the same latency rather than through the
 * filterbank, so that it comes out bit-identical and the mix can follow the
 * amount sample by sample instead of hop by hop.
 *
 * The filterbank and the all-pass filters are built for MAX_NUM_CHANNELS, and
 * the channels the input does not carry are fed silence: their share of the
 * work is wasted, but changing the channel count of the filterbank reallocates,
 * which the audio thread must not do. Only setSampleRate() allocates.
 */
struct Decorrelator {
	enum {
		/** Channels of a Rack polyphonic cable. */
		MAX_NUM_CHANNELS = 16,
		/** afSTFT hop size, as in the SAF example. The frequency resolution,
		 * the static delays and the latency all scale with it. */
		HOP_SIZE = 128,
		/** Bands of the filterbank: one per hop sample, plus the 5 the hybrid
		 * filtering splits the lowest ones into. */
		NUM_BANDS = HOP_SIZE + 5,
	};

	Decorrelator() = default;
	~Decorrelator();
	Decorrelator(const Decorrelator&) = delete;
	Decorrelator& operator=(const Decorrelator&) = delete;

	/** Builds the filterbank and the all-pass filters for `sampleRate`, and
	 * does nothing if they are already built for it. Allocates: call it from
	 * onSampleRateChange(), never from the audio thread. */
	void setSampleRate(float sampleRate);
	bool isReady() const { return hSTFT != nullptr; }

	/** Clears the filterbank, the all-pass and the dry delay line, i.e. drops
	 * the samples still inside the latency of the decorrelator. */
	void reset();

	/** Sets how many channels the input carries (clamped to
	 * 0..MAX_NUM_CHANNELS). */
	void setNumChannels(int numChannels);
	int getNumChannels() const { return numChannels; }

	/** Sets the mix between the input signals (0) and the decorrelated ones
	 * (1), reached by a short glide. The two are crossfaded at constant power,
	 * which keeps the level flat across the travel since they are largely
	 * incoherent. */
	void setAmount(float amount);

	/** Sends the transients of the input straight to the output instead of
	 * through the decorrelator, which smears them badly. The all-pass filters
	 * then only see what SAF's transient ducker leaves of the signal, and the
	 * transients it took out are added back untouched. */
	void setBypassTransients(bool enabled);

	/** Scales the decorrelated signals up so that they sum to the level the
	 * coherent input channels would, as the channels no longer add up in
	 * phase. */
	void setCompensateLevel(bool enabled);

	/** Latency of the decorrelated signal, in samples: the hop that is
	 * buffered plus the delay of the filterbank. */
	int getLatency() const { return latency; }

	/** Decorrelates one sample of every channel.
	 *
	 * @param in  getNumChannels() input samples
	 * @param out getNumChannels() output samples
	 */
	void process(const float* in, float* out);

private:
	void destroy();
	void updateMix();
	void runHop();

	/** afSTFT handle. */
	void* hSTFT = nullptr;
	/** Lattice all-pass decorrelator handle. */
	void* hDecor = nullptr;
	/** Transient ducker handle. */
	void* hDucker = nullptr;

	float sampleRate = 0.f;
	int latency = 0;
	int numChannels = 0;
	float amount = 0.f;
	float amountTarget = 0.f;
	/** Constant-power crossfade gains of the dry and the decorrelated signal,
	 * following `amount`. */
	float dryMix = 1.f;
	float wetMix = 0.f;
	bool compensateLevel = false;
	/** Gain of the decorrelated signals, 1 unless the level is compensated. */
	float compGain = 1.f;
	bool bypassTransients = false;

	/** Time-domain hops, MAX_NUM_CHANNELS x HOP_SIZE. */
	float** inputTD = nullptr;
	float** outputTD = nullptr;
	/** Time-frequency hops, NUM_BANDS x MAX_NUM_CHANNELS x 1 time slot. The
	 * residual and transient ones are only used while the transients are
	 * bypassed. */
	void*** inputTF = nullptr;
	void*** outputTF = nullptr;
	void*** residualTF = nullptr;
	void*** transientTF = nullptr;
	/** Centre frequency of each band, in Hz. */
	std::vector<float> freqVector;

	/** Position inside the hop being buffered. */
	int hopPos = 0;
	/** The input samples of every channel, delayed by the latency of the
	 * decorrelated signal so that the dry mix stays aligned with it.
	 * MAX_NUM_CHANNELS x latency, with a single write position. */
	std::vector<float> dryLines;
	int dryPos = 0;
};


} // namespace sp4t
