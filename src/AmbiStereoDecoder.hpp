#pragma once

#include <vector>


namespace sp4t {


/** Basic Ambisonic decoder for a pair of virtual stereo speakers.
 *
 * Input signals follow the ambiX convention (ACN channel ordering, SN3D
 * normalization). Both speakers lie in the horizontal plane of the front
 * hemisphere, at +/- `spread` degrees from the front (+x) axis.
 *
 * The decoding matrix is a sampling Ambisonic decoder (SAD) with max-rE
 * weighting, i.e., one hyper-cardioid beam (virtual microphone) per speaker,
 * as SAF's getLoudspeakerDecoderMtx() computes it for
 * #LOUDSPEAKER_DECODER_SAD. Only its scaling differs: the matrix is normalized
 * so that a source at a speaker direction decodes at unity gain into that
 * speaker, which keeps the output level bounded whatever the order. The beams
 * getting sharper with the order, a source between the two speakers is then
 * quieter at higher orders.
 *
 * In crude mode, only the W and Y input channels are used (mid-side decoding).
 *
 * Like AmbiEncoder, the gains are recomputed once every `blockSize` samples and
 * linearly interpolated in between, which avoids zipper noise when the spread
 * is modulated. No memory is allocated after construction.
 */
struct AmbiStereoDecoder {
	enum {
		MAX_ORDER = 3,
		MAX_NUM_SH = (MAX_ORDER + 1) * (MAX_ORDER + 1),
		NUM_CHANNELS = 2,
	};

	explicit AmbiStereoDecoder(int blockSize = 32);

	/** Clears the gain interpolation state. */
	void reset();

	/** Sets the Ambisonic order of the input signal (clamped to 1..MAX_ORDER). */
	void setOrder(int order);
	int getOrder() const { return order; }
	/** Number of input channels, i.e., (order + 1)^2. */
	int getNumSH() const { return numSH; }

	/** Sets the angle between a speaker and the front axis, in degrees
	 * (clamped to 0..90). */
	void setSpread(float spread);
	float getSpread() const { return spread; }

	/** Decodes from the W and Y input channels only (default: disabled). */
	void setCrudeMode(bool enabled);
	bool getCrudeMode() const { return crudeMode; }

	/** Decodes one sample.
	 *
	 * @param in  getNumSH() input samples
	 * @param out the left and right output samples
	 */
	void process(const float* in, float* out);

private:
	void updateGains();

	int blockSize;
	int order = 1;
	int numSH = 4;
	float spread = 45.f;
	bool crudeMode = false;

	/** max_rE weights of every order, FLAT: MAX_ORDER x MAX_NUM_SH */
	std::vector<float> maxREWeights;
	/** N3D to SN3D conversion factor of every channel, MAX_NUM_SH */
	std::vector<float> sn3dScale;

	/** Decoding gains, FLAT: NUM_CHANNELS x MAX_NUM_SH */
	std::vector<float> targetGains;
	std::vector<float> gains;
	std::vector<float> gainSteps;
    /** By default number of input Ambisonic channels (order)
     *  but may be limited to 2 in crude mode */
	int numGains = 4;
	int samplesUntilUpdate = 0;
	bool dirty = true;
	bool jumpToTarget = true;
};


} // namespace sp4t
