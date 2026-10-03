#include "AmbiRotator.hpp"

#include <algorithm>
#include <cmath>

#include "saf.h"


namespace sp4t {


namespace {

/** Wraps an angle to [-180, 180]. */
float wrapAngle(float angle) {
	angle = std::fmod(angle + 180.f, 360.f);
	if (angle < 0.f)
		angle += 360.f;
	return angle - 180.f;
}

/** Rotation of the sound field itself, i.e. Rz(yaw) Ry(pitch) Rx(roll) with
 * right-handed rotations in the ambiX frame.
 *
 * SAF's yawPitchRoll2Rzyx() builds the rotation of the *listener*, which is
 * what `rotator` wants for head tracking: it turns the scene the opposite way,
 * sending a source in front to azimuth -90 for a yaw of +90. Transposing it
 * gives the rotation of the scene, and the transpose is exactly the product
 * above (verified numerically), so a yaw of +90 takes a source in front to
 * azimuth +90, anticlockwise as the azimuth convention of the plugin.
 */
void buildRotation(float yaw, float pitch, float roll, float R[3][3]) {
	float listener[3][3];
	yawPitchRoll2Rzyx(yaw * DEG2RAD, pitch * DEG2RAD, roll * DEG2RAD, 0, listener);
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			R[i][j] = listener[j][i];
}

} // namespace


AmbiRotator::AmbiRotator(int blockSize)
	: blockSize(std::max(blockSize, 1)),
	  targetMatrix(MAX_NUM_SH * MAX_NUM_SH, 0.f),
	  matrix(MAX_NUM_SH * MAX_NUM_SH, 0.f),
	  matrixSteps(MAX_NUM_SH * MAX_NUM_SH, 0.f) {}


void AmbiRotator::reset() {
	dirty = true;
	samplesUntilUpdate = 0;
	glideRemaining = 0;
	jumpToTarget = true;
}


void AmbiRotator::setOrder(int newOrder) {
	newOrder = std::min(std::max(newOrder, (int) MIN_ORDER), (int) MAX_ORDER);
	if (newOrder == order)
		return;
	order = newOrder;
	numSH = (order + 1) * (order + 1);
	dirty = true;
	// The channels the new order adds have no interpolation state to glide
	// from, so the whole matrix takes effect at once.
	jumpToTarget = true;
}


void AmbiRotator::setAngles(float newYaw, float newPitch, float newRoll) {
	newYaw = wrapAngle(newYaw);
	newPitch = std::min(std::max(newPitch, -90.f), 90.f);
	newRoll = wrapAngle(newRoll);
	if (newYaw == yaw && newPitch == pitch && newRoll == roll)
		return;
	yaw = newYaw;
	pitch = newPitch;
	roll = newRoll;
	dirty = true;
}


void AmbiRotator::rotatedAxes(float yaw, float pitch, float roll, Vector3 axes[3]) {
	float R[3][3];
	buildRotation(wrapAngle(yaw), std::min(std::max(pitch, -90.f), 90.f), wrapAngle(roll), R);
	// The image of an axis is the matching column of the rotation matrix.
	for (int j = 0; j < 3; j++)
		axes[j] = {R[0][j], R[1][j], R[2][j]};
}


void AmbiRotator::updateMatrix() {
	bool targetChanged = dirty;
	if (dirty) {
		std::fill(targetMatrix.begin(), targetMatrix.end(), 0.f);
		float R[3][3];
		buildRotation(yaw, pitch, roll, R);
		// getSHrotMtxReal() writes a dense numSH x numSH matrix (and allocates
		// nothing up to order 10).
		float rotation[MAX_NUM_SH * MAX_NUM_SH];
		getSHrotMtxReal(R, rotation, order);
		for (int i = 0; i < numSH; i++)
			for (int j = 0; j < numSH; j++)
				targetMatrix[i * MAX_NUM_SH + j] = rotation[i * numSH + j];
		dirty = false;
	}

	float invBlockSize = 1.f / blockSize;
	for (int k = 0; k < MAX_NUM_SH * MAX_NUM_SH; k++) {
		if (jumpToTarget) {
			matrix[k] = targetMatrix[k];
			matrixSteps[k] = 0.f;
		}
		else {
			matrixSteps[k] = (targetMatrix[k] - matrix[k]) * invBlockSize;
		}
	}
	// Only a new target starts a glide: without this the countdown would be
	// rearmed on every block and never reach zero, so isIdentity() would never
	// come true while the angles sit at zero.
	if (jumpToTarget)
		glideRemaining = 0;
	else if (targetChanged)
		glideRemaining = blockSize;
	jumpToTarget = false;

	samplesUntilUpdate = blockSize;
}


void AmbiRotator::process(const float* in, float* out) {
	if (samplesUntilUpdate <= 0)
		updateMatrix();

	for (int i = 0; i < numSH; i++) {
		float* row = &matrix[i * MAX_NUM_SH];
		const float* steps = &matrixSteps[i * MAX_NUM_SH];
		float sum = 0.f;
		for (int j = 0; j < numSH; j++) {
			sum += row[j] * in[j];
			row[j] += steps[j];
		}
		out[i] = sum;
	}

	samplesUntilUpdate--;
	if (glideRemaining > 0 && --glideRemaining == 0) {
		// Land exactly on the target instead of wherever adding the steps one
		// by one ended up, which drifts by about 1e-7. It keeps a long run of
		// modulation from accumulating error, and makes the matrix exactly the
		// identity when the angles return to zero, so that a caller acting on
		// isIdentity() swaps the multiply for a copy with no step at all.
		matrix = targetMatrix;
		std::fill(matrixSteps.begin(), matrixSteps.end(), 0.f);
	}
}


} // namespace sp4t
