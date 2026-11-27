#pragma once


namespace sp4t {


/** A first-order microphone pattern that has a name of its own, by figure-8
 * weight. The abbreviation is what the screen has room for. */
struct NamedMicPattern {
	float pattern;
	const char* name;
	const char* abbreviation;
};

/** The named patterns, from omni to figure-8. */
extern const NamedMicPattern NAMED_MIC_PATTERNS[];
extern const int NUM_NAMED_MIC_PATTERNS;

/** The named pattern nearest `pattern`.
 *
 * @param exact set to whether `pattern` is that pattern rather than one on the
 *        way to it (optional)
 */
const NamedMicPattern& nearestMicPattern(float pattern, bool* exact = nullptr);


/** Ambisonic decoder for a pair of coincident virtual microphones.
 *
 * Input signals follow the ambiX convention (ACN channel ordering, SN3D
 * normalization). Both microphones lie in the horizontal plane, at the center
 * of the scene, and have the same first-order pattern: `pattern` is the weight
 * of its figure-8 component, from 0 (omni) through 0.5 (cardioid) to 1
 * (figure-8).
 *
 * In X/Y mode the two microphones are turned by `spread` on either side of the
 * front axis. In M/S mode a forward-facing mid microphone is combined with a
 * left-facing figure-8, `spread` setting their balance (0 for mid only, 90 for
 * side only); the pattern then applies to the mid microphone alone.
 *
 * A first-order microphone only picks up the first-order part of the sound
 * field, so the higher-order channels of the input are ignored rather than
 * approximated, and the Z channel is of no use to a pair lying flat.
 *
 * Like AmbiStereoDecoder, the gains are recomputed once every `blockSize`
 * samples and linearly interpolated in between, which avoids zipper noise when
 * the controls are modulated -- the gain of the Y channel changes sign in the
 * middle of the spread range. No memory is allocated after construction.
 */
struct AmbiMicPairDecoder {
	enum {
		/** W, Y, Z and X, i.e. the channels of a first-order signal. */
		NUM_SH = 4,
		NUM_CHANNELS = 2,
	};
	enum Mode {
		MODE_XY,
		MODE_MS,
		MODES_LEN
	};

	explicit AmbiMicPairDecoder(int blockSize = 32);

	/** Clears the gain interpolation state. */
	void reset();

	/** Sets the pair of microphones, Mode (clamped). */
	void setMode(int mode);
	int getMode() const { return mode; }

	/** Sets the weight of the figure-8 component of the pattern, from 0 (omni)
	 * to 1 (figure-8), clamped. */
	void setPattern(float pattern);
	float getPattern() const { return pattern; }

	/** Sets the angle between a microphone and the front axis (X/Y) or the
	 * mid/side balance (M/S), in degrees (clamped to 0..90). */
	void setSpread(float spread);
	float getSpread() const { return spread; }

	/** Keeps the diffuse-field level of the outputs roughly constant whatever
	 * the pattern and the spread (default: disabled). */
	void setLevelCompensation(bool enabled);
	bool getLevelCompensation() const { return levelCompensation; }

	/** Sets the number of channels of the input signal. The output is silent
	 * below four, i.e. unless the input is at least a first-order signal. */
	void setNumInputChannels(int channels);
	int getNumInputChannels() const { return numInputChannels; }

	/** Decodes one sample.
	 *
	 * @param in  NUM_SH input samples
	 * @param out the left and right output samples
	 */
	void process(const float* in, float* out);

private:
	void updateGains();

	int blockSize;
	int mode = MODE_XY;
	float pattern = 0.5f;
	float spread = 45.f;
	bool levelCompensation = false;
	int numInputChannels = 0;

	/** Decoding gains, FLAT: NUM_CHANNELS x NUM_SH */
	float targetGains[NUM_CHANNELS * NUM_SH] = {};
	float gains[NUM_CHANNELS * NUM_SH] = {};
	float gainSteps[NUM_CHANNELS * NUM_SH] = {};
	int samplesUntilUpdate = 0;
	bool dirty = true;
	bool jumpToTarget = true;
};


} // namespace sp4t
