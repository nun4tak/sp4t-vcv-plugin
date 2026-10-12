#include "plugin.hpp"
#include "components.hpp"
#include "AmbiRotator.hpp"
#include "RotationDisplay.hpp"

#include <cmath>


struct SP4TRotate : Module {
	enum ParamId {
		YAW_PARAM,
		PITCH_PARAM,
		ROLL_PARAM,
		YAW_CV_PARAM,
		PITCH_CV_PARAM,
		ROLL_CV_PARAM,
		YAW_CV_SWITCH_PARAM,
		PITCH_CV_SWITCH_PARAM,
		ROLL_CV_SWITCH_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
		YAW_CV_INPUT,
		PITCH_CV_INPUT,
		ROLL_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	sp4t::AmbiRotator rotator;
	dsp::ClockDivider controlDivider;

	/** The angles the screen shows, in degrees, set at control rate. */
	float yaw = 0.f;
	float pitch = 0.f;
	float roll = 0.f;

	/** Set while there is nothing to rotate or nowhere to send it. */
	bool idle = false;

	SP4TRotate() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(YAW_PARAM, -180.f, 180.f, 0.f, "Yaw", "°");
		configParam(PITCH_PARAM, -90.f, 90.f, 0.f, "Pitch", "°");
		configParam(ROLL_PARAM, -180.f, 180.f, 0.f, "Roll", "°");
		configParam(YAW_CV_PARAM, -1.f, 1.f, 0.f, "Yaw CV attenuverter", "%", 0.f, 100.f);
		configParam(PITCH_CV_PARAM, -1.f, 1.f, 0.f, "Pitch CV attenuverter", "%", 0.f, 100.f);
		configParam(ROLL_CV_PARAM, -1.f, 1.f, 0.f, "Roll CV attenuverter", "%", 0.f, 100.f);
		configSwitch(YAW_CV_SWITCH_PARAM, 0.f, 1.f, 0.f, "Yaw direction", {"Positive", "Negative"});
		configSwitch(PITCH_CV_SWITCH_PARAM, 0.f, 1.f, 0.f, "Pitch direction", {"Positive", "Negative"});
		configSwitch(ROLL_CV_SWITCH_PARAM, 0.f, 1.f, 0.f, "Roll direction", {"Positive", "Negative"});

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configInput(YAW_CV_INPUT, "Yaw CV");
		configInput(PITCH_CV_INPUT, "Pitch CV");
		configInput(ROLL_CV_INPUT, "Roll CV");
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");
		configBypass(AMBI_INPUT, AMBI_OUTPUT);

		controlDivider.setDivision(16);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		rotator.reset();
		updateControls();
	}

	/** Knob plus the attenuverted CV voltage in force, in degrees, negated by
	 * the +/- switch. The switch applies to the sum, i.e. to the knob as well as
	 * to the CV. `range` is reached at 5 V with the attenuverter wide open. */
	float getAngle(int paramId, int cvParamId, float cv, int switchId, float range) {
		float angle = params[paramId].getValue() + params[cvParamId].getValue() * cv / 5.f * range;
		return params[switchId].getValue() < 0.5f ? angle : -angle;
	}

	void updateControls() {
		// The order of the input signal is given by the number of channels of
		// the polyphonic cable, i.e., 4 (FOA), 9 (HOA-2) or 16 (HOA-3).
		int channels = inputs[AMBI_INPUT].getChannels();
		rotator.setOrder((int) std::floor(std::sqrt((float) channels)) - 1);

		// The angle CV inputs are normalled from left to right (yaw, pitch,
		// roll): an unpatched one takes the voltage of the nearest patched jack
		// on its left, and 0 V when there is none. Each angle keeps its own
		// attenuverter and direction switch, so one CV can drive the three of
		// them by different amounts and in different directions.
		float cv[3] = {};
		float normalled = 0.f;
		for (int i = 0; i < 3; i++) {
			if (inputs[YAW_CV_INPUT + i].isConnected())
				normalled = inputs[YAW_CV_INPUT + i].getVoltage();
			cv[i] = normalled;
		}

		// Yaw and roll are left to wrap around and pitch is clamped by the
		// rotator, which also keeps the screen and the audio path in agreement.
		yaw = getAngle(YAW_PARAM, YAW_CV_PARAM, cv[0], YAW_CV_SWITCH_PARAM, 180.f);
		pitch = getAngle(PITCH_PARAM, PITCH_CV_PARAM, cv[1], PITCH_CV_SWITCH_PARAM, 90.f);
		roll = getAngle(ROLL_PARAM, ROLL_CV_PARAM, cv[2], ROLL_CV_SWITCH_PARAM, 180.f);
		rotator.setAngles(yaw, pitch, roll);
	}

	void process(const ProcessArgs& args) override {
		// Kept running while idle, so that the screen follows the knobs even
		// with nothing patched.
		if (controlDivider.process())
			updateControls();

		// Nothing to rotate, or nowhere to send it. A rotation has no memory,
		// so there is no tail to let ring out and the work can stop at once.
		// The interpolation is reset so that reconnecting starts from the
		// angles in force rather than gliding from a stale matrix.
		if (!inputs[AMBI_INPUT].isConnected() || !outputs[AMBI_OUTPUT].isConnected()) {
			if (!idle) {
				idle = true;
				rotator.reset();
				outputs[AMBI_OUTPUT].setChannels(0);
			}
			return;
		}
		idle = false;

		int channels = rotator.getNumSH();
		outputs[AMBI_OUTPUT].setChannels(channels);

		// The identity rotation is a copy, which spares the matrix multiply
		// while the angles sit at zero -- the default, and whenever the module
		// is patched in ready to be modulated later. isIdentity() only holds
		// once the interpolation has reached the identity, so the copy returns
		// exactly what the multiply would.
		if (rotator.isIdentity()) {
			for (int c = 0; c < channels; c++)
				outputs[AMBI_OUTPUT].setVoltage(inputs[AMBI_INPUT].getVoltage(c), c);
			return;
		}

		float in[sp4t::AmbiRotator::MAX_NUM_SH] = {};
		for (int c = 0; c < channels; c++)
			in[c] = inputs[AMBI_INPUT].getVoltage(c);

		float out[sp4t::AmbiRotator::MAX_NUM_SH];
		rotator.process(in, out);

		for (int c = 0; c < channels; c++)
			outputs[AMBI_OUTPUT].setVoltage(out[c], c);
	}
};


struct SP4TRotateWidget : ModuleWidget {
	sp4t::RotationDisplay* display;

	SP4TRotateWidget(SP4TRotate* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TRotate.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TRotate-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		display = createWidget<sp4t::RotationDisplay>(mm2px(Vec(1.728, 11.5)));
		display->box.size = mm2px(Vec(37.294, 34.039));
		addChild(display);

		addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(7.647, 58.9)), module, SP4TRotate::YAW_PARAM));
		addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(20.32, 58.9)), module, SP4TRotate::PITCH_PARAM));
		addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(33.102, 58.9)), module, SP4TRotate::ROLL_PARAM));

		addParam(
			createParamCentered<SP4TSwitchTwo>(mm2px(Vec(8.306, 72.157)), module, SP4TRotate::YAW_CV_SWITCH_PARAM));
		addParam(
			createParamCentered<SP4TSwitchTwo>(mm2px(Vec(20.319, 72.157)), module, SP4TRotate::PITCH_CV_SWITCH_PARAM));
		addParam(
			createParamCentered<SP4TSwitchTwo>(mm2px(Vec(32.395, 72.157)), module, SP4TRotate::ROLL_CV_SWITCH_PARAM));

		addParam(createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(8.756, 84.493)), module, SP4TRotate::YAW_CV_PARAM));
		addParam(
			createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(20.321, 84.493)), module, SP4TRotate::PITCH_CV_PARAM));
		addParam(
			createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(31.885, 84.493)), module, SP4TRotate::ROLL_CV_PARAM));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(8.756, 94.97)), module, SP4TRotate::YAW_CV_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(20.32, 94.97)), module, SP4TRotate::PITCH_CV_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(31.884, 94.97)), module, SP4TRotate::ROLL_CV_INPUT));

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(8.756, 114.0)), module, SP4TRotate::AMBI_INPUT));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(31.884, 114.0)), module, SP4TRotate::AMBI_OUTPUT));
	}

	void step() override {
		SP4TRotate* module = getModule<SP4TRotate>();
		if (module)
			sp4t::AmbiRotator::rotatedAxes(module->yaw, module->pitch, module->roll, display->axes);
		ModuleWidget::step();
	}
};


Model* modelNun4takSP4TRotate = createModel<SP4TRotate, SP4TRotateWidget>("SP4T-Rotate");
