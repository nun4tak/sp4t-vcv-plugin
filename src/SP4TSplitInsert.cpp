#include "plugin.hpp"
#include "components.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>


namespace {

/** Largest delay compensation, in samples: covers the latency of the usual
 * block-based effects (the afSTFT modules of this plugin report 1664 samples). */
const int MAX_DELAY = 4096;

/** Knob position to a whole number of samples, square law: the first samples
 * take as much of the travel as the last hundreds, so that the compensation of
 * a zero-latency insert (down to the single sample of a cable) can be dialed in
 * as precisely as the latency of a block-based one. */
int delaySamples(float position) {
	return (int) std::round(MAX_DELAY * position * position);
}

/** Delay compensation knob: whole samples, with the taper above. */
struct DelayQuantity : ParamQuantity {
	float getDisplayValue() override { return delaySamples(getValue()); }

	void setDisplayValue(float displayValue) override {
		float samples = math::clamp(displayValue, 0.f, (float) MAX_DELAY);
		setImmediateValue(std::sqrt(samples / MAX_DELAY));
	}

	std::string getUnit() override { return getDisplayValue() == 1.f ? " sample" : " samples"; }
};

} // namespace


struct SP4TSplitInsert : Module {
	enum {
		NUM_CHANNELS = 16,
		NUM_INSERTS = 2
	};

	/** Insert 1 takes the omni-directional channel, insert 2 all the others. */
	enum {
		OMNI = 0,
		DIRECTIONAL = 1
	};

	enum ParamId {
		INSERT1_PRE_PARAM,
		INSERT2_PRE_PARAM,
		INSERT1_POST_PARAM,
		INSERT2_POST_PARAM,
		INSERT1_DELAY_PARAM,
		INSERT2_DELAY_PARAM,
		DRY_WET_PARAM,
		DRY_WET_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AMBI_INPUT,
		INSERT1_PRE_CV_INPUT,
		INSERT2_PRE_CV_INPUT,
		INSERT1_RETURN_INPUT,
		INSERT2_RETURN_INPUT,
		DRY_WET_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		INSERT1_SEND_OUTPUT,
		INSERT2_SEND_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	/** Whole-sample delay lines, one per ambisonic channel, sharing a write
	 * position. */
	struct DelayLines {
		float buffer[NUM_CHANNELS][MAX_DELAY + 1] = {};
		int writeIndex = 0;

		void clear() {
			std::memset(buffer, 0, sizeof(buffer));
			writeIndex = 0;
		}

		void write(int channel, float value) { buffer[channel][writeIndex] = value; }

		/** Reads what was written `delay` samples ago, the sample just written
		 * for a delay of 0. */
		float read(int channel, int delay) const {
			int index = writeIndex - delay;
			if (index < 0)
				index += MAX_DELAY + 1;
			return buffer[channel][index];
		}

		void advance() {
			if (++writeIndex > MAX_DELAY)
				writeIndex = 0;
		}
	};

	dsp::ClockDivider controlDivider;

	/** Dry signals, held until the longest insert has caught up, and signals
	 * coming back from the inserts, held until theirs has. */
	DelayLines dryLines;
	DelayLines wetLines;

	/** Number of channels of the input signal, read at control rate. */
	int numChannels = 0;
	int lastNumChannels = 0;
	/** Gains of each insert, set at control rate. */
	float preGains[NUM_INSERTS] = {};
	float postGains[NUM_INSERTS] = {};
	/** Whether each insert has its return patched, i.e., is not passed thru. */
	bool returnPatched[NUM_INSERTS] = {};
	/** Latency each insert is compensated for, and how long what comes back
	 * from it is then held. */
	int latencies[NUM_INSERTS] = {};
	int wetDelays[NUM_INSERTS] = {};
	/** Latency of the signal path, i.e., the compensation of the longest of the
	 * two inserts: what the module aligns both of them to, not what it adds. */
	int latency = 0;
	/** Dry/wet crossfading gains, set at control rate. */
	float dryGain = 0.f;
	float wetGain = 1.f;
	/** Set while there is nothing to split. */
	bool idle = false;

	SP4TSplitInsert() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		const char* inserts[NUM_INSERTS] = {"Omni", "Directional"};
		for (int i = 0; i < NUM_INSERTS; i++) {
			std::string insert = inserts[i];
			configParam(INSERT1_PRE_PARAM + i, 0.f, 2.f, 1.f, insert + " pre gain", "x");
			configParam(INSERT1_POST_PARAM + i, 0.f, 2.f, 1.f, insert + " post gain", "x");
			configParam<DelayQuantity>(INSERT1_DELAY_PARAM + i, 0.f, 1.f, 0.f, insert + " delay compensation");
			configInput(INSERT1_PRE_CV_INPUT + i, insert + " pre gain CV");
			configInput(INSERT1_RETURN_INPUT + i, insert + " return");
			configOutput(INSERT1_SEND_OUTPUT + i, insert + " send");
		}
		configParam(DRY_WET_PARAM, 0.f, 1.f, 1.f, "Dry/wet", "%", 0.f, 100.f);
		configParam(DRY_WET_CV_PARAM, -1.f, 1.f, 0.f, "Dry/wet CV attenuverter", "%", 0.f, 100.f);

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configInput(DRY_WET_CV_INPUT, "Dry/wet CV");
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");
		configBypass(AMBI_INPUT, AMBI_OUTPUT);

		controlDivider.setDivision(16);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		dryLines.clear();
		wetLines.clear();
		updateControls();
	}

	void updateControls() {
		numChannels = inputs[AMBI_INPUT].getChannels();
		outputs[AMBI_OUTPUT].setChannels(numChannels);
		outputs[INSERT1_SEND_OUTPUT].setChannels(1);
		outputs[INSERT2_SEND_OUTPUT].setChannels(std::max(numChannels - 1, 0));

		// A change of ambisonic order repatches the chains: what the delay lines
		// still hold belongs to the previous signal.
		if (numChannels != lastNumChannels) {
			lastNumChannels = numChannels;
			dryLines.clear();
			wetLines.clear();
		}

		latency = 0;
		for (int i = 0; i < NUM_INSERTS; i++) {
			// The pre-gain CV input is normalled to 5 V, so that the gain is the
			// one of the knob with nothing patched.
			preGains[i] =
				params[INSERT1_PRE_PARAM + i].getValue() * inputs[INSERT1_PRE_CV_INPUT + i].getNormalVoltage(5.f) / 5.f;
			postGains[i] = params[INSERT1_POST_PARAM + i].getValue();

			// The knob always counts, patched or not: a send normalled to its
			// return is compensated like an external insert of the latency it
			// declares, so that plugging one in changes nothing else.
			returnPatched[i] = inputs[INSERT1_RETURN_INPUT + i].isConnected();
			latencies[i] = delaySamples(params[INSERT1_DELAY_PARAM + i].getValue());
			latency = std::max(latency, latencies[i]);
		}

		// What comes back from an insert is held until the other one has caught
		// up. A return carries the latency its knob declares, so it waits for
		// what separates the two; a send normalled to its return carries none,
		// so it waits for the whole of it: the pass-thru then behaves as the
		// plain delay the knob says the insert chain is.
		for (int i = 0; i < NUM_INSERTS; i++)
			wetDelays[i] = returnPatched[i] ? latency - latencies[i] : latency;

		// The dry/wet CV is unipolar: its full scale of 10 V spans the whole
		// range of the knob with the attenuverter wide open.
		float mix = params[DRY_WET_PARAM].getValue()
		            + params[DRY_WET_CV_PARAM].getValue() * inputs[DRY_WET_CV_INPUT].getVoltage() / 10.f;
		mix = math::clamp(mix, 0.f, 1.f);

		// Linear crossfade: the dry and the wet signals of an insert are
		// correlated, which an equal-power crossfade would bump up at the centre.
		dryGain = 1.f - mix;
		wetGain = mix;
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		// Nothing to split. What the delay lines still hold is dropped rather
		// than left to play out, so that reconnecting starts from silence.
		if (numChannels == 0) {
			if (!idle) {
				idle = true;
				outputs[AMBI_OUTPUT].clearVoltages();
				outputs[INSERT1_SEND_OUTPUT].clearVoltages();
				outputs[INSERT2_SEND_OUTPUT].clearVoltages();
				dryLines.clear();
				wetLines.clear();
			}
			return;
		}
		idle = false;

		for (int c = 0; c < numChannels; c++) {
			// The omni channel goes through the first insert, the directional
			// ones through the second, where they keep their ACN order.
			int insert = c == 0 ? OMNI : DIRECTIONAL;
			int sendChannel = c == 0 ? 0 : c - 1;

			float dry = inputs[AMBI_INPUT].getVoltage(c);
			float send = dry * preGains[insert];
			outputs[INSERT1_SEND_OUTPUT + insert].setVoltage(send, sendChannel);

			// The send is normalled to the return: with nothing patched the
			// insert passes through, pre-gain and post-gain still in force.
			float returned =
				returnPatched[insert] ? inputs[INSERT1_RETURN_INPUT + insert].getVoltage(sendChannel) : send;

			// Both signals come out delayed by the same number of samples, so
			// the post-gain and the dry/wet mix act on what the two inserts
			// return at the very same moment.
			dryLines.write(c, dry);
			wetLines.write(c, returned);

			float wet = wetLines.read(c, wetDelays[insert]) * postGains[insert];
			float out = dryLines.read(c, latency) * dryGain + wet * wetGain;
			outputs[AMBI_OUTPUT].setVoltage(out, c);
		}

		dryLines.advance();
		wetLines.advance();
	}
};


struct SP4TSplitInsertWidget : ModuleWidget {
	SP4TSplitInsertWidget(SP4TSplitInsert* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TSplitInsert.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TSplitInsert-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// One section per insert: the send and the return in the middle of the
		// top row, the gains on either side and, below, the pre-gain CV input on
		// the left and the delay compensation on the right.
		const float sendY[] = {28.039f, 62.157f};
		const float cvY[] = {40.956f, 75.074f};

		for (int i = 0; i < SP4TSplitInsert::NUM_INSERTS; i++) {
			addParam(createParamCentered<SP4TRoundSmallKnob>(
				mm2px(Vec(8.756, sendY[i])), module, SP4TSplitInsert::INSERT1_PRE_PARAM + i));
			addOutput(createOutputCentered<ThemedPJ301MPort>(
				mm2px(Vec(19.852, sendY[i])), module, SP4TSplitInsert::INSERT1_SEND_OUTPUT + i));
			addInput(createInputCentered<ThemedPJ301MPort>(
				mm2px(Vec(30.948, sendY[i])), module, SP4TSplitInsert::INSERT1_RETURN_INPUT + i));
			addParam(createParamCentered<SP4TRoundSmallKnob>(
				mm2px(Vec(42.043, sendY[i])), module, SP4TSplitInsert::INSERT1_POST_PARAM + i));

			addInput(createInputCentered<ThemedPJ301MPort>(
				mm2px(Vec(8.756, cvY[i])), module, SP4TSplitInsert::INSERT1_PRE_CV_INPUT + i));
			addParam(createParamCentered<SP4TRoundSmallKnob>(
				mm2px(Vec(42.043, cvY[i])), module, SP4TSplitInsert::INSERT1_DELAY_PARAM + i));
		}

		addParam(createParamCentered<SP4TRoundLargeKnob>(
			mm2px(Vec(10.424, 94.126)), module, SP4TSplitInsert::DRY_WET_PARAM));
		addParam(createParamCentered<SP4TRoundSmallKnob>(
			mm2px(Vec(30.947, 94.19)), module, SP4TSplitInsert::DRY_WET_CV_PARAM));
		addInput(
			createInputCentered<ThemedPJ301MPort>(mm2px(Vec(42.043, 94.1)), module, SP4TSplitInsert::DRY_WET_CV_INPUT));

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(8.756, 114.0)), module, SP4TSplitInsert::AMBI_INPUT));
		addOutput(createOutputCentered<SP4TAmbiPort>(mm2px(Vec(42.043, 114.0)), module, SP4TSplitInsert::AMBI_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		SP4TSplitInsert* module = getModule<SP4TSplitInsert>();

		// The latency of the inserts, which both chains are aligned to, and not
		// one the module adds on top of them.
		int latency = module->latency;
		float milliseconds = 1e3f * latency / APP->engine->getSampleRate();
		menu->addChild(new MenuSeparator);
		menu->addChild(
			createMenuLabel(string::f("Latency for the insert chain: %d samples (%.1f ms)", latency, milliseconds)));
	}
};


Model* modelNun4takSP4TSplitInsert = createModel<SP4TSplitInsert, SP4TSplitInsertWidget>("SP4T-Split-Insert");
