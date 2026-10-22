#include "plugin.hpp"
#include "components.hpp"
#include "AmbiEncoder.hpp"
#include "AmbisonicDisplay.hpp"
#include "SphericalCoordinates.hpp"


namespace {

// Color of the sources on the screen (Ayu dark), the same the mono source of
// the Stereo In module is drawn in.
const NVGcolor SOURCE_COLOR = nvgRGB(0xff, 0xb4, 0x54);

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

	enum {
		/** Source inputs, i.e., columns of the panel. */
		NUM_SOURCE_INPUTS = 4,
		/** Each of them is a point source per channel of the cable patched into it. */
		MAX_SOURCES = NUM_SOURCE_INPUTS * PORT_MAX_CHANNELS
	};

	/** One point source: a single channel of one source input. */
	struct Source {
		sp4t::Direction direction;
		int input;
		int channel;
	};

	sp4t::AmbiEncoder encoder;
	dsp::ClockDivider controlDivider;

	// The point sources being encoded, in the order of the encoder slots they
	// occupy: the encoder takes a contiguous range, so the channels of the
	// patched inputs are packed into its first `numSources` slots. Also what the
	// screen shows, read from the UI thread.
	Source sources[MAX_SOURCES] = {};
	int numSources = 0;

	bool flipAzimuth = false;
	bool idle = false;

	/** sp4t::AmbisonicDisplay::ViewMode */
	int viewMode = sp4t::AmbisonicDisplay::VIEW_XY_Z_PROJECTIONS;

	SP4TSource4In() : encoder(MAX_SOURCES) {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_SOURCE_INPUTS; i++) {
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
	float getAngle(int paramId, float cv, float range) { return params[paramId].getValue() + cv / 5.f * range; }

	void updateControls() {
		encoder.setOrder((int) params[ORDER_PARAM].getValue() + 1);

		// The AZ and ELEV CV inputs are normalled from left to right (sources 1
		// to 4): an unpatched one takes the signal of the nearest patched jack
		// of its row on its left, and no CV at all when there is none. Which
		// jack that is, rather than its voltage, because the signal taken from
		// it is read per channel below.
		int azCvInput = -1;
		int elevCvInput = -1;

		numSources = 0;
		for (int i = 0; i < NUM_SOURCE_INPUTS; i++) {
			if (inputs[SRC1_AZ_CV_INPUT + i].isConnected())
				azCvInput = i;
			if (inputs[SRC1_ELEV_CV_INPUT + i].isConnected())
				elevCvInput = i;

			// A point source per channel of the cable patched into this input,
			// all of them taking that column's two knobs, each its own channel
			// of the two CV signals in force (channel 1 of a monophonic one,
			// and 0 V past the last channel of a shorter polyphonic one).
			int channels = inputs[SRC1_INPUT + i].getChannels();
			for (int c = 0; c < channels; c++) {
				float azCv = azCvInput < 0 ? 0.f : inputs[SRC1_AZ_CV_INPUT + azCvInput].getPolyVoltage(c);
				float elevCv = elevCvInput < 0 ? 0.f : inputs[SRC1_ELEV_CV_INPUT + elevCvInput].getPolyVoltage(c);

				// Beyond +/-180 degrees, the azimuth wraps around. The elevation
				// doesn't go past the poles (the rear is reached with the azimuth).
				float azimuth = getAngle(SRC1_AZ_PARAM + i, azCv, 180.f);
				if (flipAzimuth)
					azimuth = -azimuth;
				float elevation = math::clamp(getAngle(SRC1_ELEV_PARAM + i, elevCv, 90.f), -90.f, 90.f);

				Source& source = sources[numSources];
				source.direction = sp4t::vectorToDirection(sp4t::directionToVector(azimuth, elevation));
				source.input = i;
				source.channel = c;
				numSources++;
			}
		}

		encoder.setNumSources(numSources);
		for (int s = 0; s < numSources; s++)
			encoder.setSourceDirection(s, sources[s].direction.azimuth, sources[s].direction.elevation);
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		// Nothing to encode, or nowhere to send it. The interpolation is reset so
		// that reconnecting starts from the directions in force rather than
		// gliding from stale gains.
		if (!outputs[AMBI_OUTPUT].isConnected() || (numSources == 0 && !inputs[OMNI_INPUT].isConnected())) {
			if (!idle) {
				idle = true;
				encoder.reset();
				outputs[AMBI_OUTPUT].setChannels(0);
			}
			return;
		}
		idle = false;

		float out[sp4t::AmbiEncoder::MAX_NUM_SH] = {};
		if (numSources > 0) {
			float in[MAX_SOURCES] = {};
			for (int s = 0; s < numSources; s++)
				in[s] = inputs[SRC1_INPUT + sources[s].input].getVoltage(sources[s].channel);
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

		// One column per source input, left to right. The screen is left empty:
		// it shows the sources being encoded, and there are none until something
		// is patched (the module browser included).
		static const float x[SP4TSource4In::NUM_SOURCE_INPUTS] = {8.756f, 19.852f, 30.948f, 42.043f};
		for (int i = 0; i < SP4TSource4In::NUM_SOURCE_INPUTS; i++) {
			addParam(createParamCentered<SP4TRoundSmallKnob>(
				mm2px(Vec(x[i], 53.39)), module, SP4TSource4In::SRC1_AZ_PARAM + i));
			addInput(createInputCentered<ThemedPJ301MPort>(
				mm2px(Vec(x[i], 62.205)), module, SP4TSource4In::SRC1_AZ_CV_INPUT + i));
			addParam(createParamCentered<SP4TRoundSmallKnob>(
				mm2px(Vec(x[i], 74.386)), module, SP4TSource4In::SRC1_ELEV_PARAM + i));
			addInput(createInputCentered<ThemedPJ301MPort>(
				mm2px(Vec(x[i], 83.2)), module, SP4TSource4In::SRC1_ELEV_CV_INPUT + i));
			addInput(
				createInputCentered<ThemedPJ301MPort>(mm2px(Vec(x[i], 97.458)), module, SP4TSource4In::SRC1_INPUT + i));
		}

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(8.756, 114.0)), module, SP4TSource4In::OMNI_INPUT));
		addParam(createParamCentered<SP4TSwitchThree>(mm2px(Vec(30.947, 114.0)), module, SP4TSource4In::ORDER_PARAM));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(42.043, 114.0)), module, SP4TSource4In::AMBI_OUTPUT));
	}

	/** Shows the view mode and the sources being encoded by the module.
	 *
	 * The sources are numbered from 1 in the order they are encoded in, which
	 * runs from the first input to the fourth and, within one input, follows the
	 * channels of its cable. A given number therefore belongs to no fixed input:
	 * patching a cable, or one with a different number of channels, renumbers
	 * the sources to its right.
	 */
	void step() override {
		SP4TSource4In* module = getModule<SP4TSource4In>();
		if (module) {
			display->viewMode = module->viewMode;
			display->sources.clear();
			int numSources = math::clamp(module->numSources, 0, (int) SP4TSource4In::MAX_SOURCES);
			for (int s = 0; s < numSources; s++)
				display->sources.push_back({module->sources[s].direction, string::f("%d", s + 1), SOURCE_COLOR});
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		SP4TSource4In* module = getModule<SP4TSource4In>();

		menu->addChild(new MenuSeparator);
		menu->addChild(
			createIndexPtrSubmenuItem("3D view mode", sp4t::AmbisonicDisplay::getViewModeLabels(), &module->viewMode));
		menu->addChild(createBoolPtrMenuItem("Flip azimuth sign", "", &module->flipAzimuth));
	}
};


Model* modelNun4takSP4TSource4In = createModel<SP4TSource4In, SP4TSource4InWidget>("SP4T-Source4-In");
