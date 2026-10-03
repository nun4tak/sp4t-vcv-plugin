#include "plugin.hpp"
#include "components.hpp"

#include <cmath>


struct SP4TSplit : Module {
	enum { NUM_CHANNELS = 16 };

	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		CH1_OUTPUT, CH2_OUTPUT, CH3_OUTPUT, CH4_OUTPUT,
		CH5_OUTPUT, CH6_OUTPUT, CH7_OUTPUT, CH8_OUTPUT,
		CH9_OUTPUT, CH10_OUTPUT, CH11_OUTPUT, CH12_OUTPUT,
		CH13_OUTPUT, CH14_OUTPUT, CH15_OUTPUT, CH16_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		CH1_LED_LIGHT, CH2_LED_LIGHT, CH3_LED_LIGHT, CH4_LED_LIGHT,
		CH5_LED_LIGHT, CH6_LED_LIGHT, CH7_LED_LIGHT, CH8_LED_LIGHT,
		CH9_LED_LIGHT, CH10_LED_LIGHT, CH11_LED_LIGHT, CH12_LED_LIGHT,
		CH13_LED_LIGHT, CH14_LED_LIGHT, CH15_LED_LIGHT, CH16_LED_LIGHT,
		LIGHTS_LEN
	};

	dsp::ClockDivider lightDivider;

	/** Peak of the output channels since the last light update. */
	float peaks[NUM_CHANNELS] = {};

	/** Set while there is nothing to split. */
	bool idle = false;

	SP4TSplit() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int c = 0; c < NUM_CHANNELS; c++) {
			std::string channel = string::f("Channel %d", c + 1);
			configOutput(CH1_OUTPUT + c, channel);
			configLight(CH1_LED_LIGHT + c, channel + " level");
		}
		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");

		lightDivider.setDivision(256);
	}

	void process(const ProcessArgs& args) override {
		int channels = inputs[AMBI_INPUT].getChannels();

		// Nothing to split: zero the outputs and the lights once, then stay out
		// of the way. Routing keeps no state, so there is nothing to ring out.
		if (channels == 0) {
			if (!idle) {
				idle = true;
				for (int c = 0; c < NUM_CHANNELS; c++) {
					outputs[CH1_OUTPUT + c].setVoltage(0.f);
					lights[CH1_LED_LIGHT + c].setBrightness(0.f);
					peaks[c] = 0.f;
				}
			}
			return;
		}
		idle = false;

		// The channels the input does not carry are left at 0 V, as VCV Rack's
		// Split module does.
		for (int c = 0; c < NUM_CHANNELS; c++) {
			float out = c < channels ? inputs[AMBI_INPUT].getVoltage(c) : 0.f;
			outputs[CH1_OUTPUT + c].setVoltage(out);
			peaks[c] = std::fmax(peaks[c], std::fabs(out));
		}

		// The LEDs show the peak of the audio-rate output channels, full scale at
		// 5 V with a cube-root response so that quiet signals still show brightly.
		if (lightDivider.process()) {
			float deltaTime = args.sampleTime * lightDivider.getDivision();
			for (int c = 0; c < NUM_CHANNELS; c++) {
				float brightness = c < channels ? std::cbrt(std::fmin(peaks[c] / 5.f, 1.f)) : 0.f;
				lights[CH1_LED_LIGHT + c].setBrightnessSmooth(brightness, deltaTime);
				peaks[c] = 0.f;
			}
		}
	}
};


struct SP4TSplitWidget : ModuleWidget {
	SP4TSplitWidget(SP4TSplit* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TSplit.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TSplit-dark.svg")));

		// 6 HP: two screws, on the left.
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// The ACN channels are laid out as a grid, from top-left to bottom-right:
		// the omni channel alone on the first row, then five rows of three. Each
		// cell has its spherical harmonic drawn above its output jack, and its
		// LED in the top-left corner, in the gap left of the drawing.
		const float jackX[] = {6.411f, 15.24f, 24.068f};
		const float jackY[] = {31.45f, 44.185f, 56.92f, 69.61f, 82.327f, 95.016f};
		const float ledX[] = {3.009f, 11.893f, 20.733f};
		const float ledY[] = {24.666f, 37.344f, 50.084f, 62.725f, 75.432f, 88.132f};

		for (int c = 0; c < SP4TSplit::NUM_CHANNELS; c++) {
			int row = c == 0 ? 0 : 1 + (c - 1) / 3;
			int col = c == 0 ? 1 : (c - 1) % 3;
			addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(jackX[col], jackY[row])), module, SP4TSplit::CH1_OUTPUT + c));

			// The LEDs sit close to the harmonic drawings and to the edge of
			// their cell, so the glow stops right after the rim.
			SP4TSignalLight* light = createLightCentered<SP4TSignalLight>(mm2px(Vec(ledX[col], ledY[row])), module, SP4TSplit::CH1_LED_LIGHT + c);
			light->haloSpread = 1.5f;
			addChild(light);
		}

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(6.411, 114.0)), module, SP4TSplit::AMBI_INPUT));
	}
};


Model* modelNun4takSP4TSplit = createModel<SP4TSplit, SP4TSplitWidget>("SP4T-Split");
