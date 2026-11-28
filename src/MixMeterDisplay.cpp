#include "MixMeterDisplay.hpp"

#include "AmbisonicDisplay.hpp"
#include "ScreenStyle.hpp"

#include <algorithm>
#include <cmath>


namespace sp4t {


namespace {

/** Colour of each input, Ayu dark: the orange of the sources of Source4 In,
 * the purple of the channel directions of Channels Out, and a green of its own. */
const NVGcolor INPUT_COLOR[MixMeterDisplay::NUM_INPUTS] = {
	nvgRGB(0xff, 0xb4, 0x54),
	nvgRGB(0xd4, 0xbf, 0xff),
	nvgRGB(0xaa, 0xd9, 0x4c),
};

/** Channels that carry nothing, as dim as the unused rows of Channels Out. */
const NVGcolor UNUSED_COLOR = nvgRGB(0x4d, 0x53, 0x5c);

/** Size of the input numbers, the same as every other screen label. */
const float LABEL_SIZE = 12.6f;

/** Bottom of the scale, in dB below 5 V (Rack's audio level), and the spacing
 * of the grid lines. */
const float SCALE_DB = 48.f;
const float GRID_DB = 12.f;

/** Margins of the plot, in reference units. */
const float MARGIN_X = 3.f;
const float MARGIN_TOP = 3.f;
const float MARGIN_BOTTOM = 2.f;
/** Strip under the plot holding the order ticks and numbers. */
const float ORDER_STRIP = 11.f;
/** Gutter holding the input number of each row, MODE_BY_INPUT only. */
const float LABEL_GUTTER = 11.f;

/** Order of an ACN channel, and the channel each order starts at. */
const int ORDER_START[5] = {0, 1, 4, 9, 16};
const int NUM_ORDERS = 4;

/** Height of a bar as a fraction of the plot, 0 at the bottom of the scale. */
float barFraction(float rms) {
	if (!(rms > 0.f))
		return 0.f;
	float db = 20.f * std::log10(rms / 5.f);
	return math::clamp((db + SCALE_DB) / SCALE_DB, 0.f, 1.f);
}

void fillRect(NVGcontext* vg, float x, float y, float w, float h, NVGcolor color) {
	nvgBeginPath(vg);
	nvgRect(vg, x, y, w, h);
	nvgFillColor(vg, color);
	nvgFill(vg);
}

/** Grid lines every GRID_DB, the bottom of the scale included. */
void drawGrid(NVGcontext* vg, float left, float right, float top, float bottom) {
	nvgBeginPath(vg);
	for (float db = 0.f; db <= SCALE_DB; db += GRID_DB) {
		float y = top + (bottom - top) * (db / SCALE_DB);
		nvgMoveTo(vg, left, y);
		nvgLineTo(vg, right, y);
	}
	nvgStrokeColor(vg, SCREEN_GRID);
	nvgStrokeWidth(vg, 0.6f);
	nvgStroke(vg);
}

/** Ticks between the orders and their numbers, in the strip under the plot.
 *
 * @param slot width of one channel along the plot
 * @param left x of the first channel slot
 */
void drawOrderStrip(NVGcontext* vg, float left, float slot, float bottom) {
	float tickTop = bottom + 1.f;
	float tickBottom = tickTop + 3.f;
	// Centred rather than hung from its top: a 7 unit font draws a taller line
	// box than that, which would run past the bottom of the screen.
	float labelCentre = tickBottom + 4.5f;

	nvgBeginPath(vg);
	for (int o = 1; o < NUM_ORDERS; o++) {
		float x = left + ORDER_START[o] * slot;
		nvgMoveTo(vg, x, tickTop);
		nvgLineTo(vg, x, tickBottom);
	}
	nvgStrokeColor(vg, SCREEN_OUTLINE);
	nvgStrokeWidth(vg, 0.8f);
	nvgStroke(vg);

	for (int o = 0; o < NUM_ORDERS; o++) {
		float centre = left + (ORDER_START[o] + ORDER_START[o + 1]) * slot / 2.f;
		drawGridLabel(vg, Vec(centre, labelCentre), string::f("%d", o), NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	}
}

} // namespace


std::vector<std::string> MixMeterDisplay::getModeLabels() {
	return {"Group by ambisonic channel", "Group by ambisonic input"};
}


void MixMeterDisplay::drawLayer(const DrawArgs& args, int layer) {
	// Scaled as AmbisonicDisplay, so that strokes and fonts match the screens
	// of the other modules.
	float scale =
		std::min(box.size.x / AmbisonicDisplay::REFERENCE_SIZE.x, box.size.y / AmbisonicDisplay::REFERENCE_SIZE.y);
	if (layer == 1 && scale > 0) {
		nvgSave(args.vg);
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
		nvgScale(args.vg, scale, scale);
		Vec size = box.size.div(scale);
		if (mode == MODE_BY_INPUT)
			drawByInput(args.vg, size);
		else
			drawByChannel(args.vg, size);
		nvgRestore(args.vg);
	}
	TransparentWidget::drawLayer(args, layer);
}


/** One row of 16 bars per input, the input number introducing its row. */
void MixMeterDisplay::drawByInput(NVGcontext* vg, Vec size) {
	float left = MARGIN_X + LABEL_GUTTER;
	float right = size.x - MARGIN_X;
	float slot = (right - left) / MAX_CHANNELS;
	float barWidth = slot * 0.62f;

	float plotBottom = size.y - MARGIN_BOTTOM - ORDER_STRIP;
	float rowPitch = (plotBottom - MARGIN_TOP) / NUM_INPUTS;
	// The gap sits above each row, so that the baseline of the last one lands
	// on the order ticks.
	float barHeight = rowPitch * 0.86f;

	for (int s = 0; s < NUM_INPUTS; s++) {
		float bottom = MARGIN_TOP + (s + 1) * rowPitch;
		float top = bottom - barHeight;
		drawGrid(vg, left, right, top, bottom);

		drawText(vg,
		         SOURCE_FONT,
		         LABEL_SIZE,
		         INPUT_COLOR[s],
		         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE,
		         Vec(MARGIN_X + LABEL_GUTTER / 2.f, (top + bottom) / 2.f),
		         string::f("%d", s + 1).c_str());

		for (int c = 0; c < MAX_CHANNELS; c++) {
			float x = left + c * slot + (slot - barWidth) / 2.f;
			if (c >= channels[s]) {
				// Carries nothing: a flat dash on the baseline.
				fillRect(vg, x, bottom - 0.8f, barWidth, 0.8f, UNUSED_COLOR);
				continue;
			}
			float h = barFraction(levels[s][c]) * barHeight;
			if (h < 0.8f)
				h = 0.8f;
			fillRect(vg, x, bottom - h, barWidth, h, INPUT_COLOR[s]);
		}
	}

	drawOrderStrip(vg, left, slot, plotBottom);
}


/** One group of three bars per Ambisonic channel, with the level of the output
 * laid over each group as a tick. */
void MixMeterDisplay::drawByChannel(NVGcontext* vg, Vec size) {
	float left = MARGIN_X;
	float right = size.x - MARGIN_X;
	float slot = (right - left) / MAX_CHANNELS;
	float groupWidth = slot * 0.86f;
	float barWidth = groupWidth / NUM_INPUTS;

	float top = MARGIN_TOP;
	float bottom = size.y - MARGIN_BOTTOM - ORDER_STRIP;
	float height = bottom - top;
	drawGrid(vg, left, right, top, bottom);

	for (int c = 0; c < MAX_CHANNELS; c++) {
		float groupLeft = left + c * slot + (slot - groupWidth) / 2.f;
		for (int s = 0; s < NUM_INPUTS; s++) {
			float x = groupLeft + s * barWidth;
			if (c >= channels[s]) {
				fillRect(vg, x, bottom - 0.8f, barWidth * 0.85f, 0.8f, UNUSED_COLOR);
				continue;
			}
			float h = barFraction(levels[s][c]) * height;
			if (h < 0.8f)
				h = 0.8f;
			fillRect(vg, x, bottom - h, barWidth * 0.85f, h, INPUT_COLOR[s]);
		}

		// The output is a tick over the group rather than a fourth bar: it is
		// the sum of the three, not another input.
		if (c >= outChannels)
			continue;
		float y = bottom - barFraction(outLevels[c]) * height;
		fillRect(vg, groupLeft - 0.6f, y - 0.5f, groupWidth + 1.2f, 1.f, SCREEN_TEXT);
	}

	drawOrderStrip(vg, left, slot, bottom);
}


} // namespace sp4t
