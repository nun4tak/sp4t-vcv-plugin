#include "AmbisonicDisplay.hpp"

#include "ScreenStyle.hpp"

#include <algorithm>
#include <functional>


namespace sp4t {


const Vec AmbisonicDisplay::REFERENCE_SIZE = mm2px(Vec(45.8f, 27.3f));


namespace {

/** Size of every label of the screen, axis names and source labels alike, in
 * all the view modes. The graticule labels of the cartesian view keep their own
 * smaller size.
 *
 * It is the size the axis names of the Rotate screen come out at: both screens
 * are drawn in the same reference units and scaled to fit their own box, and
 * both are limited by their width, so the 16 units of the Rotate names render
 * here in the ratio of the two screen widths: 16 * 37.294 / 47.344 mm.
 */
const float LABEL_SIZE = 12.6f;

/** Metrics of one glyph at that size, as fractions of it: cap height and the
 * advance of the source font. */
const float LABEL_HALF_HEIGHT = 0.35f * LABEL_SIZE;
const float LABEL_SLOT = 0.6f * LABEL_SIZE;

} // namespace


std::vector<std::string> AmbisonicDisplay::getViewModeLabels() {
	return {"Orthographic (front / rear)", "Hammer", "Cartesian", "3D sphere",
	        "XY/Z projections"};
}


/** Projected position of a source; `alpha` below 1 fades hidden sources. */
struct AmbisonicDisplay::ProjectedSource {
	Vec pos;
	float alpha;
	ProjectedSource(Vec pos, float alpha = 1.f) : pos(pos), alpha(alpha) {}
};


void AmbisonicDisplay::drawLayer(const DrawArgs& args, int layer) {
	float scale = std::min(box.size.x / REFERENCE_SIZE.x, box.size.y / REFERENCE_SIZE.y);
	if (layer == 1 && scale > 0) {
		nvgSave(args.vg);
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
		nvgScale(args.vg, scale, scale);
		Vec size = box.size.div(scale);
		switch (viewMode) {
			case VIEW_HAMMER: drawHammer(args.vg, size); break;
			case VIEW_CARTESIAN: drawCartesian(args.vg, size); break;
			case VIEW_SPHERE_3D: drawSphere3D(args.vg, size); break;
			case VIEW_XY_Z_PROJECTIONS: drawXYandZProjections(args.vg, size); break;
			default: drawOrthographicPair(args.vg, size); break;
		}
		nvgRestore(args.vg);
	}
	TransparentWidget::drawLayer(args, layer);
}


/** Front (+x) and rear (-x) hemispheres side by side, both as seen by the
 * listener: the front disk has the left side on the left and the rear disk
 * continues from the right edge of the front disk. */
void AmbisonicDisplay::drawOrthographicPair(NVGcontext* vg, Vec size) {
	const float pad = 3.f;
	// The row the FRONT / REAR names sit in, above the disks.
	const float labelHeight = 1.1f * LABEL_SIZE;
	float r = std::min((size.x - 3 * pad) / 4, (size.y - labelHeight - 2 * pad) / 2);
	float cy = (size.y + labelHeight) / 2;
	Vec front(size.x / 2 - pad / 2 - r, cy);
	Vec rear(size.x / 2 + pad / 2 + r, cy);

	drawHemisphereGrid(vg, front, r);
	drawHemisphereGrid(vg, rear, r);
	drawLabel(vg, Vec(front.x, cy - r - 2), "FRONT", NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM, LABEL_SIZE);
	drawLabel(vg, Vec(rear.x, cy - r - 2), "REAR", NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM, LABEL_SIZE);

	// Sources on the front/rear boundary (azimuth or elevation at +/-90
	// degrees, up to rounding errors) are shown on the same disk as the
	// reference direction, or on the front disk if the reference is on the
	// boundary too.
	const float boundary = 1e-4f;
	bool boundaryOnRear = directionToVector(boundaryReference.azimuth, boundaryReference.elevation).x < -boundary;

	drawSources(vg, [&](const Direction& d) -> Vec {
		Vector3 v = directionToVector(d.azimuth, d.elevation);
		bool onFront = v.x > boundary || (v.x >= -boundary && !boundaryOnRear);
		if (onFront)
			return Vec(front.x - v.y * r, front.y - v.z * r);
		return Vec(rear.x + v.y * r, rear.y - v.z * r);
	});
}


void AmbisonicDisplay::drawHemisphereGrid(NVGcontext* vg, Vec c, float r) {
	nvgBeginPath(vg);
	// Parallels every 45 degrees
	for (int elevation = -45; elevation <= 45; elevation += 45) {
		float y = c.y - std::sin(elevation * DEG2RAD) * r;
		float halfWidth = std::cos(elevation * DEG2RAD) * r;
		nvgMoveTo(vg, c.x - halfWidth, y);
		nvgLineTo(vg, c.x + halfWidth, y);
	}
	// Meridians every 45 degrees
	nvgMoveTo(vg, c.x, c.y - r);
	nvgLineTo(vg, c.x, c.y + r);
	for (int azimuth = 45; azimuth < 90; azimuth += 45)
		nvgEllipse(vg, c.x, c.y, std::sin(azimuth * DEG2RAD) * r, r);
	nvgStrokeColor(vg, SCREEN_GRID);
	nvgStrokeWidth(vg, 0.5f);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgCircle(vg, c.x, c.y, r);
	nvgStrokeColor(vg, SCREEN_OUTLINE);
	nvgStrokeWidth(vg, 0.8f);
	nvgStroke(vg);
}


/** Hammer equal-area projection of the whole sphere, azimuth 0 at the center
 * and increasing toward the left. */
void AmbisonicDisplay::drawHammer(NVGcontext* vg, Vec size) {
	const float pad = 3.f;
	float a = std::min((size.x - 2 * pad) / 2, size.y - 2 * pad);
	float b = a / 2;
	Vec c = size.div(2);

	auto project = [=](float azimuth, float elevation) -> Vec {
		float halfLambda = azimuth * DEG2RAD / 2;
		float phi = elevation * DEG2RAD;
		float denom = std::sqrt(1 + std::cos(phi) * std::cos(halfLambda));
		return Vec(c.x - a * std::cos(phi) * std::sin(halfLambda) / denom,
		           c.y - b * std::sin(phi) / denom);
	};

	nvgBeginPath(vg);
	// Meridians and parallels every 45 degrees
	for (int azimuth = -135; azimuth <= 135; azimuth += 45) {
		for (int k = 0; k <= 18; k++) {
			Vec p = project(azimuth, -90 + 10 * k);
			if (k == 0)
				nvgMoveTo(vg, p.x, p.y);
			else
				nvgLineTo(vg, p.x, p.y);
		}
	}
	for (int elevation = -45; elevation <= 45; elevation += 45) {
		for (int k = 0; k <= 36; k++) {
			Vec p = project(-180 + 10 * k, elevation);
			if (k == 0)
				nvgMoveTo(vg, p.x, p.y);
			else
				nvgLineTo(vg, p.x, p.y);
		}
	}
	nvgStrokeColor(vg, SCREEN_GRID);
	nvgStrokeWidth(vg, 0.5f);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgEllipse(vg, c.x, c.y, a, b);
	nvgStrokeColor(vg, SCREEN_OUTLINE);
	nvgStrokeWidth(vg, 0.8f);
	nvgStroke(vg);

	drawSources(vg, [&](const Direction& d) {
		return project(d.azimuth, d.elevation);
	});
}


/** Equirectangular (cartesian) projection over the whole screen, with the
 * origin at the center: azimuth on the horizontal (Y) axis, increasing toward
 * the left and +/-180 degrees at the edges, and elevation on the vertical (Z)
 * axis, +/-90 degrees at the top/bottom edges. */
void AmbisonicDisplay::drawCartesian(NVGcontext* vg, Vec size) {
	Vec c = size.div(2);
	auto project = [=](float azimuth, float elevation) {
		return Vec(c.x - azimuth / 360 * size.x, c.y - elevation / 180 * size.y);
	};

	// Graticules every 45 degrees (the edges are the screen borders)
	nvgBeginPath(vg);
	for (int azimuth = -135; azimuth <= 135; azimuth += 45) {
		float x = project(azimuth, 0).x;
		nvgMoveTo(vg, x, 0);
		nvgLineTo(vg, x, size.y);
	}
	for (int elevation = -45; elevation <= 45; elevation += 90) {
		float y = project(0, elevation).y;
		nvgMoveTo(vg, 0, y);
		nvgLineTo(vg, size.x, y);
	}
	nvgStrokeColor(vg, SCREEN_GRID);
	nvgStrokeWidth(vg, 0.5f);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, 0, c.y);
	nvgLineTo(vg, size.x, c.y);
	nvgMoveTo(vg, c.x, 0);
	nvgLineTo(vg, c.x, size.y);
	nvgStrokeColor(vg, SCREEN_OUTLINE);
	nvgStrokeWidth(vg, 0.8f);
	nvgStroke(vg);

	// Labels sit next to their graticule, on the side of the origin. Azimuth
	// labels alternate between below and above the horizontal axis so they
	// don't crowd each other; elevation labels are right of the vertical axis
	// (the origin is labeled once, as an azimuth).
	const float offset = 1.5f;
	for (int azimuth = -180; azimuth <= 180; azimuth += 45) {
		float x = project(azimuth, 0).x;
		bool below = azimuth % 90 != 0;
		float y = below ? c.y + offset : c.y - offset;
		int vAlign = below ? NVG_ALIGN_TOP : NVG_ALIGN_BOTTOM;
		if (azimuth > 0)
			drawGridLabel(vg, Vec(x + offset, y), string::f("%d", azimuth), NVG_ALIGN_LEFT | vAlign);
		else
			drawGridLabel(vg, Vec(x - offset, y), string::f("%d", azimuth), NVG_ALIGN_RIGHT | vAlign);
	}
	for (int elevation = -90; elevation <= 90; elevation += 45) {
		float y = project(0, elevation).y;
		if (elevation > 0)
			drawGridLabel(vg, Vec(c.x + offset, y + offset), string::f("%d", elevation), NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		else if (elevation < 0)
			drawGridLabel(vg, Vec(c.x + offset, y - offset), string::f("%d", elevation), NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
	}

	// Keep the labels of sources at the edges (e.g., rear or poles) whole.
	const Vec margin(0.4f * LABEL_SIZE, 0.5f * LABEL_SIZE);
	drawSources(vg, [&](const Direction& d) {
		return project(d.azimuth, d.elevation).clamp(math::Rect(margin, size.minus(margin.mult(2))));
	});
}


/** 3D view of the sphere and its X, Y, Z axes, seen from the front-left and
 * above, with X (front) pointing toward the viewer at the lower left, Y (left)
 * to the right and Z up. Graticules and sources on the hidden side of the
 * sphere are faded.
 *
 * The projection is orthographic. */
void AmbisonicDisplay::drawSphere3D(NVGcontext* vg, Vec size) {
	// Camera direction, from the center toward the eye.
	const float camAzimuth = 30 * DEG2RAD;
	const float camElevation = 30 * DEG2RAD;
	const Vector3 eye = directionToVector(camAzimuth * RAD2DEG, camElevation * RAD2DEG);
	const Vector3 right = {-std::sin(camAzimuth), std::cos(camAzimuth), 0.f};
	const Vector3 up = {-std::sin(camElevation) * std::cos(camAzimuth),
	                    -std::sin(camElevation) * std::sin(camAzimuth), std::cos(camElevation)};

	Vec c = size.div(2);
	// Radius of the sphere outline on the screen (not drawn, used for the
	// layout), leaving room for the axis names.
	float outlineRadius = std::min(c.y - 1.1f * LABEL_SIZE, c.x - 0.8f * LABEL_SIZE);

	// Axis names, just outside the outline in the screen direction of the
	// positive axis ends.
	const Vector3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
	const char* axisNames[3] = {"X", "Y", "Z"};
	const float labelDistance = outlineRadius + 5;
	const float labelHalfHeight = LABEL_HALF_HEIGHT;
	auto axisDirection = [=](const Vector3& axis) {
		return Vec(dot(axis, right), -dot(axis, up)).normalize();
	};

	// Centers the plot vertically on the screen, including the axis names
	// (the Z name sticks out above the sphere).
	float top = -outlineRadius;
	float bottom = outlineRadius;
	for (const Vector3& axis : axes) {
		float y = axisDirection(axis).y * labelDistance;
		top = std::min(top, y - labelHalfHeight);
		bottom = std::max(bottom, y + labelHalfHeight);
	}
	c.y -= (top + bottom) / 2;

	auto project = [=](const Vector3& v) {
		return Vec(c.x + outlineRadius * dot(v, right), c.y - outlineRadius * dot(v, up));
	};
	auto isVisible = [=](const Vector3& v) {
		return dot(v, eye) > 0;
	};

	// Graticules: parallels and meridians every 45 degrees, including the
	// great circles in the XY, XZ and YZ planes.
	using Curve = std::function<Vector3(float)>;
	auto parallel = [](float elevation) -> Curve {
		float e = elevation * DEG2RAD;
		return [=](float t) {
			return Vector3{std::cos(e) * std::cos(t), std::cos(e) * std::sin(t), std::sin(e)};
		};
	};
	auto meridian = [](float azimuth) -> Curve {
		float a = azimuth * DEG2RAD;
		return [=](float t) {
			return Vector3{std::cos(t) * std::cos(a), std::cos(t) * std::sin(a), std::sin(t)};
		};
	};
	const std::vector<Curve> graticules = {
		parallel(-45), parallel(0), parallel(45),
		meridian(0), meridian(45), meridian(90), meridian(135),
	};

	// Strokes the curves as polylines, keeping only the segments on the hidden
	// or the visible side of the sphere.
	const int numSegments = 72;
	auto strokeCurves = [&](const std::vector<Curve>& curves, bool visible, NVGcolor color, float width) {
		nvgBeginPath(vg);
		for (const Curve& curve : curves) {
			Vector3 prev = curve(0);
			bool drawing = false;
			for (int k = 1; k <= numSegments; k++) {
				Vector3 v = curve(2 * float(M_PI) * k / numSegments);
				Vector3 mid = {prev.x + v.x, prev.y + v.y, prev.z + v.z};
				float norm = std::sqrt(dot(mid, mid));
				Vector3 midUnit = {mid.x / norm, mid.y / norm, mid.z / norm};
				if (isVisible(midUnit) == visible) {
					if (!drawing) {
						Vec p = project(prev);
						nvgMoveTo(vg, p.x, p.y);
					}
					Vec p = project(v);
					nvgLineTo(vg, p.x, p.y);
				}
				drawing = isVisible(midUnit) == visible;
				prev = v;
			}
		}
		nvgStrokeColor(vg, color);
		nvgStrokeWidth(vg, width);
		nvgStroke(vg);
	};
	strokeCurves(graticules, false, SCREEN_GRID_HIDDEN, 0.5f);
	strokeCurves(graticules, true, SCREEN_GRID, 0.5f);

	// Axes as diameters of the sphere, with an arrow at the positive end whose
	// tip touches the sphere surface.
	const float arrowLength = 4.f;
	const float arrowHalfWidth = 1.8f;
	for (const Vector3& axis : axes) {
		Vec tip = project(axis);
		Vec u = tip.minus(c).normalize();
		Vec n(-u.y, u.x);
		Vec base = tip.minus(u.mult(arrowLength));

		nvgBeginPath(vg);
		Vec p0 = project(Vector3{-axis.x, -axis.y, -axis.z});
		nvgMoveTo(vg, p0.x, p0.y);
		nvgLineTo(vg, base.x, base.y);
		nvgStrokeColor(vg, SCREEN_OUTLINE);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, tip.x, tip.y);
		nvgLineTo(vg, base.x + n.x * arrowHalfWidth, base.y + n.y * arrowHalfWidth);
		nvgLineTo(vg, base.x - n.x * arrowHalfWidth, base.y - n.y * arrowHalfWidth);
		nvgClosePath(vg);
		nvgFillColor(vg, SCREEN_OUTLINE);
		nvgFill(vg);
	}

	for (int i = 0; i < 3; i++)
		drawLabel(vg, c.plus(axisDirection(axes[i]).mult(labelDistance)), axisNames[i],
		          NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, LABEL_SIZE);

	drawSources(vg, [&](const Direction& d) {
		Vector3 v = directionToVector(d.azimuth, d.elevation);
		return ProjectedSource(project(v), isVisible(v) ? 1.f : 0.5f);
	});
}


/** The horizontal plane as a circle, seen from above: the front (+x) points up
 * and the left (+y) points left, so a source panned to the right shows on the
 * right of the circle. Sources are projected orthographically onto the plane,
 * as in the orthographic pair view, so one straight overhead lands at the
 * center.
 *
 * That projection alone says nothing about elevation, which is read instead on
 * the z axis standing on the right of the screen, over the whole height: every
 * source is projected onto it as well, at its own height.
 */
void AmbisonicDisplay::drawXYandZProjections(NVGcontext* vg, Vec size) {
	const float pad = 3.f;
	const float labelHalfHeight = LABEL_HALF_HEIGHT;
	/** How far a source label sitting on a rim or on the z axis reaches past
	 * it. */
	const float sourceReach = labelHalfHeight;
	/** Between an axis and its name: enough for the name to clear a source
	 * label sitting at the end of that axis. */
	const float nameGap = labelHalfHeight + 2.f;
	const float nameRoom = nameGap + labelHalfHeight;
	/** Advance of one glyph of the source font, i.e. the width of one slot of
	 * the z axis (see below). */
	const float slot = LABEL_SLOT;
	/** The z axis takes the width of three slots, whatever the number of
	 * sources it has to lay out (see below). */
	const float columnHalfWidth = 1.5f * slot;
	/** Between the circle and the z axis, with room for a source label on the
	 * right rim of the circle. */
	const float gap = 4.f + sourceReach;
	const float arrowLength = 4.f;
	const float arrowHalfWidth = 1.8f;

	// The circle takes what the screen allows once every label has its room:
	// vertically the X name above it and a rear source label below it,
	// horizontally the Y name, the z axis and a source label on the right rim.
	float r = std::min((size.y - 2 * pad - nameRoom - sourceReach) / 2.f,
	                   (size.x - 2 * pad - nameRoom - gap - 2 * columnHalfWidth) / 2.f);
	if (r <= 0.f)
		return;

	Vec c(pad + nameRoom + r, (size.y - nameRoom - 2 * r - sourceReach) / 2.f + nameRoom + r);

	// The z axis fills the height, with the Z name above its arrow and room for
	// a source label at its bottom end, and is centered in the width left to
	// the right of the circle.
	float axisTop = pad + nameRoom;
	float axisBottom = size.y - pad - sourceReach;
	float axisRadius = (axisBottom - axisTop) / 2.f;
	Vec axisCenter((c.x + r + sourceReach + size.x - pad) / 2.f, (axisTop + axisBottom) / 2.f);

	// A diameter of a plot, along the screen direction `u`, with an arrow at
	// that end whose tip touches the rim and a name just outside it.
	auto drawDiameter = [&](Vec center, float radius, Vec u, const char* name) {
		Vec tip = center.plus(u.mult(radius));
		Vec base = tip.minus(u.mult(arrowLength));
		Vec n(-u.y, u.x);

		nvgBeginPath(vg);
		Vec negative = center.minus(u.mult(radius));
		nvgMoveTo(vg, negative.x, negative.y);
		nvgLineTo(vg, base.x, base.y);
		nvgStrokeColor(vg, SCREEN_OUTLINE);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, tip.x, tip.y);
		nvgLineTo(vg, base.x + n.x * arrowHalfWidth, base.y + n.y * arrowHalfWidth);
		nvgLineTo(vg, base.x - n.x * arrowHalfWidth, base.y - n.y * arrowHalfWidth);
		nvgClosePath(vg);
		nvgFillColor(vg, SCREEN_OUTLINE);
		nvgFill(vg);

		drawText(vg, LABEL_FONT, LABEL_SIZE, SCREEN_TEXT, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE,
		         center.plus(u.mult(radius + nameGap)), name);
	};

	// Sources at 45 degrees of elevation, above or below, land on this circle:
	// the one graticule of the plot, drawn as those of the 3D sphere view.
	nvgBeginPath(vg);
	nvgCircle(vg, c.x, c.y, std::cos(45 * DEG2RAD) * r);
	nvgStrokeColor(vg, SCREEN_GRID);
	nvgStrokeWidth(vg, 0.5f);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgCircle(vg, c.x, c.y, r);
	nvgStrokeColor(vg, SCREEN_OUTLINE);
	nvgStrokeWidth(vg, 0.8f);
	nvgStroke(vg);

	drawDiameter(c, r, Vec(0.f, -1.f), "X");
	drawDiameter(c, r, Vec(-1.f, 0.f), "Y");
	drawDiameter(axisCenter, axisRadius, Vec(0.f, -1.f), "Z");

	// The origin of the z axis, i.e. the horizon.
	const float originHalfWidth = 2.f * arrowHalfWidth;
	nvgBeginPath(vg);
	nvgMoveTo(vg, axisCenter.x - originHalfWidth, axisCenter.y);
	nvgLineTo(vg, axisCenter.x + originHalfWidth, axisCenter.y);
	nvgStrokeColor(vg, SCREEN_OUTLINE);
	nvgStrokeWidth(vg, 0.8f);
	nvgStroke(vg);

	// The sources on the z axis, in slots side by side so that labels at the
	// same height do not pile up, which a whole set of them is whenever their
	// elevations coincide. The slots are centered on the axis and keep the
	// order the sources come in, so that a stereo pair reads as on the circle
	// (the left source on the left). Past three sources they narrow to stay
	// within the column and start to overlap, which reads better than labels
	// running into the circle or off the screen.
	int numSources = (int) sources.size();
	float spacing = numSources > 1 ? std::min(slot, 2.f * slot / (numSources - 1)) : 0.f;
	for (int i = 0; i < numSources; i++) {
		const Source& source = sources[i];
		float z = std::sin(source.direction.elevation * DEG2RAD);
		Vec p(axisCenter.x + (i - (numSources - 1) / 2.f) * spacing, axisCenter.y - z * axisRadius);
		drawSourceLabel(vg, ProjectedSource(p), source);
	}

	drawSources(vg, [&](const Direction& d) {
		Vector3 v = directionToVector(d.azimuth, d.elevation);
		return Vec(c.x - v.y * r, c.y - v.x * r);
	});
}


template <typename Project>
void AmbisonicDisplay::drawSources(NVGcontext* vg, const Project& project) {
	for (const Source& source : sources)
		drawSourceLabel(vg, project(source.direction), source);
}


void AmbisonicDisplay::drawSourceLabel(NVGcontext* vg, ProjectedSource p, const Source& source) {
	drawText(vg, SOURCE_FONT, LABEL_SIZE, nvgTransRGBAf(source.color, p.alpha), NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE,
	         p.pos, source.label.c_str());
}


} // namespace sp4t
