#include "plugin.hpp"
#include "components.hpp"
#include "MixMeterDisplay.hpp"

#include <cmath>


namespace {

enum {
	NUM_INPUTS = sp4t::MixMeterDisplay::NUM_INPUTS,
	MAX_CHANNELS = sp4t::MixMeterDisplay::MAX_CHANNELS,
};

/** Time constant of the knob and trim smoothing, in seconds. The level CV is
 * read raw: only the parameters need it, their value stepping at frame rate
 * when a knob is dragged. */
const float CONTROL_TAU = 0.005f;

/** Samples between two meter updates, and the time constant of the meters,
 * slow enough to read like a VU. */
const int METER_PERIOD = 512;
const float METER_TAU = 0.3f;

} // namespace


struct SP4TMix3 : Module {
	enum ParamId {
		LEVEL_1_PARAM,
		LEVEL_2_PARAM,
		LEVEL_3_PARAM,
		LEVEL_1_CV_PARAM,
		LEVEL_2_CV_PARAM,
		LEVEL_3_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AMBI_1_INPUT,
		AMBI_2_INPUT,
		AMBI_3_INPUT,
		LEVEL_1_CV_INPUT,
		LEVEL_2_CV_INPUT,
		LEVEL_3_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::ClockDivider controlDivider;

	/** sp4t::MixMeterDisplay::Mode */
	int displayMode = sp4t::MixMeterDisplay::MODE_BY_CHANNEL;

	/** Knob and trim of each input, smoothed against zipper noise. */
	dsp::ExponentialFilter levelFilter[NUM_INPUTS];
	dsp::ExponentialFilter trimFilter[NUM_INPUTS];

	/** Channels each input carries, 0 when it is not patched, and the highest
	 * of them, which is what the output carries. Read at control rate. */
	int channels[NUM_INPUTS] = {};
	int outChannels = 0;

	/** Sum of the squares of every metered channel since the last meter
	 * update, and the smoothed RMS the widget reads. */
	float meterSum[NUM_INPUTS][MAX_CHANNELS] = {};
	float outMeterSum[MAX_CHANNELS] = {};
	dsp::ExponentialFilter meterFilter[NUM_INPUTS][MAX_CHANNELS];
	dsp::ExponentialFilter outMeterFilter[MAX_CHANNELS];
	int meterSamples = 0;

	/** Set while there is nothing to mix. */
	bool idle = false;

	SP4TMix3() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int s = 0; s < NUM_INPUTS; s++) {
			std::string input = string::f("Input %d", s + 1);
			// No boost: the maximum is unity, which keeps the sum in range and
			// means the CV can only bring the level down from the knob.
			configParam(LEVEL_1_PARAM + s, 0.f, 1.f, 1.f, input + " level", "x");
			configParam(LEVEL_1_CV_PARAM + s, 0.f, 1.f, 1.f, input + " level CV attenuator", "%", 0.f, 100.f);
			configInput(AMBI_1_INPUT + s, input + " ambisonic (ACN/SN3D)");
			configInput(LEVEL_1_CV_INPUT + s, input + " level CV");

			levelFilter[s].setTau(CONTROL_TAU);
			trimFilter[s].setTau(CONTROL_TAU);
			levelFilter[s].out = 1.f;
			trimFilter[s].out = 1.f;
			for (int c = 0; c < MAX_CHANNELS; c++)
				meterFilter[s][c].setTau(METER_TAU);
		}
		for (int c = 0; c < MAX_CHANNELS; c++)
			outMeterFilter[c].setTau(METER_TAU);

		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");
		configBypass(AMBI_1_INPUT, AMBI_OUTPUT);

		controlDivider.setDivision(16);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		displayMode = sp4t::MixMeterDisplay::MODE_BY_CHANNEL;
		clearMeters();
		for (int s = 0; s < NUM_INPUTS; s++) {
			levelFilter[s].out = params[LEVEL_1_PARAM + s].getValue();
			trimFilter[s].out = params[LEVEL_1_CV_PARAM + s].getValue();
		}
		updateControls();
	}

	void clearMeters() {
		meterSamples = 0;
		for (int s = 0; s < NUM_INPUTS; s++) {
			for (int c = 0; c < MAX_CHANNELS; c++) {
				meterSum[s][c] = 0.f;
				meterFilter[s][c].reset();
			}
		}
		for (int c = 0; c < MAX_CHANNELS; c++) {
			outMeterSum[c] = 0.f;
			outMeterFilter[c].reset();
		}
	}

	/** The channel count of each input, i.e. the order it was encoded at. The
	 * output carries the highest of them; the channels an input does not have
	 * count as zero in the sum. */
	void updateControls() {
		outChannels = 0;
		for (int s = 0; s < NUM_INPUTS; s++) {
			channels[s] = inputs[AMBI_1_INPUT + s].isConnected()
			                  ? std::min(inputs[AMBI_1_INPUT + s].getChannels(), (int) MAX_CHANNELS)
			                  : 0;
			outChannels = std::max(outChannels, channels[s]);
		}
	}

	void process(const ProcessArgs& args) override {
		// Mixing starts as soon as there is something to mix, whether or not
		// the output is patched, so that the screen answers the inputs.
		bool connected = inputs[AMBI_1_INPUT].isConnected() || inputs[AMBI_2_INPUT].isConnected()
		                 || inputs[AMBI_3_INPUT].isConnected();

		// The channel counts are read again the moment a cable is plugged back
		// in, so that the mix does not wait for the next control tick.
		if (controlDivider.process() || (idle && connected))
			updateControls();

		if (!connected) {
			if (!idle) {
				idle = true;
				clearMeters();
				outputs[AMBI_OUTPUT].setChannels(1);
				outputs[AMBI_OUTPUT].setVoltage(0.f);
			}
			return;
		}
		idle = false;

		float sum[MAX_CHANNELS] = {};
		for (int s = 0; s < NUM_INPUTS; s++) {
			if (channels[s] == 0)
				continue;

			// The knob and the trim are smoothed, the CV is not: it is a
			// signal of its own and should keep its full bandwidth.
			float level = levelFilter[s].process(args.sampleTime, params[LEVEL_1_PARAM + s].getValue());
			float trim = trimFilter[s].process(args.sampleTime, params[LEVEL_1_CV_PARAM + s].getValue());
			float cv = inputs[LEVEL_1_CV_INPUT + s].getNormalVoltage(10.f);
			float gain = level * math::clamp(trim * cv / 10.f, 0.f, 1.f);

			for (int c = 0; c < channels[s]; c++) {
				float v = gain * inputs[AMBI_1_INPUT + s].getVoltage(c);
				sum[c] += v;
				meterSum[s][c] += v * v;
			}
		}

		outputs[AMBI_OUTPUT].setChannels(outChannels);
		for (int c = 0; c < outChannels; c++)
			outputs[AMBI_OUTPUT].setVoltage(sum[c], c);

		// The output is only metered in the mode that shows it.
		bool meterOutput = displayMode == sp4t::MixMeterDisplay::MODE_BY_CHANNEL;
		if (meterOutput) {
			for (int c = 0; c < outChannels; c++)
				outMeterSum[c] += sum[c] * sum[c];
		}

		if (++meterSamples >= METER_PERIOD) {
			float deltaTime = args.sampleTime * METER_PERIOD;
			float invCount = 1.f / METER_PERIOD;
			for (int s = 0; s < NUM_INPUTS; s++) {
				for (int c = 0; c < MAX_CHANNELS; c++) {
					meterFilter[s][c].process(deltaTime, std::sqrt(meterSum[s][c] * invCount));
					meterSum[s][c] = 0.f;
				}
			}
			for (int c = 0; c < MAX_CHANNELS; c++) {
				outMeterFilter[c].process(deltaTime, meterOutput ? std::sqrt(outMeterSum[c] * invCount) : 0.f);
				outMeterSum[c] = 0.f;
			}
			meterSamples = 0;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "displayMode", json_integer(displayMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* displayModeJ = json_object_get(rootJ, "displayMode");
		if (displayModeJ)
			displayMode =
				math::clamp((int) json_integer_value(displayModeJ), 0, (int) sp4t::MixMeterDisplay::NUM_MODES - 1);
	}
};


struct SP4TMix3Widget : ModuleWidget {
	sp4t::MixMeterDisplay* display;

	SP4TMix3Widget(SP4TMix3* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TMix3.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TMix3-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		display = createWidget<sp4t::MixMeterDisplay>(mm2px(Vec(1.728, 11.5)));
		display->box.size = mm2px(Vec(37.294, 34.039));
		addChild(display);

		// One column per input, at the x positions of the Rotate knobs.
		static const float x[NUM_INPUTS] = {7.647f, 20.32f, 33.102f};
		for (int s = 0; s < NUM_INPUTS; s++) {
			addParam(
				createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(x[s], 58.9)), module, SP4TMix3::LEVEL_1_PARAM + s));
			addParam(createParamCentered<SP4TRoundSmallKnob>(
				mm2px(Vec(x[s], 71.416)), module, SP4TMix3::LEVEL_1_CV_PARAM + s));
			addInput(createInputCentered<ThemedPJ301MPort>(
				mm2px(Vec(x[s], 81.894)), module, SP4TMix3::LEVEL_1_CV_INPUT + s));
			addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(x[s], 96.6)), module, SP4TMix3::AMBI_1_INPUT + s));
		}

		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(33.102, 114.6)), module, SP4TMix3::AMBI_OUTPUT));
	}

	void step() override {
		SP4TMix3* module = getModule<SP4TMix3>();
		if (module) {
			display->mode = module->displayMode;
			display->outChannels = module->outChannels;
			for (int s = 0; s < NUM_INPUTS; s++) {
				display->channels[s] = module->channels[s];
				for (int c = 0; c < MAX_CHANNELS; c++)
					display->levels[s][c] = module->meterFilter[s][c].out;
			}
			for (int c = 0; c < MAX_CHANNELS; c++)
				display->outLevels[c] = module->outMeterFilter[c].out;
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		SP4TMix3* module = getModule<SP4TMix3>();

		menu->addChild(new MenuSeparator);
		menu->addChild(
			createIndexPtrSubmenuItem("Display mode", sp4t::MixMeterDisplay::getModeLabels(), &module->displayMode));
	}
};


Model* modelNun4takSP4TMix3 = createModel<SP4TMix3, SP4TMix3Widget>("SP4T-Mix3");
