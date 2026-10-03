#pragma once

#include "plugin.hpp"
#include "SphericalCoordinates.hpp"


namespace sp4t {


/** Screen showing the rotation applied to an Ambisonic sound field: the x, y, z
 * axes of the field in grey, and where the rotation takes them in orange.
 *
 * Same camera and stroke settings as the 3D sphere view of AmbisonicDisplay,
 * without the sphere graticules, but with shorter axes and larger names and
 * arrowheads: this screen holds six of each and only their directions matter.
 * Both sets of names share one ring, so that with no rotation applied each
 * rotated axis and its name land exactly on their grey counterpart, drawn over
 * them. The origin stays put whatever the rotation.
 *
 * The widget is independent of any module: its owner sets the rotated axes,
 * e.g., from ModuleWidget::step() (UI thread).
 */
struct RotationDisplay : TransparentWidget {
	/** Images of the x, y and z axes, in that order (identity by default). */
	Vector3 axes[3] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};

	void drawLayer(const DrawArgs& args, int layer) override;

private:
	void drawAxes3D(NVGcontext* vg, Vec size);
};


} // namespace sp4t
