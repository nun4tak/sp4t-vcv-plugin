#include "plugin.hpp"
#include "components.hpp"
#include "AmbiEncoder.hpp"
#include "AmbisonicDisplay.hpp"
#include "SphericalCoordinates.hpp"


namespace {

// Colors of the sources on the screen (Ayu dark), the active one being the color
// of the mono source of the Stereo In module.
const NVGcolor SOURCE_ACTIVE = nvgRGB(0xff, 0xb4, 0x54);
const NVGcolor SOURCE_INACTIVE = nvgRGB(0x56, 0x5b, 0x66);

} // namespace


struct SP4TSource4In : Module {
	enum ParamId {
		SRC1_AZ_PARAM,
		SRC2_AZ_PARAM,
		SRC3_AZ_PARAM,
		SRC4_AZ_PARAM,
		SRC1_ELEV_PARAM,
		SRC2_ELEV_PARAM,
		SRC3_ELEV_PARAM,
		SRC4_ELEV_PARAM,
		ORDER_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		SRC1_AZ_CV_INPUT,
		SRC2_AZ_CV_INPUT,
		SRC3_AZ_CV_INPUT,
		SRC4_AZ_CV_INPUT,
		SRC1_ELEV_CV_INPUT,
		SRC2_ELEV_CV_INPUT,
		SRC3_ELEV_CV_INPUT,
		SRC4_ELEV_CV_INPUT,
		SRC1_INPUT,
		SRC2_INPUT,
		SRC3_INPUT,
		SRC4_INPUT,
		OMNI_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	enum { NUM_SOURCES = 4 };

	sp4t::AmbiEncoder encoder;
	dsp::ClockDivider controlDivider;

	// Source directions and connection states shown on the screen, updated by
	// the engine thread.
	sp4t::Direction directions[NUM_SOURCES] = {};
	bool connected[NUM_SOURCES] = {};

	// The encoder takes a contiguous range of sources, so the connected ones are
	// packed into its first `numActiveSources` slots.
	int sourceOfSlot[NUM_SOURCES] = {};
	int numActiveSources = 0;

	bool flipAzimuth = false;
	bool idle = false;

	/** sp4t::AmbisonicDisplay::ViewMode */
	int viewMode = sp4t::AmbisonicDisplay::VIEW_XY_Z_PROJECTIONS;

	SP4TSource4In() : encoder(NUM_SOURCES) {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_SOURCES; i++) {
			std::string source = string::f("Source %d ", i + 1);
			configParam(SRC1_AZ_PARAM + i, -180.f, 180.f, 0.f, source + "azimuth", "°");
			configParam(SRC1_ELEV_PARAM + i, -90.f, 90.f, 0.f, source + "elevation", "°");
			configInput(SRC1_AZ_CV_INPUT + i, source + "azimuth CV");
			configInput(SRC1_ELEV_CV_INPUT + i, source + "elevation CV");
			configInput(SRC1_INPUT + i, string::f("Source %d", i + 1));
		}
		configSwitch(ORDER_PARAM, 0.f, 2.f, 0.f, "Ambisonic order", {"1st (FOA)", "2nd (HOA-2)", "3rd (HOA-3)"});
		configInput(OMNI_INPUT, "Omni-directional");
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");

		// The sources are independent signals and are summed as such: the
		// encoder's power normalization would change the level of the other
		// sources whenever one is plugged or unplugged.
		encoder.setPostScaling(false);

		controlDivider.setDivision(16);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		viewMode = sp4t::AmbisonicDisplay::VIEW_XY_Z_PROJECTIONS;
		flipAzimuth = false;
		encoder.reset();
		updateControls();
	}

	/** Knob value plus the CV voltage in force, where 5 V spans `range` degrees. */
	float getAngle(int paramId, float cv, float range) {
		return params[paramId].getValue() + cv / 5.f * range;
	}

	void updateControls() {
		encoder.setOrder((int) params[ORDER_PARAM].getValue() + 1);

		// The AZ and ELEV CV inputs are normalled from left to right (sources 1
		// to 4): an unpatched one takes the voltage of the nearest patched jack
		// of its row on its left, and 0 V when there is none.
		float azCv = 0.f;
		float elevCv = 0.f;

		numActiveSources = 0;
		for (int i = 0; i < NUM_SOURCES; i++) {
			if (inputs[SRC1_AZ_CV_INPUT + i].isConnected())
				azCv = inputs[SRC1_AZ_CV_INPUT + i].getVoltage();
			if (inputs[SRC1_ELEV_CV_INPUT + i].isConnected())
				elevCv = inputs[SRC1_ELEV_CV_INPUT + i].getVoltage();

			// Beyond +/-180 degrees, the azimuth wraps around. The elevation
			// doesn't go past the poles (the rear is reached with the azimuth).
			float azimuth = getAngle(SRC1_AZ_PARAM + i, azCv, 180.f);
			if (flipAzimuth)
				azimuth = -azimuth;
			float elevation = math::clamp(getAngle(SRC1_ELEV_PARAM + i, elevCv, 90.f), -90.f, 90.f);

			directions[i] = sp4t::vectorToDirection(sp4t::directionToVector(azimuth, elevation));
			connected[i] = inputs[SRC1_INPUT + i].isConnected();
			if (connected[i])
				sourceOfSlot[numActiveSources++] = i;
		}

		encoder.setNumSources(numActiveSources);
		for (int s = 0; s < numActiveSources; s++) {
			const sp4t::Direction& direction = directions[sourceOfSlot[s]];
			encoder.setSourceDirection(s, direction.azimuth, direction.elevation);
		}
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		// Nothing to encode, or nowhere to send it. The interpolation is reset so
		// that reconnecting starts from the directions in force rather than
		// gliding from stale gains.
		if (!outputs[AMBI_OUTPUT].isConnected()
		    || (numActiveSources == 0 && !inputs[OMNI_INPUT].isConnected())) {
			if (!idle) {
				idle = true;
				encoder.reset();
				outputs[AMBI_OUTPUT].setChannels(0);
			}
			return;
		}
		idle = false;

		float out[sp4t::AmbiEncoder::MAX_NUM_SH] = {};
		if (numActiveSources > 0) {
			float in[NUM_SOURCES];
			for (int s = 0; s < numActiveSources; s++)
				in[s] = inputs[SRC1_INPUT + sourceOfSlot[s]].getVoltageSum();
			encoder.process(in, out);
		}
		// An omni-directional source feeds the omni channel (ACN 0) only, with
		// the gain an encoded point source has on it in SN3D.
		out[0] += inputs[OMNI_INPUT].getVoltageSum();

		int channels = encoder.getNumSH();
		outputs[AMBI_OUTPUT].setChannels(channels);
		for (int c = 0; c < channels; c++)
			outputs[AMBI_OUTPUT].setVoltage(out[c], c);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "viewMode", json_integer(viewMode));
		json_object_set_new(rootJ, "flipAzimuth", json_boolean(flipAzimuth));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* viewModeJ = json_object_get(rootJ, "viewMode");
		if (viewModeJ)
			viewMode = math::clamp((int) json_integer_value(viewModeJ), 0, sp4t::AmbisonicDisplay::VIEW_MODES_LEN - 1);
		json_t* flipAzimuthJ = json_object_get(rootJ, "flipAzimuth");
		if (flipAzimuthJ)
			flipAzimuth = json_boolean_value(flipAzimuthJ);
	}
};


struct SP4TSource4InWidget : ModuleWidget {
	sp4t::AmbisonicDisplay* display;

	SP4TSource4InWidget(SP4TSource4In* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TSource4In.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TSource4In-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		display = createWidget<sp4t::AmbisonicDisplay>(mm2px(Vec(1.728, 11.5)));
		display->box.size = mm2px(Vec(47.344, 34.039));
		addChild(display);

		// One column per source, left to right.
		static const float x[SP4TSource4In::NUM_SOURCES] = {8.756f, 19.852f, 30.948f, 42.043f};
		for (int i = 0; i < SP4TSource4In::NUM_SOURCES; i++) {
			// Also the screen of the module browser, where step() has no module
			// to read: the sources sit at their default direction, unconnected.
			display->sources.push_back({{0.f, 0.f}, string::f("%d", i + 1), SOURCE_INACTIVE});

			addParam(createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(x[i], 53.39)), module, SP4TSource4In::SRC1_AZ_PARAM + i));
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(x[i], 62.205)), module, SP4TSource4In::SRC1_AZ_CV_INPUT + i));
			addParam(createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(x[i], 74.386)), module, SP4TSource4In::SRC1_ELEV_PARAM + i));
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(x[i], 83.2)), module, SP4TSource4In::SRC1_ELEV_CV_INPUT + i));
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(x[i], 97.458)), module, SP4TSource4In::SRC1_INPUT + i));
		}

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(8.756, 114.0)), module, SP4TSource4In::OMNI_INPUT));
		addParam(createParamCentered<SP4TSwitchThree>(mm2px(Vec(30.947, 114.0)), module, SP4TSource4In::ORDER_PARAM));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(42.043, 114.0)), module, SP4TSource4In::AMBI_OUTPUT));
	}

	/** Shows the view mode and the source directions of the module. */
	void step() override {
		SP4TSource4In* module = getModule<SP4TSource4In>();
		if (module) {
			display->viewMode = module->viewMode;
			display->sources.clear();
			for (int i = 0; i < SP4TSource4In::NUM_SOURCES; i++) {
				display->sources.push_back({module->directions[i], string::f("%d", i + 1),
				                            module->connected[i] ? SOURCE_ACTIVE : SOURCE_INACTIVE});
			}
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		SP4TSource4In* module = getModule<SP4TSource4In>();

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("3D view mode",
			sp4t::AmbisonicDisplay::getViewModeLabels(),
			&module->viewMode));
		menu->addChild(createBoolPtrMenuItem("Flip azimuth sign", "", &module->flipAzimuth));
	}
};


Model* modelNun4takSP4TSource4In = createModel<SP4TSource4In, SP4TSource4InWidget>("SP4T-Source4-In");
