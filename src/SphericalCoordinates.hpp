#pragma once

#include <cmath>


namespace sp4t {


const float DEG2RAD = float(M_PI) / 180.f;
const float RAD2DEG = 180.f / float(M_PI);

/** Vector in the ambiX frame: x front, y left, z up. */
struct Vector3 {
	float x, y, z;
};

/** Azimuth (anticlockwise from the front, toward the left) and elevation
 * (upward), in degrees. */
struct Direction {
	float azimuth, elevation;
};

inline float dot(const Vector3& a, const Vector3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

/** Unit vector pointing to the direction given in degrees. */
inline Vector3 directionToVector(float azimuth, float elevation) {
	float a = azimuth * DEG2RAD;
	float e = elevation * DEG2RAD;
	return {std::cos(e) * std::cos(a), std::cos(e) * std::sin(a), std::sin(e)};
}

/** Returns azimuth in [-180, 180] and elevation in [-90, 90]. */
inline Direction vectorToDirection(const Vector3& v) {
	return {std::atan2(v.y, v.x) * RAD2DEG, std::atan2(v.z, std::hypot(v.x, v.y)) * RAD2DEG};
}


} // namespace sp4t
