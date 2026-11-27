#pragma once

#include "plugin.hpp"
#include "AmbiMicPairDecoder.hpp"


namespace sp4t {


/** Screen showing the polar patterns of a pair of coincident virtual
 * microphones, in the horizontal plane, with the front up and the left on the
 * left.
 *
 * The canvas is the horizontal plane of the XY/Z projections view of
 * AmbisonicDisplay, without its z axis: the same reference units, colors and
 * strokes, at the label size of the Rotate screen. The outline circle is a
 * gain of 1 and the graticule circle a gain of 0.5; the scale is fixed, so a
 * change of level shows as the patterns growing or shrinking. Lobes of
 * inverted polarity are dashed and drawn in the same color, fainter. The mode
 * and the pattern are named in the bottom corners of the screen.
 *
 * The widget is independent of any module: its owner sets the mode and the
 * controls, e.g., from ModuleWidget::step() (UI thread).
 */
struct MicPatternDisplay : TransparentWidget {
	/** AmbiMicPairDecoder::Mode */
	int mode = AmbiMicPairDecoder::MODE_XY;
	/** Weight of the figure-8 component of the pattern, 0 to 1. */
	float pattern = 0.5f;
	/** Angle between a microphone and the front axis (X/Y) or mid/side
	 * balance (M/S), in degrees. */
	float spread = 45.f;

	void drawLayer(const DrawArgs& args, int layer) override;

private:
	void drawPolarPlot(NVGcontext* vg, Vec size);
	/** One pattern, as the polar curve of
	 * gain(azimuth) = w + x * cos(azimuth) + y * sin(azimuth). */
	void drawPattern(NVGcontext* vg, Vec c, float radius, NVGcolor color, float w, float x, float y);
};


} // namespace sp4t
