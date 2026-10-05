#include "plugin.hpp"
#include "components.hpp"
#include "Decorrelator.hpp"


struct SP4TSmear : Module {
	enum ParamId {
		AMOUNT_PARAM,
		AMOUNT_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		POLY_INPUT,
		AMOUNT_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		POLY_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	sp4t::Decorrelator decorrelator;
	dsp::ClockDivider controlDivider;

	/** Sends the transients straight through instead of decorrelating them.
	 * Right-click menu, saved with the patch. */
	bool bypassTransients = false;

	/** Scales the decorrelated channels up so that they sum at the level of
	 * the coherent input. Right-click menu, saved with the patch. */
	bool compensateLevel = false;

	/** Set while there is nothing to decorrelate or nowhere to send it. */
	bool idle = false;

	SP4TSmear() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(AMOUNT_PARAM, 0.f, 1.f, 0.5f, "Decorrelation amount", "%", 0.f, 100.f);
		// Attenuator and not attenuverter: the amount CV is unipolar.
		configParam(AMOUNT_CV_PARAM, 0.f, 1.f, 0.f, "Amount CV attenuator", "%", 0.f, 100.f);

		configInput(POLY_INPUT, "Polyphonic");
		configInput(AMOUNT_CV_INPUT, "Amount CV");
		configOutput(POLY_OUTPUT, "Polyphonic");
		configBypass(POLY_INPUT, POLY_OUTPUT);

		controlDivider.setDivision(16);
		updateControls();
	}

	/** The filterbank and the all-pass filters depend on the sample rate.
	 * Rack also sends this event when the module is added, which is where they
	 * are built: it allocates, so it cannot happen in process(). */
	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		decorrelator.setSampleRate(e.sampleRate);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		bypassTransients = false;
		decorrelator.setBypassTransients(false);
		compensateLevel = false;
		decorrelator.setCompensateLevel(false);
		decorrelator.reset();
		updateControls();
	}

	void updateControls() {
		// The amount CV is unipolar: its full scale of 10 V spans the whole
		// range of the knob with the attenuator wide open.
		float amount = params[AMOUNT_PARAM].getValue()
		               + params[AMOUNT_CV_PARAM].getValue() * inputs[AMOUNT_CV_INPUT].getVoltage() / 10.f;
		decorrelator.setAmount(amount);

		int channels = inputs[POLY_INPUT].getChannels();
		decorrelator.setNumChannels(channels);
		outputs[POLY_OUTPUT].setChannels(channels);
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		// Nothing to decorrelate, or nowhere to send it. What is still inside
		// the latency of the decorrelator is dropped rather than left to ring
		// out, so that reconnecting starts from silence.
		if (!inputs[POLY_INPUT].isConnected() || !outputs[POLY_OUTPUT].isConnected()) {
			if (!idle) {
				idle = true;
				decorrelator.reset();
			}
			return;
		}
		idle = false;

		int channels = decorrelator.getNumChannels();
		float in[sp4t::Decorrelator::MAX_NUM_CHANNELS];
		float out[sp4t::Decorrelator::MAX_NUM_CHANNELS];
		for (int c = 0; c < channels; c++)
			in[c] = inputs[POLY_INPUT].getVoltage(c);

		decorrelator.process(in, out);

		for (int c = 0; c < channels; c++)
			outputs[POLY_OUTPUT].setVoltage(out[c], c);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "bypassTransients", json_boolean(bypassTransients));
		json_object_set_new(rootJ, "compensateLevel", json_boolean(compensateLevel));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* bypassTransientsJ = json_object_get(rootJ, "bypassTransients");
		if (bypassTransientsJ) {
			bypassTransients = json_boolean_value(bypassTransientsJ);
			decorrelator.setBypassTransients(bypassTransients);
		}
		json_t* compensateLevelJ = json_object_get(rootJ, "compensateLevel");
		if (compensateLevelJ) {
			compensateLevel = json_boolean_value(compensateLevelJ);
			decorrelator.setCompensateLevel(compensateLevel);
		}
	}
};


struct SP4TSmearWidget : ModuleWidget {
	SP4TSmearWidget(SP4TSmear* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TSmear.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TSmear-dark.svg")));

		// 4 HP: two screws, on the left.
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(10.16, 41.418)), module, SP4TSmear::AMOUNT_PARAM));
		addParam(createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(10.16, 69.57)), module, SP4TSmear::AMOUNT_CV_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(10.16, 80.049)), module, SP4TSmear::AMOUNT_CV_INPUT));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(5.745, 114.0)), module, SP4TSmear::POLY_INPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(14.574, 114.0)), module, SP4TSmear::POLY_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		SP4TSmear* module = getModule<SP4TSmear>();

		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolMenuItem("Bypass transients", "",
			[=]() { return module->bypassTransients; },
			[=](bool enabled) {
				module->bypassTransients = enabled;
				module->decorrelator.setBypassTransients(enabled);
			}));
		menu->addChild(createBoolMenuItem("Compensate level", "",
			[=]() { return module->compensateLevel; },
			[=](bool enabled) {
				module->compensateLevel = enabled;
				module->decorrelator.setCompensateLevel(enabled);
			}));

		int latency = module->decorrelator.getLatency();
		float milliseconds = 1e3f * latency / APP->engine->getSampleRate();
		menu->addChild(createMenuLabel(string::f("Latency: %d samples (%.1f ms)",
		                                        latency, milliseconds)));
	}
};


Model* modelNun4takSP4TSmear = createModel<SP4TSmear, SP4TSmearWidget>("SP4T-Smear");
