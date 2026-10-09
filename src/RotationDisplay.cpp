#include "RotationDisplay.hpp"

#include "AmbisonicDisplay.hpp"
#include "ScreenStyle.hpp"

#include <algorithm>
#include <cmath>


namespace sp4t {


namespace {

/** Rotated axes (Ayu orange). */
const NVGcolor ROTATED_AXIS = nvgRGB(0xff, 0xb4, 0x54);

} // namespace


void RotationDisplay::drawLayer(const DrawArgs& args, int layer) {
	// Scaled as AmbisonicDisplay, so that strokes and fonts match the screens
	// of the other modules.
	float scale =
		std::min(box.size.x / AmbisonicDisplay::REFERENCE_SIZE.x, box.size.y / AmbisonicDisplay::REFERENCE_SIZE.y);
	if (layer == 1 && scale > 0) {
		nvgSave(args.vg);
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
		nvgScale(args.vg, scale, scale);
		drawAxes3D(args.vg, box.size.div(scale));
		nvgRestore(args.vg);
	}
	TransparentWidget::drawLayer(args, layer);
}


void RotationDisplay::drawAxes3D(NVGcontext* vg, Vec size) {
	// Camera direction, from the center toward the eye.
	const float camAzimuth = 30 * DEG2RAD;
	const float camElevation = 30 * DEG2RAD;
	const Vector3 right = {-std::sin(camAzimuth), std::cos(camAzimuth), 0.f};
	const Vector3 up = {-std::sin(camElevation) * std::cos(camAzimuth),
	                    -std::sin(camElevation) * std::sin(camAzimuth),
	                    std::cos(camElevation)};

	const Vector3 fieldAxes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
	const char* names[3] = {"X", "Y", "Z"};
	const char* rotatedNames[3] = {"X'", "Y'", "Z'"};

	// Larger names and arrowheads than the 3D sphere view of AmbisonicDisplay,
	// over shorter axes: this screen holds six arrows and six names, and only
	// their directions matter.
	const float labelFontSize = 16.f;
	const float labelHalfHeight = 0.35f * labelFontSize; // cap height
	// Advance of one glyph of the screen font, generously, and how far a name
	// reaches right of its anchor: a primed name is anchored on its first glyph
	// (see below), so it extends one and a half glyphs past it.
	const float glyphAdvance = 0.60f * labelFontSize;
	const float labelRightReach = 1.5f * glyphAdvance;
	const float labelGap = 11.f;
	const float arrowLength = 6.f;
	const float arrowHalfWidth = 2.6f;
	const float axisWidth = 1.4f;
	/** How close to an edge a name of a rotated axis may come. */
	const float edgePadding = 3.f;
	/** How much of the centering below to apply. Putting the bounding box of
	 * the ink dead center overshoots: the names weigh much less than the axis
	 * lines, which are symmetric about the origin to begin with, so the eye
	 * places the middle of the figure between the two. */
	const float centeringFraction = 0.5f;

	// Screen direction of an axis, with a fallback for an axis pointing at the
	// camera, whose projection has no direction of its own.
	auto screenDirection = [=](const Vector3& v) {
		Vec d(dot(v, right), -dot(v, up));
		float norm = d.norm();
		return norm > 1e-4f ? d.div(norm) : Vec(0.f, -1.f);
	};

	Vec c = size.div(2);
	// Half-length of the axes: short enough that the names and the arrowheads
	// are what the eye reads first, and never so long that a name of a rotated
	// axis falls off the screen, which it could do in any direction.
	//
	// Vertically that takes one name's reach for the rotated axis, plus what
	// the centering below spends: it moves the origin down by its fraction of
	// half of what the lone Z name adds above the plot. Horizontally a name
	// reaches further right than left, so that side sets the limit.
	float radius = std::min(std::min(c.y - (1.f + centeringFraction / 2.f) * (labelGap + labelHalfHeight) - edgePadding,
	                                 c.x - labelGap - labelRightReach - edgePadding),
	                        0.62f * std::min(c.x, c.y));
	if (radius <= 0)
		return;

	// Screen offset of the tip of an axis from the origin, and of its name,
	// which trails the arrowhead by labelGap. Anchoring the name to the tip
	// instead of a fixed ring keeps that gap constant while a rotation
	// foreshortens the axis, which is otherwise visible as the name drifting
	// toward and away from its arrow.
	auto tipOffset = [=](const Vector3& v) { return Vec(radius * dot(v, right), -radius * dot(v, up)); };
	auto nameOffset = [=](const Vector3& v) { return tipOffset(v).plus(screenDirection(v).mult(labelGap)); };

	// Moves the origin part of the way onto the ink the field axes put on the
	// screen. The axis lines are diameters and so symmetric about the origin,
	// but only the positive end of each axis carries a name: with this camera
	// the Y name sits further to the right than the X name does to the left,
	// and the Z name stands alone above the plot. Only the field axes take
	// part, so both shifts are constants and the origin does not move as the
	// rotation changes.
	Vec inkMin(0.f, 0.f);
	Vec inkMax(0.f, 0.f);
	for (int i = 0; i < 3; i++) {
		Vec tip = tipOffset(fieldAxes[i]);
		inkMin = inkMin.min(Vec(-std::fabs(tip.x), -std::fabs(tip.y)));
		inkMax = inkMax.max(Vec(std::fabs(tip.x), std::fabs(tip.y)));
		// A plain name is centered on its anchor.
		Vec name = nameOffset(fieldAxes[i]);
		inkMin = inkMin.min(name.minus(Vec(glyphAdvance / 2.f, labelHalfHeight)));
		inkMax = inkMax.max(name.plus(Vec(glyphAdvance / 2.f, labelHalfHeight)));
	}
	// Never so far that a name of a rotated axis leaves the screen: one can
	// point anywhere, so its reach is the same in every direction. The radius
	// above leaves room for the shift, so this only bites on a smaller screen.
	Vec limit(std::max(c.x - radius - labelGap - glyphAdvance / 2.f - edgePadding, 0.f),
	          std::max(c.y - radius - labelGap - labelHalfHeight - edgePadding, 0.f));
	Vec shift = inkMin.plus(inkMax).div(2.f).mult(centeringFraction);
	c.x -= math::clamp(shift.x, -limit.x, limit.x);
	c.y -= math::clamp(shift.y, -limit.y, limit.y);

	auto project = [=](const Vector3& v) { return c.plus(tipOffset(v)); };

	// Axis as a diameter, with an arrow at the positive end.
	auto drawAxis = [&](const Vector3& axis, NVGcolor color) {
		Vec tip = project(axis);
		Vec d = tip.minus(c);
		// An axis pointing at the camera collapses onto the center, where a dot
		// reads better than an arrow with no length.
		if (d.norm() < arrowLength) {
			nvgBeginPath(vg);
			nvgCircle(vg, c.x, c.y, arrowHalfWidth);
			nvgFillColor(vg, color);
			nvgFill(vg);
			return;
		}
		Vec u = d.normalize();
		Vec n(-u.y, u.x);
		Vec base = tip.minus(u.mult(arrowLength));

		nvgBeginPath(vg);
		Vec negative = project(Vector3{-axis.x, -axis.y, -axis.z});
		nvgMoveTo(vg, negative.x, negative.y);
		nvgLineTo(vg, base.x, base.y);
		nvgStrokeColor(vg, color);
		nvgStrokeWidth(vg, axisWidth);
		nvgStroke(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, tip.x, tip.y);
		nvgLineTo(vg, base.x + n.x * arrowHalfWidth, base.y + n.y * arrowHalfWidth);
		nvgLineTo(vg, base.x - n.x * arrowHalfWidth, base.y - n.y * arrowHalfWidth);
		nvgClosePath(vg);
		nvgFillColor(vg, color);
		nvgFill(vg);
	};

	// The rotated axes on top, as they are the ones being set.
	for (int i = 0; i < 3; i++)
		drawAxis(fieldAxes[i], SCREEN_OUTLINE);
	for (int i = 0; i < 3; i++)
		drawAxis(axes[i], ROTATED_AXIS);

	auto drawAxisName = [&](Vec p, const char* text, NVGcolor color) {
		drawText(vg, LABEL_FONT, labelFontSize, color, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, p, text);
	};

	for (int i = 0; i < 3; i++)
		drawAxisName(c.plus(nameOffset(fieldAxes[i])), names[i], SCREEN_TEXT);

	// Advance width of a name, to align the primed names on the plain ones.
	auto nameAdvance = [&](const char* text) {
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system(LABEL_FONT));
		if (!font)
			return 0.f;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, labelFontSize);
		float bounds[4] = {0.f, 0.f, 0.f, 0.f};
		return nvgTextBounds(vg, 0.f, 0.f, text, NULL, bounds);
	};

	// The names of the rotated axes are placed by the same rule as the field
	// ones, so that at the identity each one lands on its plain counterpart,
	// drawn over it as the axes themselves are. They are aligned on their first
	// glyph rather than centered: a centered "X'" would sit half a character
	// left of the "X" it covers and read as a doubled letter.
	for (int i = 0; i < 3; i++) {
		Vec p = c.plus(nameOffset(axes[i]));
		drawText(vg,
		         LABEL_FONT,
		         labelFontSize,
		         ROTATED_AXIS,
		         NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE,
		         Vec(p.x - nameAdvance(names[i]) / 2.f, p.y),
		         rotatedNames[i]);
	}
}


} // namespace sp4t
