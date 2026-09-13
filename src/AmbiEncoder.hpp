#pragma once

#include <vector>


namespace sp4t {


/** Basic Ambisonic encoder for an arbitrary number of point sources.
 *
 * Output signals follow the ambiX convention (ACN channel ordering, SN3D
 * normalization). Adapted from SAF's `ambi_enc` example for sample-by-sample
 * processing: the encoding gains are recomputed (only for sources whose
 * direction changed) once every `blockSize` samples and linearly interpolated
 * in between, which avoids zipper noise when directions are modulated.
 *
 * No memory is allocated after construction.
 */
struct AmbiEncoder {
	enum {
		MAX_ORDER = 3,
		MAX_NUM_SH = (MAX_ORDER + 1) * (MAX_ORDER + 1),
	};

	explicit AmbiEncoder(int maxNumSources, int blockSize = 32);

	/** Clears the gain interpolation state. */
	void reset();

	/** Sets the Ambisonic order (clamped to 1..MAX_ORDER). */
	void setOrder(int order);
	int getOrder() const { return order; }
	/** Number of output channels, i.e., (order + 1)^2. */
	int getNumSH() const { return numSH; }

	/** Sets the number of active sources (clamped to 1..maxNumSources). */
	void setNumSources(int numSources);
	int getNumSources() const { return numSources; }

	/** Scales the output by 1/sqrt(numSources) to preserve the total power (default: enabled). */
	void setPostScaling(bool enabled);

	/** Sets the direction of a source, in degrees.
	 *
	 * Azimuth is anticlockwise from the front (+x) toward the left (+y) and is
	 * wrapped to [-180, 180]. Elevation is upward (+z) and clamped to [-90, 90].
	 */
	void setSourceDirection(int index, float azimuth, float elevation);

	/** Encodes one sample.
	 *
	 * @param in  getNumSources() input samples
	 * @param out getNumSH() output samples
	 */
	void process(const float* in, float* out);

private:
	void updateGains();

	int maxNumSources;
	int blockSize;
	int order = 1;
	int numSH = 4;
	int numSources = 1;
	bool postScaling = true;
	float scale = 1.f;

	/** Source [azimuth, elevation] in degrees, FLAT: maxNumSources x 2 */
	std::vector<float> directions;
	std::vector<char> dirty;
	/** SH gains, FLAT: MAX_NUM_SH x maxNumSources */
	std::vector<float> targetGains;
	std::vector<float> gains;
	std::vector<float> gainSteps;
	int samplesUntilUpdate = 0;
	bool jumpToTarget = true;
};


} // namespace sp4t
