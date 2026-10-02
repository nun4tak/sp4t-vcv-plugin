#include "plugin.hpp"
#include "components.hpp"

#include <cmath>


namespace {

/** Fade of a channel when it is muted or unmuted, in gain units per second
 * (5 ms, short enough to feel immediate and long enough to avoid clicks). */
const float MUTE_FADE_RATE = 1.f / 0.005f;

} // namespace


struct SP4TMute : Module {
	enum { NUM_CHANNELS = 16 };

	enum ParamId {
		CH1_MUTE_BUTTON_PARAM, CH2_MUTE_BUTTON_PARAM, CH3_MUTE_BUTTON_PARAM, CH4_MUTE_BUTTON_PARAM,
		CH5_MUTE_BUTTON_PARAM, CH6_MUTE_BUTTON_PARAM, CH7_MUTE_BUTTON_PARAM, CH8_MUTE_BUTTON_PARAM,
		CH9_MUTE_BUTTON_PARAM, CH10_MUTE_BUTTON_PARAM, CH11_MUTE_BUTTON_PARAM, CH12_MUTE_BUTTON_PARAM,
		CH13_MUTE_BUTTON_PARAM, CH14_MUTE_BUTTON_PARAM, CH15_MUTE_BUTTON_PARAM, CH16_MUTE_BUTTON_PARAM,
		TRIGGER_MODE_SWITCH_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
		CH1_MUTE_INPUT, CH2_MUTE_INPUT, CH3_MUTE_INPUT, CH4_MUTE_INPUT,
		CH5_MUTE_INPUT, CH6_MUTE_INPUT, CH7_MUTE_INPUT, CH8_MUTE_INPUT,
		CH9_MUTE_INPUT, CH10_MUTE_INPUT, CH11_MUTE_INPUT, CH12_MUTE_INPUT,
		CH13_MUTE_INPUT, CH14_MUTE_INPUT, CH15_MUTE_INPUT, CH16_MUTE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		CH1_LED_LIGHT, CH2_LED_LIGHT, CH3_LED_LIGHT, CH4_LED_LIGHT,
		CH5_LED_LIGHT, CH6_LED_LIGHT, CH7_LED_LIGHT, CH8_LED_LIGHT,
		CH9_LED_LIGHT, CH10_LED_LIGHT, CH11_LED_LIGHT, CH12_LED_LIGHT,
		CH13_LED_LIGHT, CH14_LED_LIGHT, CH15_LED_LIGHT, CH16_LED_LIGHT,
		LIGHTS_LEN
	};

	dsp::ClockDivider controlDivider;
	dsp::ClockDivider lightDivider;
	dsp::SchmittTrigger muteTriggers[NUM_CHANNELS];

	/** Number of channels of the input signal, read at control rate. */
	int numChannels = 0;
	/** Gain of each channel and the value it fades to, 0 when muted. */
	float gains[NUM_CHANNELS] = {};
	float gainTargets[NUM_CHANNELS] = {};
	/** Whether a channel is fading, i.e., whether the fades must be advanced. */
	bool fading = false;
	/** State of the mute CV gates, to act on their edges only. */
	bool gateHigh[NUM_CHANNELS] = {};
	/** Peak of the output channels since the last light update. */
	float peaks[NUM_CHANNELS] = {};

	SP4TMute() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int c = 0; c < NUM_CHANNELS; c++) {
			std::string channel = string::f("Channel %d", c + 1);
			configSwitch(CH1_MUTE_BUTTON_PARAM + c, 0.f, 1.f, 0.f, channel + " mute", {"Unmuted", "Muted"});
			configInput(CH1_MUTE_INPUT + c, channel + " mute gate/trigger");
			configLight(CH1_LED_LIGHT + c, channel + " level");
		}
		configSwitch(TRIGGER_MODE_SWITCH_PARAM, 0.f, 1.f, 1.f, "Buttons and mute CV",
		             {"Momentary (gate)", "Latching (trigger)"});

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");
		configBypass(AMBI_INPUT, AMBI_OUTPUT);

		controlDivider.setDivision(16);
		lightDivider.setDivision(256);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int c = 0; c < NUM_CHANNELS; c++) {
			muteTriggers[c].reset();
			gateHigh[c] = false;
		}
		// Lets updateControls() set the gains of the channels in use again.
		numChannels = 0;
		updateControls();
	}

	/** Reads the buttons and the mute CV inputs of every channel. Mute controls
	 * do not need audio-rate accuracy: a division of 16 samples is well under
	 * the 1 ms of the shortest trigger of the voltage standard. */
	void updateControls() {
		bool latching = params[TRIGGER_MODE_SWITCH_PARAM].getValue() > 0.5f;

		for (int c = 0; c < NUM_CHANNELS; c++) {
			Param& button = params[CH1_MUTE_BUTTON_PARAM + c];
			bool triggered = muteTriggers[c].process(inputs[CH1_MUTE_INPUT + c].getVoltage(), 0.1f, 1.f);
			bool high = muteTriggers[c].isHigh();

			// The CV sets the state of the button, as a click on it does: a
			// trigger toggles it in latching mode, a gate follows it in
			// momentary mode.
			if (latching) {
				if (triggered)
					button.setValue(button.getValue() > 0.5f ? 0.f : 1.f);
			}
			else if (high != gateHigh[c]) {
				button.setValue(high ? 1.f : 0.f);
			}
			gateHigh[c] = high;

			gainTargets[c] = button.getValue() > 0.5f ? 0.f : 1.f;
		}

		// Channels that the input signal starts carrying are not faded in: their
		// gain was left where it was when they stopped being processed.
		int channels = inputs[AMBI_INPUT].getChannels();
		for (int c = numChannels; c < channels; c++)
			gains[c] = gainTargets[c];
		numChannels = channels;
		outputs[AMBI_OUTPUT].setChannels(numChannels);

		fading = false;
		for (int c = 0; c < numChannels; c++)
			fading = fading || gains[c] != gainTargets[c];
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		// Advancing the fades is skipped while no channel is fading, which is
		// most of the time, and only the channels carried by the input signal
		// are processed.
		if (fading) {
			float step = args.sampleTime * MUTE_FADE_RATE;
			for (int c = 0; c < numChannels; c++) {
				float target = gainTargets[c];
				gains[c] = gains[c] < target ? std::fmin(gains[c] + step, target)
				                             : std::fmax(gains[c] - step, target);
			}
		}

		for (int c = 0; c < numChannels; c++) {
			float out = inputs[AMBI_INPUT].getVoltage(c) * gains[c];
			outputs[AMBI_OUTPUT].setVoltage(out, c);
			peaks[c] = std::fmax(peaks[c], std::fabs(out));
		}

		// The LEDs show the peak of the audio-rate output channels, full scale at
		// 5 V with a cube-root response so that quiet signals still show brightly.
		if (lightDivider.process()) {
			float deltaTime = args.sampleTime * lightDivider.getDivision();
			for (int c = 0; c < NUM_CHANNELS; c++) {
				float brightness = c < numChannels ? std::cbrt(std::fmin(peaks[c] / 5.f, 1.f)) : 0.f;
				lights[CH1_LED_LIGHT + c].setBrightnessSmooth(brightness, deltaTime);
				peaks[c] = 0.f;
			}
		}
	}
};


struct SP4TMuteWidget : ModuleWidget {
	SP4TMuteWidget(SP4TMute* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TMute.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TMute-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// The ACN channels are laid out as a 4 x 4 grid, from top-left to
		// bottom-right. Each cell has its mute button, its CV input below and its
		// LED in the top-left corner.
		const float buttonX[] = {8.757f, 19.852f, 30.948f, 42.044f};
		const float buttonY[] = {19.509f, 41.369f, 62.89f, 84.492f};
		const float inputX[] = {8.756f, 19.852f, 30.948f, 42.043f};
		const float inputY[] = {29.747f, 51.607f, 73.128f, 94.73f};
		const float ledX[] = {4.832f, 15.927f, 27.023f, 38.119f};
		const float ledY[] = {15.831f, 37.668f, 59.2f, 80.961f};

		for (int row = 0; row < 4; row++) {
			for (int col = 0; col < 4; col++) {
				int c = 4 * row + col;

				SP4TModeButton* button = createParamCentered<SP4TModeButton>(mm2px(Vec(buttonX[col], buttonY[row])), module, SP4TMute::CH1_MUTE_BUTTON_PARAM + c);
				button->modeParamId = SP4TMute::TRIGGER_MODE_SWITCH_PARAM;
				addParam(button);

				addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(inputX[col], inputY[row])), module, SP4TMute::CH1_MUTE_INPUT + c));

				SP4TSignalLight* light = createLightCentered<SP4TSignalLight>(mm2px(Vec(ledX[col], ledY[row])), module, SP4TMute::CH1_LED_LIGHT + c);
				light->haloSpread = 1.5f;
				addChild(light);
			}
		}

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(8.756, 114.0)), module, SP4TMute::AMBI_INPUT));
		addParam(createParamCentered<SP4TSwitchTwo>(mm2px(Vec(19.852, 114.001)), module, SP4TMute::TRIGGER_MODE_SWITCH_PARAM));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(42.043, 114.0)), module, SP4TMute::AMBI_OUTPUT));
	}
};


Model* modelNun4takSP4TMute = createModel<SP4TMute, SP4TMuteWidget>("SP4T-Mute");
