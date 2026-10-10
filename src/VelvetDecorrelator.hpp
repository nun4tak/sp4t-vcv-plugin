#pragma once

#include <vector>


namespace sp4t {


/** Multi-channel signal decorrelator, in the time domain.
 *
 * Each channel is convolved with a velvet noise sequence of its own: impulses
 * of +1 or -1 at random positions, a thousand or so per second, over a few tens
 * of milliseconds. Sparse enough that the convolution is a few dozen additions
 * per sample, random enough that two channels come out incoherent, and dense
 * enough that the magnitude response stays flat -- which is what makes velvet
 * noise the cheap way to decorrelate (Välimäki et al., "Late reverberation
 * synthesis using filtered velvet noise").
 *
 * Unlike the filterbank of Decorrelator it adds no latency at all: the smearing
 * is the effect itself, not a delay to compensate for. It is also the coarser
 * of the two, with no per-band control over how far the channels are taken
 * apart, and it smears transients with no way to duck them.
 *
 * Below the bass split frequency the signal is passed through untouched:
 * smearing the low end is what makes a decorrelator sound loose, and the ear
 * places low frequencies by phase, which this would scramble.
 *
 * The sequences are drawn once, from a fixed seed, so that a patch sounds the
 * same every time it is opened. Only setSampleRate() allocates.
 */
struct VelvetDecorrelator {
	enum {
		/** Channels of a Rack polyphonic cable. */
		MAX_NUM_CHANNELS = 16,
	};

	/** Draws the sequences for `sampleRate`, and does nothing if they are
	 * already drawn for it. Allocates: call it from onSampleRateChange(),
	 * never from the audio thread. */
	void setSampleRate(float sampleRate);
	bool isReady() const { return !taps.empty(); }

	/** Clears the delay lines and the bass split, i.e. drops what the
	 * sequences are still spreading out. */
	void reset();

	/** Sets how many channels the input carries (clamped to
	 * 0..MAX_NUM_CHANNELS). */
	void setNumChannels(int numChannels);
	int getNumChannels() const { return numChannels; }

	/** Sets the mix between the input signals (0) and the decorrelated ones
	 * (1), reached by a short glide, crossfaded at constant power as in
	 * Decorrelator. */
	void setAmount(float amount);

	/** Scales the decorrelated signals up so that they sum to the level the
	 * coherent input channels would. */
	void setCompensateLevel(bool enabled);

	/** Sets the frequency below which the signal is left alone, 0 to decorrelate
	 * the whole of it. */
	void setBassSplit(float frequency);

	/** Latency of the decorrelated signal, in samples: none. */
	int getLatency() const { return 0; }

	/** Decorrelates one sample of every channel.
	 *
	 * @param in  getNumChannels() input samples
	 * @param out getNumChannels() output samples
	 */
	void process(const float* in, float* out);

private:
	void updateMix();

	/** Impulse positions of every channel, as offsets into its delay line,
	 * negated where the impulse is -1. MAX_NUM_CHANNELS x numTaps. */
	std::vector<int> taps;
	/** One delay line per channel, MAX_NUM_CHANNELS x lineLength, written at a
	 * single position. */
	std::vector<float> lines;
	int numTaps = 0;
	int lineLength = 0;
	int lineMask = 0;
	int writePos = 0;

	/** State of the one-pole low-pass that holds the bass back, one per
	 * channel, and its coefficient (0: no split). */
	std::vector<float> bass;
	float bassCoeff = 0.f;
	float bassSplit = 0.f;

	float sampleRate = 0.f;
	int numChannels = 0;
	float amount = 0.f;
	float amountTarget = 0.f;
	/** Constant-power crossfade gains of the dry and the decorrelated signal,
	 * following `amount`. */
	float dryMix = 1.f;
	float wetMix = 0.f;
	/** Flattens the sum of the impulses back to the level of one of them, times
	 * the level compensation when it is on. */
	float tapGain = 1.f;
	bool compensateLevel = false;
};


} // namespace sp4t
