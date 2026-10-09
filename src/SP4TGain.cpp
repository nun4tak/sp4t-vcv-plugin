#include "plugin.hpp"
#include "components.hpp"

#include <cmath>


struct SP4TGain : Module {
	enum {
		NUM_CHANNELS = 16
	};

	enum ParamId {
		CH1_GAIN_PARAM,
		CH2_GAIN_PARAM,
		CH3_GAIN_PARAM,
		CH4_GAIN_PARAM,
		CH5_GAIN_PARAM,
		CH6_GAIN_PARAM,
		CH7_GAIN_PARAM,
		CH8_GAIN_PARAM,
		CH9_GAIN_PARAM,
		CH10_GAIN_PARAM,
		CH11_GAIN_PARAM,
		CH12_GAIN_PARAM,
		CH13_GAIN_PARAM,
		CH14_GAIN_PARAM,
		CH15_GAIN_PARAM,
		CH16_GAIN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
		CH1_GAIN_CV_INPUT,
		CH2_GAIN_CV_INPUT,
		CH3_GAIN_CV_INPUT,
		CH4_GAIN_CV_INPUT,
		CH5_GAIN_CV_INPUT,
		CH6_GAIN_CV_INPUT,
		CH7_GAIN_CV_INPUT,
		CH8_GAIN_CV_INPUT,
		CH9_GAIN_CV_INPUT,
		CH10_GAIN_CV_INPUT,
		CH11_GAIN_CV_INPUT,
		CH12_GAIN_CV_INPUT,
		CH13_GAIN_CV_INPUT,
		CH14_GAIN_CV_INPUT,
		CH15_GAIN_CV_INPUT,
		CH16_GAIN_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		CH1_LED_LIGHT,
		CH2_LED_LIGHT,
		CH3_LED_LIGHT,
		CH4_LED_LIGHT,
		CH5_LED_LIGHT,
		CH6_LED_LIGHT,
		CH7_LED_LIGHT,
		CH8_LED_LIGHT,
		CH9_LED_LIGHT,
		CH10_LED_LIGHT,
		CH11_LED_LIGHT,
		CH12_LED_LIGHT,
		CH13_LED_LIGHT,
		CH14_LED_LIGHT,
		CH15_LED_LIGHT,
		CH16_LED_LIGHT,
		LIGHTS_LEN
	};

	dsp::ClockDivider lightDivider;

	/** Peak of the output channels since the last light update. */
	float peaks[NUM_CHANNELS] = {};

	SP4TGain() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int c = 0; c < NUM_CHANNELS; c++) {
			std::string channel = string::f("Channel %d", c + 1);
			configParam(CH1_GAIN_PARAM + c, 0.f, 2.f, 1.f, channel + " gain", "x");
			configInput(CH1_GAIN_CV_INPUT + c, channel + " gain CV");
			configLight(CH1_LED_LIGHT + c, channel + " level");
		}
		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");
		configBypass(AMBI_INPUT, AMBI_OUTPUT);

		lightDivider.setDivision(256);
	}

	void process(const ProcessArgs& args) override {
		int channels = inputs[AMBI_INPUT].getChannels();
		outputs[AMBI_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++) {
			// The CV input is normalized to 5 V, so that the gain is unity with
			// nothing patched and the knob at its mid position.
			float gain =
				params[CH1_GAIN_PARAM + c].getValue() * inputs[CH1_GAIN_CV_INPUT + c].getNormalVoltage(5.f) / 5.f;
			float out = inputs[AMBI_INPUT].getVoltage(c) * gain;
			outputs[AMBI_OUTPUT].setVoltage(out, c);
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


struct SP4TGainWidget : ModuleWidget {
	SP4TGainWidget(SP4TGain* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TGain.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TGain-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// The ACN channels are laid out as a 4 x 4 grid, from top-left to
		// bottom-right. Each cell has its gain knob, its CV input below and its
		// LED in the top-left corner.
		const float knobX[] = {8.756f, 19.852f, 30.948f, 42.043f};
		const float knobY[] = {19.509f, 41.369f, 62.89f, 84.492f};
		const float inputY[] = {29.747f, 51.607f, 73.128f, 94.73f};
		const float ledX[] = {4.832f, 15.927f, 27.023f, 38.119f};
		const float ledY[] = {15.831f, 37.668f, 59.2f, 80.961f};

		for (int row = 0; row < 4; row++) {
			for (int col = 0; col < 4; col++) {
				int c = 4 * row + col;
				addParam(createParamCentered<SP4TRoundSmallKnob>(
					mm2px(Vec(knobX[col], knobY[row])), module, SP4TGain::CH1_GAIN_PARAM + c));
				addInput(createInputCentered<ThemedPJ301MPort>(
					mm2px(Vec(knobX[col], inputY[row])), module, SP4TGain::CH1_GAIN_CV_INPUT + c));

				// The LEDs sit close to the knobs and their arcs, so the glow
				// stops right after the rim.
				SP4TSignalLight* light = createLightCentered<SP4TSignalLight>(
					mm2px(Vec(ledX[col], ledY[row])), module, SP4TGain::CH1_LED_LIGHT + c);
				light->haloSpread = 1.5f;
				addChild(light);
			}
		}

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(8.756, 114.0)), module, SP4TGain::AMBI_INPUT));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(42.043, 114.0)), module, SP4TGain::AMBI_OUTPUT));
	}
};


Model* modelNun4takSP4TGain = createModel<SP4TGain, SP4TGainWidget>("SP4T-Gain");
