#include "plugin.hpp"
#include "components.hpp"

#include <cmath>


namespace {

/** Axes a flip can be applied across, as the bits of a channel sign mask. */
enum FlipAxis {
	FLIP_X = 1,
	FLIP_Y = 2,
	FLIP_Z = 4
};

/** Axes whose flip negates each ACN channel, as a mask of the bits above.
 *
 * A flip across an axis negates the spherical harmonics that are odd with
 * respect to it; the others are left untouched.
 *
 *   ACN   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *   X     +  +  +  -  -  +  +  -  +  +  -  +  +  -  +  -
 *   Y     +  -  +  +  -  -  +  +  +  -  -  -  +  +  +  +
 *   Z     +  +  -  +  +  -  +  -  +  +  -  +  -  +  -  +
 */
const int CHANNEL_FLIP_MASK[16] = {
	0,
	FLIP_Y,
	FLIP_Z,
	FLIP_X,
	FLIP_X | FLIP_Y,
	FLIP_Y | FLIP_Z,
	0,
	FLIP_X | FLIP_Z,
	0,
	FLIP_Y,
	FLIP_X | FLIP_Y | FLIP_Z,
	FLIP_Y,
	FLIP_Z,
	FLIP_X,
	FLIP_Z,
	FLIP_X,
};

/** Ramp of an axis when its flip is engaged or released, in flip units per
 * second (5 ms, as the mute fade of the Mute module: short enough for the flip
 * to feel instantaneous and long enough to avoid clicks). A slower, audible
 * flip is what the dry/wet control is for. */
const float FLIP_RAMP_RATE = 1.f / 0.005f;

} // namespace


struct SP4TFlip : Module {
	enum {
		NUM_AXES = 3,
		NUM_MASKS = 8
	};

	enum ParamId {
		FLIP_X_BUTTON_PARAM,
		FLIP_Y_BUTTON_PARAM,
		FLIP_Z_BUTTON_PARAM,
		DRY_WET_PARAM,
		DRY_WET_CV_PARAM,
		DRYWET_MODE_SWITCH_PARAM,
		TRIGGER_MODE_SWITCH_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
		X_CV_INPUT,
		Y_CV_INPUT,
		Z_CV_INPUT,
		DRY_WET_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::ClockDivider controlDivider;
	dsp::SchmittTrigger flipTriggers[NUM_AXES];

	/** Number of channels of the input signal, read at control rate. */
	int numChannels = 0;
	/** How far each axis is flipped, and the value it ramps to (0: not flipped,
	 * 1: flipped). */
	float flipAmounts[NUM_AXES] = {};
	float flipTargets[NUM_AXES] = {};
	/** Whether an axis is ramping, i.e., whether the ramps must be advanced. */
	bool ramping = false;
	/** State of the flip CV gates, to act on their edges only. */
	bool gateHigh[NUM_AXES] = {};
	/** Dry/wet crossfading gains, set at control rate. */
	float dryGain = 0.f;
	float wetGain = 1.f;
	/** Output gain of each sign mask of CHANNEL_FLIP_MASK, i.e., the dry and the
	 * flipped channel mixed down to the single factor the channel is scaled by. */
	float maskGains[NUM_MASKS] = {};
	/** Set while there is nothing to flip or nowhere to send it. Starts set, so
	 * that a patch is loaded with the flips of its buttons already in force. */
	bool idle = true;

	SP4TFlip() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(FLIP_X_BUTTON_PARAM, 0.f, 1.f, 0.f, "Flip across X (front-rear)", {"Off", "On"});
		configSwitch(FLIP_Y_BUTTON_PARAM, 0.f, 1.f, 0.f, "Flip across Y (left-right)", {"Off", "On"});
		configSwitch(FLIP_Z_BUTTON_PARAM, 0.f, 1.f, 0.f, "Flip across Z (top-bottom)", {"Off", "On"});
		configParam(DRY_WET_PARAM, 0.f, 1.f, 1.f, "Dry/wet", "%", 0.f, 100.f);
		configParam(DRY_WET_CV_PARAM, -1.f, 1.f, 0.f, "Dry/wet CV attenuverter", "%", 0.f, 100.f);
		configSwitch(DRYWET_MODE_SWITCH_PARAM, 0.f, 1.f, 0.f, "Dry/wet crossfade", {"Equal power", "Through zero"});
		configSwitch(TRIGGER_MODE_SWITCH_PARAM,
		             0.f,
		             1.f,
		             1.f,
		             "Buttons and flip CV",
		             {"Momentary (gate)", "Latching (trigger)"});

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configInput(X_CV_INPUT, "X flip gate/trigger");
		configInput(Y_CV_INPUT, "Y flip gate/trigger");
		configInput(Z_CV_INPUT, "Z flip gate/trigger");
		configInput(DRY_WET_CV_INPUT, "Dry/wet CV");
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");
		configBypass(AMBI_INPUT, AMBI_OUTPUT);

		controlDivider.setDivision(16);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int a = 0; a < NUM_AXES; a++) {
			flipTriggers[a].reset();
			gateHigh[a] = false;
			flipAmounts[a] = 0.f;
		}
		updateControls();
	}

	/** Reads the buttons, the flip CV inputs and the dry/wet control. Those do
	 * not need audio-rate accuracy: a division of 16 samples is well under the
	 * 1 ms of the shortest trigger of the voltage standard. */
	void updateControls() {
		bool latching = params[TRIGGER_MODE_SWITCH_PARAM].getValue() > 0.5f;

		for (int a = 0; a < NUM_AXES; a++) {
			Param& button = params[FLIP_X_BUTTON_PARAM + a];
			bool triggered = flipTriggers[a].process(inputs[X_CV_INPUT + a].getVoltage(), 0.1f, 1.f);
			bool high = flipTriggers[a].isHigh();

			// The CV sets the state of the button, as a click on it does: a
			// trigger toggles it in latching mode, a gate follows it in
			// momentary mode.
			if (latching) {
				if (triggered)
					button.setValue(button.getValue() > 0.5f ? 0.f : 1.f);
			}
			else if (high != gateHigh[a]) {
				button.setValue(high ? 1.f : 0.f);
			}
			gateHigh[a] = high;

			flipTargets[a] = button.getValue() > 0.5f ? 1.f : 0.f;
		}

		// The dry/wet CV is unipolar: its full scale of 10 V spans the whole
		// range of the knob with the attenuverter wide open.
		float mix = params[DRY_WET_PARAM].getValue()
		            + params[DRY_WET_CV_PARAM].getValue() * inputs[DRY_WET_CV_INPUT].getVoltage() / 10.f;
		mix = math::clamp(mix, 0.f, 1.f);

		// Equal power: the two fields overlap over the whole travel, their
		// gains crossing at -3 dB, which keeps the power constant for
		// uncorrelated signals. Through zero: each gain falls linearly to zero
		// at the centre, where the output is silent, so the fields are never
		// heard over each other.
		if (params[DRYWET_MODE_SWITCH_PARAM].getValue() < 0.5f) {
			float angle = mix * 0.5f * float(M_PI);
			dryGain = std::cos(angle);
			wetGain = std::sin(angle);
		}
		else {
			dryGain = std::fmax(1.f - 2.f * mix, 0.f);
			wetGain = std::fmax(2.f * mix - 1.f, 0.f);
		}

		numChannels = inputs[AMBI_INPUT].getChannels();
		outputs[AMBI_OUTPUT].setChannels(numChannels);

		ramping = false;
		for (int a = 0; a < NUM_AXES; a++)
			ramping = ramping || flipAmounts[a] != flipTargets[a];

		updateMaskGains();
	}

	/** Mixes the dry and the flipped channels into one gain per sign mask. The
	 * sign of a channel negated by an axis sweeps from +1 to -1 as that axis is
	 * flipped, and a channel negated by several axes follows the product of
	 * their signs. */
	void updateMaskGains() {
		float signs[NUM_AXES];
		for (int a = 0; a < NUM_AXES; a++)
			signs[a] = 1.f - 2.f * flipAmounts[a];

		for (int mask = 0; mask < NUM_MASKS; mask++) {
			float sign = 1.f;
			for (int a = 0; a < NUM_AXES; a++) {
				if (mask & (1 << a))
					sign *= signs[a];
			}
			maskGains[mask] = dryGain + wetGain * sign;
		}
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		// Nothing to flip, or nowhere to send it. A flip has no memory, so there
		// is no tail to let ring out and the work can stop at once.
		if (!inputs[AMBI_INPUT].isConnected() || !outputs[AMBI_OUTPUT].isConnected()) {
			idle = true;
			return;
		}

		// The ramps jump to where they were heading while the module was idle:
		// the buttons pressed meanwhile take effect at once rather than fading
		// in once it is patched back in.
		if (idle) {
			idle = false;
			for (int a = 0; a < NUM_AXES; a++)
				flipAmounts[a] = flipTargets[a];
			ramping = false;
			updateMaskGains();
		}

		// Advancing the ramps is skipped while no axis is ramping, which is most
		// of the time.
		if (ramping) {
			float step = args.sampleTime * FLIP_RAMP_RATE;
			for (int a = 0; a < NUM_AXES; a++) {
				float target = flipTargets[a];
				flipAmounts[a] = flipAmounts[a] < target ? std::fmin(flipAmounts[a] + step, target)
				                                         : std::fmax(flipAmounts[a] - step, target);
			}
			updateMaskGains();
		}

		for (int c = 0; c < numChannels; c++) {
			float out = inputs[AMBI_INPUT].getVoltage(c) * maskGains[CHANNEL_FLIP_MASK[c]];
			outputs[AMBI_OUTPUT].setVoltage(out, c);
		}
	}
};


struct SP4TFlipWidget : ModuleWidget {
	SP4TFlipWidget(SP4TFlip* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TFlip.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TFlip-dark.svg")));

		// 6 HP: two screws, on the left.
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const float buttonY[] = {19.509f, 29.384f, 39.26f};
		for (int a = 0; a < SP4TFlip::NUM_AXES; a++) {
			SP4TModeButton* button = createParamCentered<SP4TModeButton>(
				mm2px(Vec(15.24, buttonY[a])), module, SP4TFlip::FLIP_X_BUTTON_PARAM + a);
			button->modeParamId = SP4TFlip::TRIGGER_MODE_SWITCH_PARAM;
			addParam(button);
		}

		addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(15.239, 57.156)), module, SP4TFlip::DRY_WET_PARAM));
		addParam(
			createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(6.411, 69.355)), module, SP4TFlip::DRY_WET_CV_PARAM));
		addParam(
			createParamCentered<SP4TSwitchTwo>(mm2px(Vec(25.91, 69.355)), module, SP4TFlip::DRYWET_MODE_SWITCH_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.411, 79.833)), module, SP4TFlip::DRY_WET_CV_INPUT));
		addParam(
			createParamCentered<SP4TSwitchTwo>(mm2px(Vec(25.91, 79.864)), module, SP4TFlip::TRIGGER_MODE_SWITCH_PARAM));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(6.411, 95.143)), module, SP4TFlip::X_CV_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(15.24, 95.143)), module, SP4TFlip::Y_CV_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(24.068, 95.143)), module, SP4TFlip::Z_CV_INPUT));

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(6.411, 114.0)), module, SP4TFlip::AMBI_INPUT));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(24.068, 114.0)), module, SP4TFlip::AMBI_OUTPUT));
	}
};


Model* modelNun4takSP4TFlip = createModel<SP4TFlip, SP4TFlipWidget>("SP4T-Flip");
