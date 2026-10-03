#pragma once

#include "SphericalCoordinates.hpp"

#include <vector>


namespace sp4t {


/** Rotates an Ambisonic sound field by yaw, pitch and roll.
 *
 * Input and output signals follow the ambiX convention (ACN channel ordering,
 * SN3D normalization). Adapted from SAF's `rotator` example for sample-by-sample
 * processing: the spherical harmonic rotation matrix is recomputed once every
 * `blockSize` samples and its entries are linearly interpolated in between,
 * which avoids zipper noise when the angles are modulated. `rotator` crossfades
 * the output of the previous and the new matrix over a frame instead, which
 * amounts to the same thing for rotations that change slowly.
 *
 * The matrix only mixes channels of the same order, so the order-dependent
 * scaling between N3D and SN3D cancels out and the same matrix applies to both.
 *
 * No memory is allocated after construction.
 */
struct AmbiRotator {
	enum {
		MIN_ORDER = 1,
		MAX_ORDER = 3,
		MAX_NUM_SH = (MAX_ORDER + 1) * (MAX_ORDER + 1),
	};

	explicit AmbiRotator(int blockSize = 32);

	/** Clears the matrix interpolation state. */
	void reset();

	/** Sets the Ambisonic order of the input signal (clamped to 1..MAX_ORDER). */
	void setOrder(int order);
	int getOrder() const { return order; }
	/** Number of input and output channels, i.e., (order + 1)^2. */
	int getNumSH() const { return numSH; }

	/** Sets the rotation angles, in degrees.
	 *
	 * The sound field is rotated by Rz(yaw) Ry(pitch) Rx(roll), i.e. roll
	 * around the x (front) axis first, then pitch around the y (left) axis,
	 * then yaw around the z (up) axis, all right-handed in the ambiX frame. A
	 * positive angle therefore turns the field anticlockwise seen from the
	 * positive end of its axis: yaw takes a source in front toward the left
	 * (azimuth +90 for a yaw of +90), pitch takes it downward, and roll takes
	 * the left side upward.
	 *
	 * Yaw and roll are wrapped to [-180, 180] and pitch is clamped to
	 * [-90, 90]. Wrapping is seamless: an angle and the same angle plus a full
	 * turn describe the same rotation.
	 */
	void setAngles(float yaw, float pitch, float roll);

	/** True when the rotation in force is exactly the identity, i.e. the angles
	 * are all zero and the interpolation has reached them.
	 *
	 * The caller can then copy its input to its output and skip process()
	 * altogether: the samples are bit-identical, as the matrix is the identity.
	 * It stays false while a glide toward zero is still running, so that
	 * returning the angles to zero does not cut the glide short. */
	bool isIdentity() const {
		return !dirty && glideRemaining == 0 && yaw == 0.f && pitch == 0.f && roll == 0.f;
	}

	/** Where the x, y and z axes of the sound field end up under the rotation
	 * given in degrees, in the order x, y, z. For the screen, which has to
	 * agree with what the audio path does. */
	static void rotatedAxes(float yaw, float pitch, float roll, Vector3 axes[3]);

	/** Rotates one sample.
	 *
	 * @param in  getNumSH() input samples
	 * @param out getNumSH() output samples
	 */
	void process(const float* in, float* out);

private:
	void updateMatrix();

	int blockSize;
	int order = 1;
	int numSH = 4;
	float yaw = 0.f;
	float pitch = 0.f;
	float roll = 0.f;

	/** Spherical harmonic rotation matrices, FLAT: MAX_NUM_SH x MAX_NUM_SH */
	std::vector<float> targetMatrix;
	std::vector<float> matrix;
	std::vector<float> matrixSteps;
	int samplesUntilUpdate = 0;
	/** Samples of interpolation left before the matrix equals its target. */
	int glideRemaining = 0;
	bool dirty = true;
	bool jumpToTarget = true;
};


} // namespace sp4t
