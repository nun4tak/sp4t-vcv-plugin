#include "plugin.hpp"
#include "components.hpp"
#include "AmbiEncoder.hpp"
#include "AmbisonicDisplay.hpp"
#include "SphericalCoordinates.hpp"

#include <cmath>


namespace {

/** Computes the directions of a stereo pair centered on (azimuth, elevation).
 *
 * Each source lies at `spread` degrees (great-circle distance) from the center,
 * along the horizontal tangent pointing toward increasing azimuth for the left
 * source.
 */
void stereoPairDirections(float azimuth, float elevation, float spread,
                          sp4t::Direction* left, sp4t::Direction* right) {
	sp4t::Vector3 d = sp4t::directionToVector(azimuth, elevation);
	float a = azimuth * sp4t::DEG2RAD;
	sp4t::Vector3 t = {-std::sin(a), std::cos(a), 0.f};
	float c = std::cos(spread * sp4t::DEG2RAD);
	float s = std::sin(spread * sp4t::DEG2RAD);
	*left = sp4t::vectorToDirection(sp4t::Vector3{c * d.x + s * t.x, c * d.y + s * t.y, c * d.z + s * t.z});
	*right = sp4t::vectorToDirection(sp4t::Vector3{c * d.x - s * t.x, c * d.y - s * t.y, c * d.z - s * t.z});
}

// Colors of the sources on the screen (Ayu dark)
const NVGcolor SOURCE_MONO = nvgRGB(0xff, 0xb4, 0x54);
const NVGcolor SOURCE_INACTIVE = nvgRGB(0x56, 0x5b, 0x66);
const NVGcolor SOURCE_LEFT = nvgRGB(0x59, 0xc2, 0xff);
const NVGcolor SOURCE_RIGHT = nvgRGB(0xf0, 0x71, 0x78);

} // namespace


struct SP4TStereoIn : Module {
	enum ParamId {
		AZIMUTH_PARAM,
		ELEVATION_PARAM,
		SPREAD_PARAM,
		ORDER_PARAM,
		AZIMUTH_CV_PARAM,
		ELEVATION_CV_PARAM,
		SPREAD_CV_PARAM,
		AZIMUTH_CV_SWITCH_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AZIMUTH_CV_INPUT,
		ELEVATION_CV_INPUT,
		SPREAD_CV_INPUT,
		LEFT_INPUT,
		RIGHT_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	enum { MAX_NUM_SOURCES = 2 };

	sp4t::AmbiEncoder encoder;
	dsp::ClockDivider controlDivider;

	// Source directions shown on the screen, updated by the engine thread.
	sp4t::Direction centerDirection = {0.f, 0.f};
	sp4t::Direction leftDirection = {0.f, 0.f};
	sp4t::Direction rightDirection = {0.f, 0.f};
	bool stereo = false;

	/** sp4t::AmbisonicDisplay::ViewMode */
	int viewMode = sp4t::AmbisonicDisplay::VIEW_ORTHOGRAPHIC_PAIR;

	SP4TStereoIn() : encoder(MAX_NUM_SOURCES) {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(AZIMUTH_PARAM, -180.f, 180.f, 0.f, "Azimuth", "°");
		configParam(ELEVATION_PARAM, -90.f, 90.f, 0.f, "Elevation", "°");
		configParam(SPREAD_PARAM, 0.f, 90.f, 45.f, "Spread", "°");
		configParam(AZIMUTH_CV_PARAM, -1.f, 1.f, 0.f, "Azimuth CV attenuverter", "%", 0.f, 100.f);
		configParam(ELEVATION_CV_PARAM, -1.f, 1.f, 0.f, "Elevation CV attenuverter", "%", 0.f, 100.f);
		configParam(SPREAD_CV_PARAM, -1.f, 1.f, 0.f, "Spread CV attenuverter", "%", 0.f, 100.f);
		configSwitch(ORDER_PARAM, 0.f, 2.f, 0.f, "Ambisonic order", {"1st (FOA)", "2nd (HOA-2)", "3rd (HOA-3)"});
		// The Ambisonic convention (positive azimuth = anticlockwise, i.e., to the
		// left) is the default; the other position is more natural for eurorack
		// users, for whom turning the knob to the right moves the source to the
		// right.
		configSwitch(AZIMUTH_CV_SWITCH_PARAM, 0.f, 1.f, 0.f, "Azimuth direction",
		             {"Positive = left (Ambisonic)", "Positive = right"});

		configInput(AZIMUTH_CV_INPUT, "Azimuth CV");
		configInput(ELEVATION_CV_INPUT, "Elevation CV");
		configInput(SPREAD_CV_INPUT, "Spread CV");
		configInput(LEFT_INPUT, "Left (mono)");
		configInput(RIGHT_INPUT, "Right");
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");

		controlDivider.setDivision(16);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		viewMode = sp4t::AmbisonicDisplay::VIEW_ORTHOGRAPHIC_PAIR;
		encoder.reset();
		updateControls();
	}

	/** Knob value plus the CV input scaled by its attenuverter, where
	 * `fullScaleVoltage` (5 V bipolar, 10 V unipolar) spans `range` degrees. */
	float getAngle(int paramId, int cvParamId, int inputId, float range, float fullScaleVoltage) {
		float cv = inputs[inputId].getVoltage() / fullScaleVoltage;
		return params[paramId].getValue() + params[cvParamId].getValue() * cv * range;
	}

	void updateControls() {
		encoder.setOrder((int) params[ORDER_PARAM].getValue() + 1);

		// Beyond +/-180 degrees, the azimuth wraps around. The elevation doesn't
		// go past the poles (the rear is reached with the azimuth).
		float azimuth = getAngle(AZIMUTH_PARAM, AZIMUTH_CV_PARAM, AZIMUTH_CV_INPUT, 180.f, 5.f);
		if (params[AZIMUTH_CV_SWITCH_PARAM].getValue() > 0.5f)
			azimuth = -azimuth;
		float elevation = math::clamp(getAngle(ELEVATION_PARAM, ELEVATION_CV_PARAM, ELEVATION_CV_INPUT, 90.f, 5.f), -90.f, 90.f);
		float spread = math::clamp(getAngle(SPREAD_PARAM, SPREAD_CV_PARAM, SPREAD_CV_INPUT, 90.f, 10.f), 0.f, 90.f);

		// Stereo as soon as the right input is connected.
		bool isStereo = inputs[RIGHT_INPUT].isConnected();
		encoder.setNumSources(isStereo ? 2 : 1);

		sp4t::Direction center = sp4t::vectorToDirection(sp4t::directionToVector(azimuth, elevation));
		sp4t::Direction left = center;
		sp4t::Direction right = center;
		if (isStereo) {
			stereoPairDirections(azimuth, elevation, spread, &left, &right);
			encoder.setSourceDirection(0, left.azimuth, left.elevation);
			encoder.setSourceDirection(1, right.azimuth, right.elevation);
		}
		else {
			encoder.setSourceDirection(0, center.azimuth, center.elevation);
		}

		centerDirection = center;
		leftDirection = left;
		rightDirection = right;
		stereo = isStereo;
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		float in[MAX_NUM_SOURCES] = {
			inputs[LEFT_INPUT].getVoltageSum(),
			inputs[RIGHT_INPUT].getVoltageSum(),
		};
		float out[sp4t::AmbiEncoder::MAX_NUM_SH];
		encoder.process(in, out);

		int channels = encoder.getNumSH();
		outputs[AMBI_OUTPUT].setChannels(channels);
		for (int c = 0; c < channels; c++)
			outputs[AMBI_OUTPUT].setVoltage(out[c], c);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// Saved as "projection", the former name of the view mode.
		json_object_set_new(rootJ, "projection", json_integer(viewMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* viewModeJ = json_object_get(rootJ, "projection");
		if (viewModeJ)
			viewMode = math::clamp((int) json_integer_value(viewModeJ), 0, sp4t::AmbisonicDisplay::VIEW_MODES_LEN - 1);
	}
};


struct SP4TStereoInWidget : ModuleWidget {
	sp4t::AmbisonicDisplay* display;

	SP4TStereoInWidget(SP4TStereoIn* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TStereoIn.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TStereoIn-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		display = createWidget<sp4t::AmbisonicDisplay>(mm2px(Vec(1.728, 11.5)));
		display->box.size = mm2px(Vec(47.344, 34.039));
		display->sources = {{{0.f, 0.f}, "M", SOURCE_MONO}};
		addChild(display);

		addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(8.757, 58.901)), module, SP4TStereoIn::AZIMUTH_PARAM));
		addParam(createParamCentered<SP4TSwitchTwo>(mm2px(Vec(8.829, 72.418)), module, SP4TStereoIn::AZIMUTH_CV_SWITCH_PARAM));
		addParam(createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(8.756, 84.492)), module, SP4TStereoIn::AZIMUTH_CV_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(8.756, 94.903)), module, SP4TStereoIn::AZIMUTH_CV_INPUT));

		addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(25.4, 58.901)), module, SP4TStereoIn::ELEVATION_PARAM));
		addParam(createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(19.852, 84.492)), module, SP4TStereoIn::ELEVATION_CV_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(19.852, 94.903)), module, SP4TStereoIn::ELEVATION_CV_INPUT));

		addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(42.044, 67.656)), module, SP4TStereoIn::SPREAD_PARAM));
		addParam(createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(42.043, 84.492)), module, SP4TStereoIn::SPREAD_CV_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(42.043, 94.903)), module, SP4TStereoIn::SPREAD_CV_INPUT));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(8.756, 114.0)), module, SP4TStereoIn::LEFT_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(19.852, 114.0)), module, SP4TStereoIn::RIGHT_INPUT));
		addParam(createParamCentered<SP4TSwitchThree>(mm2px(Vec(31.188, 114.0)), module, SP4TStereoIn::ORDER_PARAM));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(42.043, 114.0)), module, SP4TStereoIn::AMBI_OUTPUT));
	}

	/** Shows the view mode and the source directions of the module. */
	void step() override {
		SP4TStereoIn* module = getModule<SP4TStereoIn>();
		if (module) {
			display->viewMode = module->viewMode;
			display->boundaryReference = module->centerDirection;
			display->sources.clear();
			if (module->stereo) {
				display->sources.push_back({module->centerDirection, "M", SOURCE_INACTIVE});
				display->sources.push_back({module->leftDirection, "L", SOURCE_LEFT});
				display->sources.push_back({module->rightDirection, "R", SOURCE_RIGHT});
			}
			else {
				display->sources.push_back({module->centerDirection, "M", SOURCE_MONO});
			}
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		SP4TStereoIn* module = getModule<SP4TStereoIn>();

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("3D view mode",
			sp4t::AmbisonicDisplay::getViewModeLabels(),
			&module->viewMode));
	}
};


Model* modelNun4takSP4TStereoIn = createModel<SP4TStereoIn, SP4TStereoInWidget>("SP4T-Stereo-In");
