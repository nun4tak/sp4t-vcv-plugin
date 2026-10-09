#include "plugin.hpp"
#include "components.hpp"

#include <cmath>


struct SP4TMerge : Module {
	enum {
		NUM_CHANNELS = 16
	};

	enum ParamId {
		ORDER_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CH1_INPUT,
		CH2_INPUT,
		CH3_INPUT,
		CH4_INPUT,
		CH5_INPUT,
		CH6_INPUT,
		CH7_INPUT,
		CH8_INPUT,
		CH9_INPUT,
		CH10_INPUT,
		CH11_INPUT,
		CH12_INPUT,
		CH13_INPUT,
		CH14_INPUT,
		CH15_INPUT,
		CH16_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	SP4TMerge() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(ORDER_PARAM,
		             0.f,
		             2.f,
		             0.f,
		             "Ambisonic order",
		             {"1 (FOA, 4 channels)", "2 (HOA-2, 9 channels)", "3 (HOA-3, 16 channels)"});
		for (int c = 0; c < NUM_CHANNELS; c++)
			configInput(CH1_INPUT + c, string::f("Channel %d", c + 1));
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");
	}

	void process(const ProcessArgs& args) override {
		// Routing keeps no state, so with nothing listening there is nothing to
		// do and nothing to ring out.
		if (!outputs[AMBI_OUTPUT].isConnected())
			return;

		// The switch selects the order of the output signal: 1 at the top.
		int order = 1 + (int) std::round(params[ORDER_PARAM].getValue());
		int channels = (order + 1) * (order + 1);

		// setChannels() clears the channels above the selected order, and an
		// unplugged input reads as 0 V, so the missing channels are silent.
		// The inputs beyond the selected order are simply left unread.
		outputs[AMBI_OUTPUT].setChannels(channels);
		for (int c = 0; c < channels; c++)
			outputs[AMBI_OUTPUT].setVoltage(inputs[CH1_INPUT + c].getVoltage(), c);
	}
};


struct SP4TMergeWidget : ModuleWidget {
	SP4TMergeWidget(SP4TMerge* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TMerge.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TMerge-dark.svg")));

		// 6 HP: two screws, on the left.
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// The ACN channels are laid out as a grid, from top-left to bottom-right:
		// the omni channel alone on the first row, then five rows of three, as
		// on the Split panel.
		const float jackX[] = {6.411f, 15.24f, 24.068f};
		const float jackY[] = {31.45f, 44.185f, 56.92f, 69.61f, 82.327f, 95.016f};

		for (int c = 0; c < SP4TMerge::NUM_CHANNELS; c++) {
			int row = c == 0 ? 0 : 1 + (c - 1) / 3;
			int col = c == 0 ? 1 : (c - 1) % 3;
			addInput(createInputCentered<ThemedPJ301MPort>(
				mm2px(Vec(jackX[col], jackY[row])), module, SP4TMerge::CH1_INPUT + c));
		}

		addParam(createParamCentered<SP4TSwitchThree>(mm2px(Vec(13.451, 114.027)), module, SP4TMerge::ORDER_PARAM));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(24.068, 114.0)), module, SP4TMerge::AMBI_OUTPUT));
	}
};


Model* modelNun4takSP4TMerge = createModel<SP4TMerge, SP4TMergeWidget>("SP4T-Merge");
