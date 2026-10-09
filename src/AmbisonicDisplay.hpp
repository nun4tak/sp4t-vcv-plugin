#pragma once

#include "plugin.hpp"
#include "SphericalCoordinates.hpp"

#include <string>
#include <vector>


namespace sp4t {


/** Screen showing the directions of point sources on the sphere, with one of
 * several view modes (projections).
 *
 * The widget is independent of any module: its owner sets the view mode and
 * the sources to show, e.g., from ModuleWidget::step() (UI thread).
 *
 * The screen can have any size. Its content is designed for a screen of
 * REFERENCE_SIZE and is scaled uniformly (fonts, strokes, margins) by the
 * smallest ratio between the actual and reference width or height, while the
 * plots fill the actual aspect ratio. Scaling is a single transform applied at
 * draw time, so it has no performance cost.
 */
struct AmbisonicDisplay : TransparentWidget {
	enum ViewMode {
		VIEW_ORTHOGRAPHIC_PAIR,
		VIEW_HAMMER,
		VIEW_CARTESIAN,
		VIEW_SPHERE_3D,
		VIEW_XY_Z_PROJECTIONS,
		VIEW_MODES_LEN
	};

	/** Point source, drawn as a short label (e.g., one letter) centered on its
	 * projected direction. */
	struct Source {
		Direction direction;
		std::string label;
		NVGcolor color;
	};

	/** 45.8 x 27.3 mm, in px. */
	static const Vec REFERENCE_SIZE;

	/** Menu labels of the view modes, in ViewMode order. */
	static std::vector<std::string> getViewModeLabels();

	int viewMode = VIEW_XY_Z_PROJECTIONS;
	/** Drawn in order (the last one on top). */
	std::vector<Source> sources;
	/** Orthographic pair only: sources on the front/rear boundary are drawn on
	 * the hemisphere of this direction, or on the front hemisphere if this
	 * direction is on the boundary too. */
	Direction boundaryReference = {0.f, 0.f};

	void drawLayer(const DrawArgs& args, int layer) override;

private:
	struct ProjectedSource;

	/** Each view is drawn in reference units, within `size`. */
	void drawOrthographicPair(NVGcontext* vg, Vec size);
	void drawHemisphereGrid(NVGcontext* vg, Vec c, float r);
	void drawHammer(NVGcontext* vg, Vec size);
	void drawCartesian(NVGcontext* vg, Vec size);
	void drawSphere3D(NVGcontext* vg, Vec size);
	void drawXYandZProjections(NVGcontext* vg, Vec size);

	template <typename Project> void drawSources(NVGcontext* vg, const Project& project);
	void drawSourceLabel(NVGcontext* vg, ProjectedSource p, const Source& source);
};


} // namespace sp4t
