#include "plugin.hpp"
#include "components.hpp"
#include "AmbiMicPairDecoder.hpp"
#include "MicPatternDisplay.hpp"

#include <algorithm>
#include <cmath>
#include <string>


namespace {

/** Shows the pattern the knob is on, named when it is one of the named ones,
 * e.g. "0.5 (cardioid)". */
struct PatternQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		std::string text = ParamQuantity::getDisplayValueString();
		bool exact = false;
		const sp4t::NamedMicPattern& named = sp4t::nearestMicPattern(getDisplayValue(), &exact);
		if (exact)
			text += string::f(" (%s)", named.name);
		return text;
	}
};

} // namespace


struct SP4TXYMSOut : Module {
	enum ParamId {
		PATTERN_PARAM,
		SPREAD_PARAM,
		PATTERN_CV_PARAM,
		SPREAD_CV_PARAM,
		MODE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
		PATTERN_CV_INPUT,
		SPREAD_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		XY_LEFT_LED_LIGHT,
		XY_RIGHT_LED_LIGHT,
		MS_MID_LED_LIGHT,
		MS_SIDE_LED_LIGHT,
		LIGHTS_LEN
	};

	sp4t::AmbiMicPairDecoder decoder;
	dsp::ClockDivider controlDivider;
	dsp::ClockDivider lightDivider;

	/** When enabled, the outputs keep the same diffuse-field level whatever
	 * the pattern and the spread. */
	bool levelCompensation = false;

	/** Nothing to decode: no input, or nowhere to send the result. */
	bool idle = false;

	/** Peak of the two signals the LEDs show since the last light update. */
	float peaks[2] = {0.f, 0.f};

	SP4TXYMSOut() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam<PatternQuantity>(PATTERN_PARAM, 0.f, 1.f, 0.5f, "Pattern");
		configParam(SPREAD_PARAM, 0.f, 90.f, 45.f, "Spread", "°");
		configParam(PATTERN_CV_PARAM, -1.f, 1.f, 0.f, "Pattern CV attenuverter", "%", 0.f, 100.f);
		configParam(SPREAD_CV_PARAM, -1.f, 1.f, 0.f, "Spread CV attenuverter", "%", 0.f, 100.f);
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Microphone pair", {"X/Y (coincident pair)", "M/S (mid and side)"});

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configInput(PATTERN_CV_INPUT, "Pattern CV");
		configInput(SPREAD_CV_INPUT, "Spread CV");
		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
		configLight(XY_LEFT_LED_LIGHT, "X/Y left level");
		configLight(XY_RIGHT_LED_LIGHT, "X/Y right level");
		configLight(MS_MID_LED_LIGHT, "M/S mid level");
		configLight(MS_SIDE_LED_LIGHT, "M/S side level");

		controlDivider.setDivision(16);
		lightDivider.setDivision(256);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		levelCompensation = false;
		decoder.reset();
		updateControls();
	}

	/** Knob value plus the CV input scaled by its attenuverter, where 10 V
	 * spans the whole range of the knob, from 0 to `max`. */
	float getControl(int paramId, int cvParamId, int inputId, float max) {
		float cv = inputs[inputId].getVoltage() / 10.f;
		return math::clamp(params[paramId].getValue() + params[cvParamId].getValue() * cv * max, 0.f, max);
	}

	void updateControls() {
		decoder.setNumInputChannels(inputs[AMBI_INPUT].getChannels());
		decoder.setMode(params[MODE_PARAM].getValue() > 0.5f ? sp4t::AmbiMicPairDecoder::MODE_MS
		                                                     : sp4t::AmbiMicPairDecoder::MODE_XY);
		decoder.setPattern(getControl(PATTERN_PARAM, PATTERN_CV_PARAM, PATTERN_CV_INPUT, 1.f));
		decoder.setSpread(getControl(SPREAD_PARAM, SPREAD_CV_PARAM, SPREAD_CV_INPUT, 90.f));
		decoder.setLevelCompensation(levelCompensation);
	}

	void process(const ProcessArgs& args) override {
		bool connected = inputs[AMBI_INPUT].isConnected()
		                 && (outputs[LEFT_OUTPUT].isConnected() || outputs[RIGHT_OUTPUT].isConnected());

		// The controls are read even while the module sits unpatched, so that
		// the screen follows them, and again the moment a cable is plugged
		// back in, so that the decoder does not wait for the next tick.
		if (controlDivider.process() || (idle && connected))
			updateControls();

		if (!connected) {
			if (!idle) {
				idle = true;
				// The gains in force when the module is patched back in are
				// the ones its controls ask for, not the ones it was
				// interpolating through when it was unpatched.
				decoder.reset();
				outputs[LEFT_OUTPUT].setVoltage(0.f);
				outputs[RIGHT_OUTPUT].setVoltage(0.f);
				for (int i = 0; i < LIGHTS_LEN; i++)
					lights[i].setBrightness(0.f);
				peaks[0] = peaks[1] = 0.f;
			}
			return;
		}
		idle = false;

		float in[sp4t::AmbiMicPairDecoder::NUM_SH] = {};
		int channels = std::min(inputs[AMBI_INPUT].getChannels(), (int) sp4t::AmbiMicPairDecoder::NUM_SH);
		for (int c = 0; c < channels; c++)
			in[c] = inputs[AMBI_INPUT].getVoltage(c);

		float out[sp4t::AmbiMicPairDecoder::NUM_CHANNELS];
		decoder.process(in, out);

		outputs[LEFT_OUTPUT].setVoltage(out[0]);
		outputs[RIGHT_OUTPUT].setVoltage(out[1]);

		// The LEDs show the peak of the audio-rate signals of the microphones
		// of the active pair, full scale at 5 V with a cube-root response so
		// that quiet signals still show brightly. The mid and side signals are
		// what the outputs sum and difference to.
		bool msMode = decoder.getMode() == sp4t::AmbiMicPairDecoder::MODE_MS;
		float monitored[2] = {msMode ? 0.5f * (out[0] + out[1]) : out[0], msMode ? 0.5f * (out[0] - out[1]) : out[1]};
		for (int i = 0; i < 2; i++)
			peaks[i] = std::fmax(peaks[i], std::fabs(monitored[i]));

		if (lightDivider.process()) {
			float deltaTime = args.sampleTime * lightDivider.getDivision();
			int first = msMode ? MS_MID_LED_LIGHT : XY_LEFT_LED_LIGHT;
			for (int i = 0; i < LIGHTS_LEN; i++) {
				// The LEDs of the pair that is not in use stay off.
				float brightness =
					(i == first || i == first + 1) ? std::cbrt(std::fmin(peaks[i - first] / 5.f, 1.f)) : 0.f;
				lights[i].setBrightnessSmooth(brightness, deltaTime);
			}
			peaks[0] = peaks[1] = 0.f;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "levelCompensation", json_boolean(levelCompensation));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* levelCompensationJ = json_object_get(rootJ, "levelCompensation");
		if (levelCompensationJ)
			levelCompensation = json_boolean_value(levelCompensationJ);
	}
};


struct SP4TXYMSOutWidget : ModuleWidget {
	sp4t::MicPatternDisplay* display;

	SP4TXYMSOutWidget(SP4TXYMSOut* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TXY-MSOut.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TXY-MSOut-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		display = createWidget<sp4t::MicPatternDisplay>(mm2px(Vec(1.728, 11.5)));
		display->box.size = mm2px(Vec(37.294, 34.039));
		addChild(display);

		addChild(createLightCentered<SP4TSignalLight>(mm2px(Vec(5.79, 49.65)), module, SP4TXYMSOut::XY_LEFT_LED_LIGHT));
		addChild(
			createLightCentered<SP4TSignalLight>(mm2px(Vec(8.19, 49.65)), module, SP4TXYMSOut::XY_RIGHT_LED_LIGHT));
		addChild(createLightCentered<SP4TSignalLight>(mm2px(Vec(16.1, 49.65)), module, SP4TXYMSOut::MS_MID_LED_LIGHT));
		addChild(createLightCentered<SP4TSignalLight>(mm2px(Vec(18.5, 49.65)), module, SP4TXYMSOut::MS_SIDE_LED_LIGHT));

		addParam(createParamCentered<SP4TSwitchTwo>(mm2px(Vec(34.78, 51.83)), module, SP4TXYMSOut::MODE_PARAM));

		addParam(
			createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(11.42, 70.084)), module, SP4TXYMSOut::PATTERN_PARAM));
		addParam(
			createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(11.42, 83.992)), module, SP4TXYMSOut::PATTERN_CV_PARAM));
		addInput(
			createInputCentered<ThemedPJ301MPort>(mm2px(Vec(11.42, 94.469)), module, SP4TXYMSOut::PATTERN_CV_INPUT));

		addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(29.22, 70.084)), module, SP4TXYMSOut::SPREAD_PARAM));
		addParam(
			createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(29.22, 83.992)), module, SP4TXYMSOut::SPREAD_CV_PARAM));
		addInput(
			createInputCentered<ThemedPJ301MPort>(mm2px(Vec(29.22, 94.469)), module, SP4TXYMSOut::SPREAD_CV_INPUT));

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(7.419, 114.0)), module, SP4TXYMSOut::AMBI_INPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(24.505, 114.0)), module, SP4TXYMSOut::LEFT_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(33.335, 114.0)), module, SP4TXYMSOut::RIGHT_OUTPUT));
	}

	/** Shows the pair the module is decoding with, controls included. */
	void step() override {
		SP4TXYMSOut* module = getModule<SP4TXYMSOut>();
		if (module) {
			display->mode = module->decoder.getMode();
			display->pattern = module->decoder.getPattern();
			display->spread = module->decoder.getSpread();
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		SP4TXYMSOut* module = getModule<SP4TXYMSOut>();

		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Diffuse-field level compensation", "", &module->levelCompensation));
	}
};


Model* modelNun4takSP4TXYMSOut = createModel<SP4TXYMSOut, SP4TXYMSOutWidget>("SP4T-XY-MS-Out");
