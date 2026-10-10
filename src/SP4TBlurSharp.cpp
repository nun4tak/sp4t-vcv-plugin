#include "plugin.hpp"
#include "components.hpp"

#include <cmath>


namespace {

enum {
	NUM_CHANNELS = 16,
	/** Ambisonic degrees of a 3rd-order signal, i.e., l = 0 ... 3. */
	NUM_DEGREES = 4
};

/** Ambisonic degree each ACN channel belongs to, i.e., floor(sqrt(n)). */
const int DEGREE_OF[NUM_CHANNELS] = {0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3};

/** Ramp of the per-degree gains when Amount or Slope changes, in gain units per
 * second (5 ms, short enough to feel immediate and long enough to avoid
 * clicks, which W and the first degree are especially prone to here). */
const float GAIN_RAMP_RATE = 1.f / 0.005f;

/** Width of the fade between degrees on the blur side, over the whole travel of
 * the Slope knob. The upper end is narrowed to the order of the input signal,
 * which is as wide as the fade can be. */
const float SLOPE_MIN = 0.25f;
const float SLOPE_MAX = 3.f;

/** Loudest the blur side may turn the surviving degrees up, at any order and
 * any setting. Preserving the energy of the sound field exactly asks for up to
 * order + 1, i.e., +12 dB on W at order 3, which clips the synthetic and
 * already loud material a modular patch tends to carry. +3 dB still reads as
 * "nothing was lost" and leaves a signal at the 5 V audio standard well inside
 * the +/-10 V Rack allows. */
const float MAX_BLUR_BOOST = 1.4142f;

/** How far the high-pass ramp of the sharpen side goes at full right. Short of
 * 1, which would take the omni channel out altogether: the useful range of the
 * ramp is roughly a third to two thirds of it, and leaving W in place keeps the
 * process reversible and the overall level from collapsing. */
const float SHARP_MAX = 0.7f;

} // namespace


struct SP4TBlurSharp : Module {
	enum ParamId {
		AMOUNT_PARAM,
		AMOUNT_CV_PARAM,
		SLOPE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
		AMOUNT_CV_INPUT,
		SLOPE_CV_INPUT,
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

	/** Gain of each ambisonic degree and the value it ramps to. */
	float gains[NUM_DEGREES] = {1.f, 1.f, 1.f, 1.f};
	float gainTargets[NUM_DEGREES] = {1.f, 1.f, 1.f, 1.f};
	/** Whether a degree is ramping, i.e., whether the ramps must be advanced. */
	bool ramping = false;

	/** Number of channels of the input signal, read at control rate. */
	int numChannels = 0;

	/** Set while there is nothing to process or nowhere to send it. */
	bool idle = false;

	SP4TBlurSharp() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(AMOUNT_PARAM, -1.f, 1.f, 0.f, "Blur (left) / sharpen (right)", "%", 0.f, 100.f);
		configParam(AMOUNT_CV_PARAM, -1.f, 1.f, 0.f, "Amount CV attenuverter", "%", 0.f, 100.f);
		configParam(SLOPE_PARAM, SLOPE_MIN, SLOPE_MAX, 1.f, "Blur slope");

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configInput(AMOUNT_CV_INPUT, "Amount CV");
		configInput(SLOPE_CV_INPUT, "Slope CV");
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");
		configBypass(AMBI_INPUT, AMBI_OUTPUT);

		controlDivider.setDivision(16);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		updateControls();
		for (int l = 0; l < NUM_DEGREES; l++)
			gains[l] = gainTargets[l];
		ramping = false;
	}

	/** Reads the knobs and the CV inputs, then the gains they ask for. The
	 * controls do not need audio-rate accuracy: what follows them is a set of
	 * gains that is ramped to anyway. */
	void updateControls() {
		numChannels = inputs[AMBI_INPUT].getChannels();
		outputs[AMBI_OUTPUT].setChannels(numChannels);

		// The order of the input signal is given by the number of channels of
		// the polyphonic cable, i.e., 4 (FOA), 9 (HOA-2) or 16 (HOA-3).
		int order = math::clamp((int) std::floor(std::sqrt((float) numChannels)) - 1, 0, NUM_DEGREES - 1);

		// The amount CV is bipolar: 5 V spans the whole range of the knob in
		// either direction with the attenuverter wide open.
		float amount = params[AMOUNT_PARAM].getValue()
		               + params[AMOUNT_CV_PARAM].getValue() * inputs[AMOUNT_CV_INPUT].getVoltage() / 5.f;
		amount = math::clamp(amount, -1.f, 1.f);

		// The slope CV is unipolar, and has no attenuverter of its own: its full
		// scale of 10 V spans the whole range of the knob.
		float slope =
			params[SLOPE_PARAM].getValue() + (SLOPE_MAX - SLOPE_MIN) * inputs[SLOPE_CV_INPUT].getVoltage() / 10.f;
		slope = math::clamp(slope, SLOPE_MIN, std::fmax((float) order, SLOPE_MIN));

		setGainTargets(order, amount, slope);

		ramping = false;
		for (int l = 0; l < NUM_DEGREES; l++)
			ramping = ramping || gains[l] != gainTargets[l];
	}

	/** One real gain per ambisonic degree, shared by the 2l+1 channels of that
	 * degree: a linear, rotation-invariant filter on the sphere, which acts on
	 * the whole sound field, direct and diffuse alike, and leaves its order
	 * untouched. */
	void setGainTargets(int order, float amount, float slope) {
		// The degrees the input signal does not carry are left at unity, so that
		// a lower-order signal patched in later starts from a clean slate.
		for (int l = 0; l < NUM_DEGREES; l++)
			gainTargets[l] = 1.f;

		// There is nothing to blur or to sharpen with the omni channel alone,
		// and the two sides meet at unity, so crossing the centre is seamless.
		if (order == 0 || amount == 0.f)
			return;

		if (amount < 0.f) {
			// Blur: a cutoff moving down from the top degree to 0, the degrees
			// fading out over a window `slope` wide as it passes them.
			float cutoff = order + amount * (order + slope - 1.f);
			float weights[NUM_DEGREES] = {};
			float energy = 0.f;
			for (int l = 0; l <= order; l++) {
				float u = math::clamp((cutoff - l) / slope + 1.f, 0.f, 1.f);
				weights[l] = u * u * (3.f - 2.f * u);
				energy += (2 * l + 1) * weights[l] * weights[l];
			}
			// The surviving degrees rise together as the others fade. Preserving
			// the energy exactly would ask for a factor anywhere from 1 to
			// order + 1, so that factor is remapped onto 1 to MAX_BLUR_BOOST
			// instead: the balance between the degrees stays the one the
			// weights ask for and only the level it is applied at is held back,
			// the same ceiling at every order. W is never faded, hence there is
			// always energy to divide by.
			float norm = std::sqrt((order + 1) * (order + 1) / energy);
			float boost = std::pow(MAX_BLUR_BOOST, std::log(norm) / std::log((float) (order + 1)));
			for (int l = 0; l <= order; l++)
				gainTargets[l] = weights[l] * boost;
		}
		else {
			// Sharpen: a fixed high-pass ramp over the degrees, which ignores
			// the slope, unity on the top one. Not normalized, so the overall
			// level drops as W is taken out, as a high-pass is expected to.
			float sharp = SHARP_MAX * amount;
			for (int l = 0; l <= order; l++)
				gainTargets[l] = (1.f - sharp) + sharp * l / order;
		}
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		if (!inputs[AMBI_INPUT].isConnected() || !outputs[AMBI_OUTPUT].isConnected()) {
			// The ramps jump to where they were heading: the gains in force when
			// the module is patched back in are the ones its controls ask for,
			// not the ones it was ramping through when it was unpatched.
			if (!idle) {
				idle = true;
				for (int l = 0; l < NUM_DEGREES; l++)
					gains[l] = gainTargets[l];
				ramping = false;
			}
			return;
		}
		idle = false;

		// Advancing the ramps is skipped while no degree is ramping, which is
		// most of the time.
		if (ramping) {
			float step = args.sampleTime * GAIN_RAMP_RATE;
			ramping = false;
			for (int l = 0; l < NUM_DEGREES; l++) {
				float target = gainTargets[l];
				gains[l] = gains[l] < target ? std::fmin(gains[l] + step, target) : std::fmax(gains[l] - step, target);
				ramping = ramping || gains[l] != target;
			}
		}

		for (int c = 0; c < numChannels; c++)
			outputs[AMBI_OUTPUT].setVoltage(inputs[AMBI_INPUT].getVoltage(c) * gains[DEGREE_OF[c]], c);
	}
};


struct SP4TBlurSharpWidget : ModuleWidget {
	SP4TBlurSharpWidget(SP4TBlurSharp* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TBlurSharp.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TBlurSharp-dark.svg")));

		// 4 HP: two screws, on the left.
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(
			createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(10.16, 31.99)), module, SP4TBlurSharp::AMOUNT_PARAM));
		addParam(
			createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(10.16, 50.422)), module, SP4TBlurSharp::AMOUNT_CV_PARAM));
		addInput(
			createInputCentered<ThemedPJ301MPort>(mm2px(Vec(10.16, 60.901)), module, SP4TBlurSharp::AMOUNT_CV_INPUT));

		addParam(
			createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(10.16, 78.971)), module, SP4TBlurSharp::SLOPE_PARAM));
		addInput(
			createInputCentered<ThemedPJ301MPort>(mm2px(Vec(10.16, 89.45)), module, SP4TBlurSharp::SLOPE_CV_INPUT));

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(5.745, 114.0)), module, SP4TBlurSharp::AMBI_INPUT));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(14.574, 114.0)), module, SP4TBlurSharp::AMBI_OUTPUT));
	}
};


Model* modelNun4takSP4TBlurSharp = createModel<SP4TBlurSharp, SP4TBlurSharpWidget>("SP4T-Blur-Sharp");
