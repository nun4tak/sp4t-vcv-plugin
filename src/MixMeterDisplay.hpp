#pragma once

#include "plugin.hpp"

#include <string>
#include <vector>


namespace sp4t {


/** Screen showing the level of every Ambisonic channel of the inputs a mixer
 * sums, one bar per channel and per input, in the colour of the input.
 *
 * A hint rather than a monitoring meter: the levels are RMS values smoothed
 * over about a third of a second, on a 48 dB scale with no numbers on it. The
 * channels an input does not carry are drawn as flat dashes, so that the order
 * of each input can be read at a glance.
 *
 * The widget is independent of any module: its owner fills in the levels and
 * the channel counts, e.g., from ModuleWidget::step() (UI thread).
 */
struct MixMeterDisplay : TransparentWidget {
	enum {
		NUM_INPUTS = 3,
		MAX_CHANNELS = 16,
	};

	/** How the bars are grouped. */
	enum Mode {
		/** One group of three bars per Ambisonic channel. */
		MODE_BY_CHANNEL,
		/** One row of 16 bars per input. */
		MODE_BY_INPUT,
		NUM_MODES
	};

	/** Menu labels of the modes, in Mode order. */
	static std::vector<std::string> getModeLabels();

	int mode = MODE_BY_CHANNEL;

	/** Channels each input carries, 0 when it is not patched. */
	int channels[NUM_INPUTS] = {};
	/** Channels the output carries. */
	int outChannels = 0;

	/** RMS level of each channel of each input, in volts, as it enters the sum
	 * (i.e. after the input gain). */
	float levels[NUM_INPUTS][MAX_CHANNELS] = {};
	/** RMS level of each output channel, drawn in MODE_BY_CHANNEL only. */
	float outLevels[MAX_CHANNELS] = {};

	void drawLayer(const DrawArgs& args, int layer) override;

private:
	void drawByChannel(NVGcontext* vg, Vec size);
	void drawByInput(NVGcontext* vg, Vec size);
};


} // namespace sp4t
