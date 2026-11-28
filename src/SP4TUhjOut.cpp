#include "plugin.hpp"
#include "components.hpp"
#include "UhjEncoder.hpp"

#include <cmath>


namespace {

/** How long the allpass chains keep being run after the last cable has gone,
 * in seconds. Their tail is down by 60 dB after about 0.4 s. */
const float FLUSH_TIME = 0.5f;

} // namespace


struct SP4TUhjOut : Module {
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

	sp4t::UhjEncoder encoder;
	dsp::ClockDivider lightDivider;

	/** Peak of the output signals since the last light update. */
	float peaks[2] = {0.f, 0.f};

	/** Set while the encoding is skipped, i.e. while there is nothing to
	 * encode. */
	bool idle = false;
	/** Samples of the allpass tail left to encode before going idle. */
	int flushSamples = 0;
	int flushLength = 0;

	SP4TUhjOut() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configOutput(LEFT_OUTPUT, "UHJ left");
		configOutput(RIGHT_OUTPUT, "UHJ right");
		configLight(LEFT_LED_LIGHT, "Left level");
		configLight(RIGHT_LED_LIGHT, "Right level");

		lightDivider.setDivision(256);
	}

	/** Also called when the module is added to the engine, i.e. this is where
	 * the phase-shift network is first designed. */
	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		encoder.setSampleRate(e.sampleRate);
		flushLength = (int) (FLUSH_TIME * e.sampleRate);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		encoder.reset();
	}

	void process(const ProcessArgs& args) override {
		// Encoding starts as soon as there is something to encode, whether or
		// not an output is patched, so that the LEDs answer the input. This is
		// what every decoder of the plugin does.
		bool connected = inputs[AMBI_INPUT].isConnected();

		// Keep encoding until the tail of the allpass chains has rung out, so
		// that unplugging a cable does not cut the output short. Rack clears
		// the voltages of a disconnected input, so the tail decays to silence.
		if (connected)
			flushSamples = flushLength;
		else if (flushSamples > 0)
			flushSamples--;

		if (!connected && flushSamples == 0) {
			if (!idle) {
				idle = true;
				encoder.reset();
				for (int i = 0; i < 2; i++) {
					peaks[i] = 0.f;
					lights[LEFT_LED_LIGHT + i].setBrightness(0.f);
				}
			}
			outputs[LEFT_OUTPUT].setVoltage(0.f);
			outputs[RIGHT_OUTPUT].setVoltage(0.f);
			return;
		}
		idle = false;

		// Only the first-order horizontal components are encoded, so the order
		// of the input signal does not matter: the channels beyond the first
		// four are left out, and any of those four the cable does not carry
		// reads as zero.
		float in[sp4t::UhjEncoder::NUM_SH] = {};
		for (int c = 0; c < sp4t::UhjEncoder::NUM_SH; c++)
			in[c] = inputs[AMBI_INPUT].getVoltage(c);

		float out[sp4t::UhjEncoder::NUM_CHANNELS];
		encoder.process(in, out);

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
};


struct SP4TUhjOutWidget : ModuleWidget {
	SP4TUhjOutWidget(SP4TUhjOut* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TUhjOut.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TUhjOut-dark.svg")));

		// 4 HP: two screws, on the left.
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<SP4TSignalLight>(mm2px(Vec(6.261, 20.681)), module, SP4TUhjOut::LEFT_LED_LIGHT));
		addChild(createLightCentered<SP4TSignalLight>(mm2px(Vec(14.125, 20.681)), module, SP4TUhjOut::RIGHT_LED_LIGHT));

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(10.16, 94.588)), module, SP4TUhjOut::AMBI_INPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(5.745, 114.0)), module, SP4TUhjOut::LEFT_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(14.574, 114.0)), module, SP4TUhjOut::RIGHT_OUTPUT));
	}
};


Model* modelNun4takSP4TUhjOut = createModel<SP4TUhjOut, SP4TUhjOutWidget>("SP4T-Uhj-Out");
