#include "plugin.hpp"
#include "components.hpp"
#include "AmbiBinauralDecoder.hpp"

#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>


struct SP4TEarsOut : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
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

	using Decoder = sp4t::AmbiBinauralDecoder;

	Decoder decoder;
	Decoder::Options options;
	dsp::ClockDivider controlDivider;
	dsp::ClockDivider lightDivider;

	/** Peak of the output signals since the last light update. */
	float peaks[2] = {0.f, 0.f};

	/** Set while the convolution is skipped, i.e. while there is nothing to
	 * decode or nowhere to send it. It costs the same whatever the input. */
	bool idle = false;
	/** Samples of filter tail left to decode before going idle. */
	int flushSamples = 0;

	// Computing the decoding filters takes about a second, so it is left to a
	// worker thread; the module outputs silence until the first set is ready
	// and keeps decoding with the previous one while a new one is computed.
	std::thread worker;
	std::mutex workerMutex;
	std::condition_variable workerCv;
	Decoder::Options workerOptions;
	bool workerDirty = false;
	bool workerQuit = false;

	SP4TEarsOut() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
		configLight(LEFT_LED_LIGHT, "Left level");
		configLight(RIGHT_LED_LIGHT, "Right level");

		controlDivider.setDivision(16);
		lightDivider.setDivision(256);

		// The filters are built when the module is added to the engine, i.e.
		// once onSampleRateChange() has given the worker a sample rate.
		worker = std::thread(&SP4TEarsOut::workerLoop, this);
	}

	~SP4TEarsOut() {
		{
			std::lock_guard<std::mutex> lock(workerMutex);
			workerQuit = true;
		}
		workerCv.notify_one();
		worker.join();
	}

	void workerLoop() {
		std::unique_lock<std::mutex> lock(workerMutex);
		while (true) {
			workerCv.wait(lock, [this] { return workerQuit || workerDirty; });
			if (workerQuit)
				break;
			Decoder::Options pending = workerOptions;
			workerDirty = false;
			lock.unlock();
			decoder.build(pending);
			lock.lock();
		}
	}

	void requestBuild() {
		{
			std::lock_guard<std::mutex> lock(workerMutex);
			workerOptions = options;
			workerDirty = true;
		}
		workerCv.notify_one();
	}

	/** Replaces the decoding options, recomputing the filters if they changed. */
	void setOptions(const Decoder::Options& newOptions) {
		if (newOptions == options)
			return;
		options = newOptions;
		requestBuild();
	}

	/** Also called when the module is added to the engine, i.e. this is where
	 * the first set of decoding filters is requested. */
	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		options.sampleRate = e.sampleRate;
		requestBuild();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		Decoder::Options defaults;
		defaults.sampleRate = options.sampleRate;
		setOptions(defaults);
		decoder.reset();
	}

	void process(const ProcessArgs& args) override {
		bool connected = inputs[AMBI_INPUT].isConnected()
		                 && (outputs[LEFT_OUTPUT].isConnected() || outputs[RIGHT_OUTPUT].isConnected());

		// Keep decoding until the tail of the filters has rung out, so that
		// unplugging a cable does not cut the output short. Rack clears the
		// voltages of a disconnected input, so the tail decays to silence.
		if (connected)
			flushSamples = Decoder::MAX_FILTER_LEN;
		else if (flushSamples > 0)
			flushSamples--;

		if (!connected && flushSamples == 0) {
			if (!idle) {
				idle = true;
				decoder.reset();
				for (int i = 0; i < 2; i++) {
					peaks[i] = 0.f;
					lights[LEFT_LED_LIGHT + i].setBrightness(0.f);
				}
			}
			outputs[LEFT_OUTPUT].setVoltage(0.f);
			outputs[RIGHT_OUTPUT].setVoltage(0.f);
			return;
		}

		// `idle` also updates the order at once when a cable has just been
		// plugged, instead of waiting for the next control tick.
		if (controlDivider.process() || idle) {
			// The order of the input signal is given by the number of channels of
			// the polyphonic cable, i.e., 4 (FOA), 9 (HOA-2) or 16 (HOA-3).
			int channels = inputs[AMBI_INPUT].getChannels();
			decoder.setOrder((int) std::floor(std::sqrt((float) channels)) - 1);
		}
		idle = false;

		float in[Decoder::MAX_NUM_SH];
		for (int c = 0; c < decoder.getNumSH(); c++)
			in[c] = inputs[AMBI_INPUT].getVoltage(c);

		float out[Decoder::NUM_CHANNELS];
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
		json_object_set_new(rootJ, "method", json_integer(options.method));
		json_object_set_new(rootJ, "maxRE", json_boolean(options.maxRE));
		json_object_set_new(rootJ, "diffuseMatching", json_boolean(options.diffuseMatching));
		json_object_set_new(rootJ, "truncationEQ", json_boolean(options.truncationEQ));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		Decoder::Options newOptions = options;

		json_t* methodJ = json_object_get(rootJ, "method");
		if (methodJ) {
			int method = (int) json_integer_value(methodJ);
			if (method >= 0 && method < Decoder::NUM_METHODS)
				newOptions.method = (Decoder::Method) method;
		}
		json_t* maxREJ = json_object_get(rootJ, "maxRE");
		if (maxREJ)
			newOptions.maxRE = json_boolean_value(maxREJ);
		json_t* diffuseMatchingJ = json_object_get(rootJ, "diffuseMatching");
		if (diffuseMatchingJ)
			newOptions.diffuseMatching = json_boolean_value(diffuseMatchingJ);
		json_t* truncationEQJ = json_object_get(rootJ, "truncationEQ");
		if (truncationEQJ)
			newOptions.truncationEQ = json_boolean_value(truncationEQJ);

		setOptions(newOptions);
	}
};


struct SP4TEarsOutWidget : ModuleWidget {
	SP4TEarsOutWidget(SP4TEarsOut* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TEarsOut.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TEarsOut-dark.svg")));

		// 4 HP: two screws, on the left.
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<SP4TSignalLight>(mm2px(Vec(4.742, 30.152)), module, SP4TEarsOut::LEFT_LED_LIGHT));
		addChild(
			createLightCentered<SP4TSignalLight>(mm2px(Vec(15.578, 30.152)), module, SP4TEarsOut::RIGHT_LED_LIGHT));

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(10.16, 94.588)), module, SP4TEarsOut::AMBI_INPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(5.745, 114.0)), module, SP4TEarsOut::LEFT_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(14.574, 114.0)), module, SP4TEarsOut::RIGHT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		SP4TEarsOut* module = getModule<SP4TEarsOut>();

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem(
			"Decoding method",
			{"Least-squares",
			 "Least-squares + diffuse-field EQ",
			 "Spatial resampling",
			 "Time-alignment",
			 "Magnitude least-squares"},
			[=]() { return (int) module->options.method; },
			[=](int index) {
				SP4TEarsOut::Decoder::Options options = module->options;
				options.method = (SP4TEarsOut::Decoder::Method) index;
				module->setOptions(options);
			}));

		menu->addChild(createBoolMenuItem(
			"Max-rE weighting",
			"",
			[=]() { return module->options.maxRE; },
			[=](bool enabled) {
				SP4TEarsOut::Decoder::Options options = module->options;
				options.maxRE = enabled;
				module->setOptions(options);
			}));

		menu->addChild(createBoolMenuItem(
			"Diffuse-field covariance matching",
			"",
			[=]() { return module->options.diffuseMatching; },
			[=](bool enabled) {
				SP4TEarsOut::Decoder::Options options = module->options;
				options.diffuseMatching = enabled;
				module->setOptions(options);
			}));

		menu->addChild(createBoolMenuItem(
			"Truncation EQ (least-squares only)",
			"",
			[=]() { return module->options.truncationEQ; },
			[=](bool enabled) {
				SP4TEarsOut::Decoder::Options options = module->options;
				options.truncationEQ = enabled;
				module->setOptions(options);
			}));

		menu->addChild(new MenuSeparator);
		if (module->decoder.isReady()) {
			int latency = module->decoder.getLatency();
			float milliseconds = 1e3f * latency / APP->engine->getSampleRate();
			menu->addChild(createMenuLabel(string::f("Latency: %d samples (%.2f ms)", latency, milliseconds)));
		}
		else {
			menu->addChild(createMenuLabel("Computing the decoding filters..."));
		}
	}
};


Model* modelNun4takSP4TEarsOut = createModel<SP4TEarsOut, SP4TEarsOutWidget>("SP4T-Ears-Out");
