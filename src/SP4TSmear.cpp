#include "plugin.hpp"
#include "components.hpp"
#include "Decorrelator.hpp"
#include "VelvetDecorrelator.hpp"


/** Frequencies the bass split of the velvet noise engine offers, 0 being off. */
static const float BASS_SPLITS[] = {0.f, 100.f, 200.f, 400.f, 800.f};


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

	/** Which of the two decorrelators the module runs. */
	enum Engine {
		VELVET_ENGINE,
		SPECTRAL_ENGINE,
	};

	sp4t::VelvetDecorrelator velvet;
	sp4t::Decorrelator decorrelator;
	dsp::ClockDivider controlDivider;

	/** Which engine decorrelates, and the frequency below which the velvet
	 * noise one leaves the signal alone. Right-click menu, saved with the
	 * patch. */
	int engine = VELVET_ENGINE;
	float bassSplit = 200.f;
	/** The engine the audio thread last ran, to drop the tail of the other one
	 * when the menu switches between them. */
	int runningEngine = VELVET_ENGINE;

	/** Sends the transients straight through instead of decorrelating them.
	 * Right-click menu, saved with the patch. */
	bool bypassTransients = false;

	/** Scales the decorrelated channels up so that they sum at the level of
	 * the coherent input. Right-click menu, saved with the patch. */
	bool compensateLevel = false;

	/** Set while there is nothing to decorrelate or nowhere to send it. */
	bool idle = false;

	/** Index of `bassSplit` among the frequencies the menu offers. */
	int bassSplitIndex() const {
		for (int i = 0; i < (int) (sizeof(BASS_SPLITS) / sizeof(*BASS_SPLITS)); i++) {
			if (bassSplit == BASS_SPLITS[i])
				return i;
		}
		return 0;
	}

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
		velvet.setSampleRate(e.sampleRate);
		velvet.setBassSplit(bassSplit);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		bypassTransients = false;
		decorrelator.setBypassTransients(false);
		compensateLevel = false;
		decorrelator.setCompensateLevel(false);
		velvet.setCompensateLevel(false);
		engine = VELVET_ENGINE;
		bassSplit = 200.f;
		velvet.setBassSplit(bassSplit);
		decorrelator.reset();
		velvet.reset();
		updateControls();
	}

	void updateControls() {
		// The amount CV is unipolar: its full scale of 10 V spans the whole
		// range of the knob with the attenuator wide open.
		float amount = params[AMOUNT_PARAM].getValue()
		               + params[AMOUNT_CV_PARAM].getValue() * inputs[AMOUNT_CV_INPUT].getVoltage() / 10.f;
		decorrelator.setAmount(amount);
		velvet.setAmount(amount);

		int channels = inputs[POLY_INPUT].getChannels();
		decorrelator.setNumChannels(channels);
		velvet.setNumChannels(channels);
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
				velvet.reset();
			}
			return;
		}
		idle = false;

		int channels = decorrelator.getNumChannels();
		float in[sp4t::Decorrelator::MAX_NUM_CHANNELS];
		float out[sp4t::Decorrelator::MAX_NUM_CHANNELS];
		for (int c = 0; c < channels; c++)
			in[c] = inputs[POLY_INPUT].getVoltage(c);

		// Switched from the menu: what the other engine was still spreading out
		// is dropped rather than played when it is selected again.
		if (engine != runningEngine) {
			runningEngine = engine;
			decorrelator.reset();
			velvet.reset();
		}

		if (engine == VELVET_ENGINE)
			velvet.process(in, out);
		else
			decorrelator.process(in, out);

		for (int c = 0; c < channels; c++)
			outputs[POLY_OUTPUT].setVoltage(out[c], c);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "bypassTransients", json_boolean(bypassTransients));
		json_object_set_new(rootJ, "compensateLevel", json_boolean(compensateLevel));
		json_object_set_new(rootJ, "engine", json_integer(engine));
		json_object_set_new(rootJ, "bassSplit", json_real(bassSplit));
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
			velvet.setCompensateLevel(compensateLevel);
		}
		json_t* engineJ = json_object_get(rootJ, "engine");
		if (engineJ)
			engine = runningEngine = (int) json_integer_value(engineJ);
		json_t* bassSplitJ = json_object_get(rootJ, "bassSplit");
		if (bassSplitJ) {
			bassSplit = json_real_value(bassSplitJ);
			velvet.setBassSplit(bassSplit);
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

		bool velvet = module->engine == SP4TSmear::VELVET_ENGINE;

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem(
			"Decorrelator",
			{"Velvet noise (cheaper, no latency)", "Time-frequency (finer, latency)"},
			[=]() { return module->engine; },
			[=](int index) { module->engine = index; }));

		// Velvet noise smears the bass as readily as the rest, which is what
		// makes a decorrelator sound loose, so it is held back below here.
		MenuItem* split = createIndexSubmenuItem(
			"Bass split",
			{"Off", "100 Hz", "200 Hz", "400 Hz", "800 Hz"},
			[=]() { return module->bassSplitIndex(); },
			[=](int index) {
				module->bassSplit = BASS_SPLITS[index];
				module->velvet.setBassSplit(module->bassSplit);
			});
		split->disabled = !velvet;
		menu->addChild(split);

		MenuItem* transients = createBoolMenuItem(
			"Bypass transients",
			"",
			[=]() { return module->bypassTransients; },
			[=](bool enabled) {
				module->bypassTransients = enabled;
				module->decorrelator.setBypassTransients(enabled);
			});
		transients->disabled = velvet;
		menu->addChild(transients);

		menu->addChild(createBoolMenuItem(
			"Compensate level",
			"",
			[=]() { return module->compensateLevel; },
			[=](bool enabled) {
				module->compensateLevel = enabled;
				module->decorrelator.setCompensateLevel(enabled);
				module->velvet.setCompensateLevel(enabled);
			}));

		int latency = velvet ? module->velvet.getLatency() : module->decorrelator.getLatency();
		float milliseconds = 1e3f * latency / APP->engine->getSampleRate();
		menu->addChild(createMenuLabel(string::f("Latency: %d samples (%.1f ms)", latency, milliseconds)));
	}
};


Model* modelNun4takSP4TSmear = createModel<SP4TSmear, SP4TSmearWidget>("SP4T-Smear");
