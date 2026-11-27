#include "MicPatternDisplay.hpp"

#include "AmbisonicDisplay.hpp"
#include "ScreenStyle.hpp"
#include "SphericalCoordinates.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>


namespace sp4t {


namespace {

/** Size of every label of this screen, the same the names of the Rotate screen
 * are drawn at: both screens are drawn in the same reference units and their
 * panels give them the same width, so they come out alike. */
const float LABEL_SIZE = 16.f;

/** Metrics of one glyph at that size, as fractions of it: cap height, how far
 * capitals reach above the baseline, and the advance of the screen font. */
const float LABEL_HALF_HEIGHT = 0.35f * LABEL_SIZE;
const float LABEL_CAP_HEIGHT = 0.7f * LABEL_SIZE;
const float LABEL_SLOT = 0.6f * LABEL_SIZE;

/** What the module is set to, as opposed to the canvas it is drawn on: the
 * Ayu orange the Rotate and Source4 In screens give what their module does. */
const NVGcolor INFO_TEXT = nvgRGB(0xff, 0xb4, 0x54);

/** Colors of the microphones (Ayu dark), the same the sources of Stereo In
 * are given: the X/Y pair reads as the left and right it decodes to, the M/S
 * pair as a mid and a side signal. */
const NVGcolor MIC_LEFT = nvgRGB(0x59, 0xc2, 0xff);
const NVGcolor MIC_RIGHT = nvgRGB(0xf0, 0x71, 0x78);
const NVGcolor MIC_MID = nvgRGB(0xff, 0xb4, 0x54);
const NVGcolor MIC_SIDE = nvgRGB(0xd2, 0xa6, 0xff);

/** A lobe of inverted polarity is dashed, and fainter still. */
const float INVERTED_ALPHA = 0.7f;
const float DASH_LENGTH = 5.f;
const float DASH_GAP = 3.5f;

const float PATTERN_WIDTH = 1.4f;

/** Samples of a polar curve, i.e. one every two degrees. */
const int NUM_PATTERN_STEPS = 180;

const char* const MODE_NAMES[AmbiMicPairDecoder::MODES_LEN] = {"X/Y", "M/S"};

/** Glyphs of the longest pattern abbreviation, plus the tilde that marks a
 * value on the way from one named pattern to the next. */
int longestPatternText() {
	std::size_t longest = 0;
	for (int i = 0; i < NUM_NAMED_MIC_PATTERNS; i++)
		longest = std::max(longest, std::strlen(NAMED_MIC_PATTERNS[i].abbreviation));
	return (int) longest + 1;
}

/** One run of a polar curve: plain for the polarity of the pattern, dashed and
 * fainter for an inverted lobe. nanovg has no dash support, so the dashes are
 * stroked one by one along the polyline. */
void strokeRun(NVGcontext* vg, const Vec* points, int n, NVGcolor color, bool positive) {
	if (n < 2)
		return;
	nvgStrokeColor(vg, nvgTransRGBAf(color, positive ? 1.f : INVERTED_ALPHA));
	nvgBeginPath(vg);
	nvgMoveTo(vg, points[0].x, points[0].y);

	if (positive) {
		for (int i = 1; i < n; i++)
			nvgLineTo(vg, points[i].x, points[i].y);
		nvgStroke(vg);
		return;
	}

	// `left` is how much of the current dash, or of the gap after it, is still
	// to run; the pen turns over wherever that falls inside a segment.
	bool penDown = true;
	float left = DASH_LENGTH;
	for (int i = 1; i < n; i++) {
		Vec from = points[i - 1];
		Vec to = points[i];
		float remaining = to.minus(from).norm();
		if (remaining <= 0.f)
			continue;
		while (remaining > left) {
			Vec p = from.plus(to.minus(from).mult(left / remaining));
			if (penDown) {
				nvgLineTo(vg, p.x, p.y);
				nvgStroke(vg);
			}
			else {
				nvgBeginPath(vg);
				nvgMoveTo(vg, p.x, p.y);
			}
			penDown = !penDown;
			remaining -= left;
			from = p;
			left = penDown ? DASH_LENGTH : DASH_GAP;
		}
		left -= remaining;
		if (penDown)
			nvgLineTo(vg, to.x, to.y);
	}
	if (penDown)
		nvgStroke(vg);
}

} // namespace


void MicPatternDisplay::drawLayer(const DrawArgs& args, int layer) {
	// Scaled as AmbisonicDisplay, so that strokes and fonts match the screens
	// of the other modules.
	float scale =
		std::min(box.size.x / AmbisonicDisplay::REFERENCE_SIZE.x, box.size.y / AmbisonicDisplay::REFERENCE_SIZE.y);
	if (layer == 1 && scale > 0) {
		nvgSave(args.vg);
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
		nvgScale(args.vg, scale, scale);
		drawPolarPlot(args.vg, box.size.div(scale));
		nvgRestore(args.vg);
	}
	TransparentWidget::drawLayer(args, layer);
}


void MicPatternDisplay::drawPolarPlot(NVGcontext* vg, Vec size) {
	const float pad = 3.f;
	/** Between an axis and its name. */
	const float nameGap = LABEL_HALF_HEIGHT + 2.f;
	const float nameRoom = nameGap + LABEL_HALF_HEIGHT;
	const float arrowLength = 4.f;
	const float arrowHalfWidth = 1.8f;

	// The mode and the pattern sit in the two bottom corners. Both keep the
	// room of the longest text they can hold, whatever is being shown, so that
	// the plot does not resize as the controls are turned.
	float textBaseline = size.y - pad;
	float textTop = textBaseline - LABEL_CAP_HEIGHT;
	float leftTextWidth = 3 * LABEL_SLOT;
	float rightTextWidth = longestPatternText() * LABEL_SLOT;

	// The circle is centered in the width and hangs below the row of the X
	// name, taking all the room left down to those two texts. With
	// `cy = pad + nameRoom + r`, clearing a text whose inner edge is `dx` from
	// the center means `r + sqrt(r^2 - dx^2) <= room`, hence the last two
	// limits below.
	float cx = size.x / 2.f;
	float room = textTop - pad - nameRoom;
	float dxLeft = cx - pad - leftTextWidth;
	float dxRight = size.x - pad - rightTextWidth - cx;
	if (room <= 0.f)
		return;
	float r = std::min((size.x - 2 * pad - 2 * nameRoom) / 2.f, (size.y - 2 * pad - nameRoom) / 2.f);
	r = std::min(r, (room * room + dxLeft * dxLeft) / (2 * room));
	r = std::min(r, (room * room + dxRight * dxRight) / (2 * room));
	if (r <= 0.f)
		return;
	Vec c(cx, pad + nameRoom + r);

	// A diameter of the plot, along the screen direction `u`, with an arrow at
	// that end whose tip touches the rim and a name just outside it. Drawn as
	// the axes of the XY/Z projections view of AmbisonicDisplay.
	auto drawDiameter = [&](Vec u, const char* name) {
		Vec tip = c.plus(u.mult(r));
		Vec base = tip.minus(u.mult(arrowLength));
		Vec n(-u.y, u.x);

		nvgBeginPath(vg);
		Vec negative = c.minus(u.mult(r));
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

		drawText(vg,
		         LABEL_FONT,
		         LABEL_SIZE,
		         SCREEN_TEXT,
		         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE,
		         c.plus(u.mult(r + nameGap)),
		         name);
	};

	// Half of the gain of the rim: the one graticule of the plot.
	nvgBeginPath(vg);
	nvgCircle(vg, c.x, c.y, 0.5f * r);
	nvgStrokeColor(vg, SCREEN_GRID);
	nvgStrokeWidth(vg, 0.5f);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgCircle(vg, c.x, c.y, r);
	nvgStrokeColor(vg, SCREEN_OUTLINE);
	nvgStrokeWidth(vg, 0.8f);
	nvgStroke(vg);

	drawDiameter(Vec(0.f, -1.f), "X");
	drawDiameter(Vec(-1.f, 0.f), "Y");

	// The patterns, as the decoder weighs the W, X and Y channels (the pair
	// lies flat, so Z never takes part).
	float d = math::clamp(pattern, 0.f, 1.f);
	float a = 1.f - d;
	float cosSpread = std::cos(math::clamp(spread, 0.f, 90.f) * DEG2RAD);
	float sinSpread = std::sin(math::clamp(spread, 0.f, 90.f) * DEG2RAD);
	if (mode == AmbiMicPairDecoder::MODE_MS) {
		// Each scaled by its share of the mid/side balance, so that the spread
		// shows as the side lobes growing against the mid.
		drawPattern(vg, c, r, MIC_MID, cosSpread * a, cosSpread * d, 0.f);
		drawPattern(vg, c, r, MIC_SIDE, 0.f, 0.f, sinSpread);
	}
	else {
		drawPattern(vg, c, r, MIC_LEFT, a, d * cosSpread, d * sinSpread);
		drawPattern(vg, c, r, MIC_RIGHT, a, d * cosSpread, -d * sinSpread);
	}

	// The pair in use, in the bottom left corner.
	int modeIndex = math::clamp(mode, 0, (int) AmbiMicPairDecoder::MODES_LEN - 1);
	drawText(vg,
	         LABEL_FONT,
	         LABEL_SIZE,
	         INFO_TEXT,
	         NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE,
	         Vec(pad, textBaseline),
	         MODE_NAMES[modeIndex]);

	// The pattern the knob is on, in the bottom right corner, named when it is
	// one of the named ones and prefixed with a tilde on the way from one to
	// the next.
	bool exact = false;
	const NamedMicPattern& named = nearestMicPattern(d, &exact);
	std::string text = exact ? named.abbreviation : "~" + std::string(named.abbreviation);
	drawText(vg,
	         LABEL_FONT,
	         LABEL_SIZE,
	         INFO_TEXT,
	         NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE,
	         Vec(size.x - pad, textBaseline),
	         text.c_str());
}


void MicPatternDisplay::drawPattern(NVGcontext* vg, Vec c, float radius, NVGcolor color, float w, float x, float y) {
	if (w == 0.f && x == 0.f && y == 0.f)
		return;

	const float step = 2.f * float(M_PI) / NUM_PATTERN_STEPS;
	auto gainAt = [&](int i) {
		float azimuth = i * step;
		return w + x * std::cos(azimuth) + y * std::sin(azimuth);
	};
	// The front is up and the left is on the left, as on the rest of the
	// screen; a lobe of inverted polarity is drawn on the side it points to
	// all the same, which is what a polar plot shows.
	auto pointAt = [&](int i, float gain) {
		float azimuth = i * step;
		return Vec(c.x - std::sin(azimuth) * gain * radius, c.y - std::cos(azimuth) * gain * radius);
	};

	// Where the curve first goes through zero, if it does at all. Walking from
	// there keeps every lobe in one piece; a curve that keeps its polarity is
	// one closed loop instead.
	int start = 0;
	bool split = false;
	for (int i = 0; i < NUM_PATTERN_STEPS && !split; i++) {
		if ((gainAt(i) >= 0.f) != (gainAt((i + 1) % NUM_PATTERN_STEPS) >= 0.f)) {
			start = (i + 1) % NUM_PATTERN_STEPS;
			split = true;
		}
	}
	// A split curve ends on the zero its last lobe runs into; a loop has to
	// come back to the sample it started from.
	int lastStep = split ? NUM_PATTERN_STEPS - 1 : NUM_PATTERN_STEPS;

	nvgStrokeWidth(vg, PATTERN_WIDTH);
	nvgLineJoin(vg, NVG_ROUND);
	nvgLineCap(vg, NVG_ROUND);

	// Each lobe is laid out before being stroked, so that its dashes can be
	// walked along it. The two spare points are the center it grows out of and
	// the one it runs back into.
	Vec run[NUM_PATTERN_STEPS + 3];
	int n = 0;
	bool positive = true;
	for (int k = 0; k <= lastStep; k++) {
		int i = (start + k) % NUM_PATTERN_STEPS;
		float gain = gainAt(i);
		if (n == 0) {
			positive = gain >= 0.f;
			if (split)
				run[n++] = c;
		}
		run[n++] = pointAt(i, std::fabs(gain));

		if (k == lastStep || (gainAt((i + 1) % NUM_PATTERN_STEPS) >= 0.f) != positive) {
			run[n++] = split ? c : run[0];
			strokeRun(vg, run, n, color, positive);
			n = 0;
		}
	}
}


} // namespace sp4t
