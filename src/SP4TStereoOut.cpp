#include "plugin.hpp"
#include "components.hpp"
#include "AmbiStereoDecoder.hpp"

#include <cmath>


struct SP4TStereoOut : Module {
	enum ParamId {
		SPREAD_PARAM,
		SPREAD_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
		SPREAD_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LEFT_LED_LIGHT,
		RIGHT_LED_LIGHT,
		LIGHTS_LEN
	};

	sp4t::AmbiStereoDecoder decoder;
	dsp::ClockDivider controlDivider;
	dsp::ClockDivider lightDivider;

	/** When enabled, only the W and Y input channels are decoded (mid-side). */
	bool crudeMode = false;

	/** Peak of the output signals since the last light update. */
	float peaks[2] = {0.f, 0.f};

	SP4TStereoOut() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SPREAD_PARAM, 0.f, 90.f, 45.f, "Spread", "°");
		configParam(SPREAD_CV_PARAM, -1.f, 1.f, 0.f, "Spread CV attenuverter", "%", 0.f, 100.f);

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configInput(SPREAD_CV_INPUT, "Spread CV");
		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
		configLight(LEFT_LED_LIGHT, "Left level");
		configLight(RIGHT_LED_LIGHT, "Right level");

		controlDivider.setDivision(16);
		lightDivider.setDivision(256);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		crudeMode = false;
		decoder.reset();
		updateControls();
	}

	void updateControls() {
		// The order of the input signal is given by the number of channels of
		// the polyphonic cable, i.e., 4 (FOA), 9 (HOA-2) or 16 (HOA-3).
		int channels = inputs[AMBI_INPUT].getChannels();
		decoder.setOrder((int) std::floor(std::sqrt((float) channels)) - 1);

		float cv = inputs[SPREAD_CV_INPUT].getVoltage() / 10.f;
		decoder.setSpread(params[SPREAD_PARAM].getValue() + params[SPREAD_CV_PARAM].getValue() * cv * 90.f);
		decoder.setCrudeMode(crudeMode);
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		float in[sp4t::AmbiStereoDecoder::MAX_NUM_SH] = {};
		for (int c = 0; c < decoder.getNumSH(); c++)
			in[c] = inputs[AMBI_INPUT].getVoltage(c);

		float out[sp4t::AmbiStereoDecoder::NUM_CHANNELS];
		decoder.process(in, out);

		outputs[LEFT_OUTPUT].setVoltage(out[0]);
		outputs[RIGHT_OUTPUT].setVoltage(out[1]);

		// The LEDs show the peak of the audio-rate output signals, full scale at
		// 5 V with a cube-root response so that quiet signals still show brightly.
		for (int i = 0; i < 2; i++)
			peaks[i] = std::fmax(peaks[i], std::fabs(out[i]));
		if (lightDivider.process()) {
			float deltaTime = args.sampleTime * lightDivider.getDivision();
			for (int i = 0; i < 2; i++) {
				float brightness = std::cbrt(std::fmin(peaks[i] / 5.f, 1.f));
				lights[LEFT_LED_LIGHT + i].setBrightnessSmooth(brightness, deltaTime);
				peaks[i] = 0.f;
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "crudeMode", json_boolean(crudeMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* crudeModeJ = json_object_get(rootJ, "crudeMode");
		if (crudeModeJ)
			crudeMode = json_boolean_value(crudeModeJ);
	}
};


struct SP4TStereoOutWidget : ModuleWidget {
	SP4TStereoOutWidget(SP4TStereoOut* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TStereoOut.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TStereoOut-dark.svg")));

		// 4 HP: two screws, on the left.
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(
			createLightCentered<SP4TSignalLight>(mm2px(Vec(5.713, 30.611)), module, SP4TStereoOut::LEFT_LED_LIGHT));
		addChild(
			createLightCentered<SP4TSignalLight>(mm2px(Vec(14.608, 30.611)), module, SP4TStereoOut::RIGHT_LED_LIGHT));

		addParam(
			createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(10.16, 55.627)), module, SP4TStereoOut::SPREAD_PARAM));
		addParam(
			createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(10.16, 68.142)), module, SP4TStereoOut::SPREAD_CV_PARAM));
		addInput(
			createInputCentered<ThemedPJ301MPort>(mm2px(Vec(10.16, 78.621)), module, SP4TStereoOut::SPREAD_CV_INPUT));

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(10.16, 94.588)), module, SP4TStereoOut::AMBI_INPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(5.745, 114.0)), module, SP4TStereoOut::LEFT_OUTPUT));
		addOutput(
			createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(14.574, 114.0)), module, SP4TStereoOut::RIGHT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		SP4TStereoOut* module = getModule<SP4TStereoOut>();

		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Crude mode (W/Y channels only)", "", &module->crudeMode));
	}
};


Model* modelNun4takSP4TStereoOut = createModel<SP4TStereoOut, SP4TStereoOutWidget>("SP4T-Stereo-Out");
