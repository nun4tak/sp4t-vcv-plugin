#include "plugin.hpp"
#include "components.hpp"
#include "ScreenStyle.hpp"
#include "AmbisonicDisplay.hpp"
#include "AmbiSpeakerDecoder.hpp"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


namespace {

using Decoder = sp4t::AmbiSpeakerDecoder;

enum {
	NUM_CHANNELS = Decoder::MAX_CHANNELS,
	ROWS_PER_PAGE = 4,
	NUM_PAGES = NUM_CHANNELS / ROWS_PER_PAGE,
};

/** What a channel carries. The two LFE states differ only in what toggling
 * LFE off goes back to, which is what the angles of the channel mean while it
 * is an LFE: a direction it had before, or nothing. */
enum ChannelState {
	CHANNEL_UNUSED,
	CHANNEL_SPEAKER,
	CHANNEL_LFE,
	CHANNEL_LFE_WITH_ANGLES,
	NUM_CHANNEL_STATES
};

bool isLfe(int state) {
	return state == CHANNEL_LFE || state == CHANNEL_LFE_WITH_ANGLES;
}

/** What the LFE channels carry. */
enum LfeMode {
	LFE_SILENT,
	LFE_OMNI,
	NUM_LFE_MODES
};

/** Cutoff of the LFE low-pass, in Hz, and its 4th-order Linkwitz-Riley Q. */
const float LFE_CUTOFF = 120.f;
const float LR4_Q = 0.70710678f;

/** How long the decoder keeps running after the last cable has gone, in
 * seconds: enough for the crossover filters to ring out. */
const float FLUSH_TIME = 0.1f;

const float CROSSOVER_FREQS[] = {200.f, 400.f, 800.f, 1600.f};
const int NUM_CROSSOVER_FREQS = 4;

// ------------------------------------------------------------- presets ----

struct PresetChannel {
	float azimuth;
	float elevation;
	bool lfe;
};

struct LayoutPreset {
	const char* name;
	int numChannels;
	PresetChannel channels[NUM_CHANNELS];
};

/** Speaker layouts, with the angles of SAF's loudspeaker presets where there
 * is one. Azimuth is positive to the left, as everywhere else in the plugin,
 * which is also SAF's convention.
 *
 * The surround layouts follow the SMPTE channel order (L R C LFE Ls Rs ...),
 * not the order of SAF's own arrays, which differs from one preset to the
 * next. The rings start with a speaker dead ahead, as ambisonic rings usually
 * do, while the surround layouts start with a front pair.
 */
const float TOP = 45.f;
const float CUBE_ELEV = 35.264f;
const float ICO_ELEV = 26.565f;

const LayoutPreset LAYOUT_PRESETS[] = {
	{"Stereo", 2, {{30, 0, false}, {-30, 0, false}}},
	{"LCR", 3, {{30, 0, false}, {-30, 0, false}, {0, 0, false}}},
	{"Quad", 4, {{45, 0, false}, {-45, 0, false}, {135, 0, false}, {-135, 0, false}}},
	{"5.0", 5, {{30, 0, false}, {-30, 0, false}, {0, 0, false}, {110, 0, false}, {-110, 0, false}}},
	{"5.1", 6, {{30, 0, false}, {-30, 0, false}, {0, 0, false}, {0, 0, true}, {110, 0, false}, {-110, 0, false}}},
	{"7.0",
	 7,
	 {{30, 0, false},
	  {-30, 0, false},
	  {0, 0, false},
	  {90, 0, false},
	  {-90, 0, false},
	  {135, 0, false},
	  {-135, 0, false}}},
	{"7.1",
	 8,
	 {{30, 0, false},
	  {-30, 0, false},
	  {0, 0, false},
	  {0, 0, true},
	  {90, 0, false},
	  {-90, 0, false},
	  {135, 0, false},
	  {-135, 0, false}}},
	{"5.1.4",
	 10,
	 {{30, 0, false},
	  {-30, 0, false},
	  {0, 0, false},
	  {0, 0, true},
	  {110, 0, false},
	  {-110, 0, false},
	  {45, TOP, false},
	  {-45, TOP, false},
	  {135, TOP, false},
	  {-135, TOP, false}}},
	{"7.1.4",
	 12,
	 {{30, 0, false},
	  {-30, 0, false},
	  {0, 0, false},
	  {0, 0, true},
	  {90, 0, false},
	  {-90, 0, false},
	  {135, 0, false},
	  {-135, 0, false},
	  {45, TOP, false},
	  {-45, TOP, false},
	  {135, TOP, false},
	  {-135, TOP, false}}},
	{"Hexagon",
	 6,
	 {{0, 0, false}, {60, 0, false}, {120, 0, false}, {180, 0, false}, {-120, 0, false}, {-60, 0, false}}},
	{"Octagon",
	 8,
	 {{0, 0, false},
	  {45, 0, false},
	  {90, 0, false},
	  {135, 0, false},
	  {180, 0, false},
	  {-135, 0, false},
	  {-90, 0, false},
	  {-45, 0, false}}},
	{"Ring 12",
	 12,
	 {{0, 0, false},
	  {30, 0, false},
	  {60, 0, false},
	  {90, 0, false},
	  {120, 0, false},
	  {150, 0, false},
	  {180, 0, false},
	  {-150, 0, false},
	  {-120, 0, false},
	  {-90, 0, false},
	  {-60, 0, false},
	  {-30, 0, false}}},
	{"Ring 16",
	 16,
	 {{0, 0, false},
	  {22.5f, 0, false},
	  {45, 0, false},
	  {67.5f, 0, false},
	  {90, 0, false},
	  {112.5f, 0, false},
	  {135, 0, false},
	  {157.5f, 0, false},
	  {180, 0, false},
	  {-157.5f, 0, false},
	  {-135, 0, false},
	  {-112.5f, 0, false},
	  {-90, 0, false},
	  {-67.5f, 0, false},
	  {-45, 0, false},
	  {-22.5f, 0, false}}},
	{"Octahedron",
	 6,
	 {{0, 0, false}, {90, 0, false}, {180, 0, false}, {-90, 0, false}, {0, 90, false}, {0, -90, false}}},
	{"Cube",
	 8,
	 {{45, CUBE_ELEV, false},
	  {-45, CUBE_ELEV, false},
	  {135, CUBE_ELEV, false},
	  {-135, CUBE_ELEV, false},
	  {45, -CUBE_ELEV, false},
	  {-45, -CUBE_ELEV, false},
	  {135, -CUBE_ELEV, false},
	  {-135, -CUBE_ELEV, false}}},
	{"Icosahedron",
	 12,
	 {{0, 90, false},
	  {0, ICO_ELEV, false},
	  {72, ICO_ELEV, false},
	  {144, ICO_ELEV, false},
	  {-144, ICO_ELEV, false},
	  {-72, ICO_ELEV, false},
	  {36, -ICO_ELEV, false},
	  {108, -ICO_ELEV, false},
	  {180, -ICO_ELEV, false},
	  {-108, -ICO_ELEV, false},
	  {-36, -ICO_ELEV, false},
	  {0, -90, false}}},
};

const int NUM_LAYOUT_PRESETS = (int) (sizeof(LAYOUT_PRESETS) / sizeof(LAYOUT_PRESETS[0]));
/** The layout a new module starts with, and what "Initialize" goes back to. */
const int DEFAULT_PRESET = 2; // Quad

// -------------------------------------------------------------- screen ----

/** Ayu purple, telling the speakers of this module apart from the orange
 * sources of the encoders. */
const NVGcolor SPEAKER_COLOR = nvgRGB(0xd4, 0xbf, 0xff);
/** Ayu orange, for the LFE channels. */
const NVGcolor LFE_COLOR = nvgRGB(0xff, 0xb4, 0x54);
/** Dimmer than SCREEN_TEXT_DIM: the rows that carry nothing. */
const NVGcolor UNUSED_COLOR = nvgRGB(0x4d, 0x53, 0x5c);

/** Columns of the channel table: centre and width, in mm. */
struct Column {
	float centre;
	float width;
};
const Column COLUMN_CH = {5.228f, 7.0f};
const Column COLUMN_AZ = {16.428f, 13.6f};
const Column COLUMN_ELEV = {31.828f, 13.6f};
const Column COLUMN_LFE = {43.428f, 7.6f};

const float TABLE_X = 1.728f;
const float TABLE_Y = 70.4f;
const float TABLE_W = 45.6f;
const float TABLE_H = 29.6f;
const float ROW_H = TABLE_H / ROWS_PER_PAGE;

const float VALUE_FONT_SIZE = 10.5f;

} // namespace


struct SP4TSpeakersOut : Module {
	enum ParamId {
		PREV_LAYOUT_PARAM,
		NEXT_LAYOUT_PARAM,
		PREV_PAGE_PARAM,
		NEXT_PAGE_PARAM,
		CH_AZ_PARAM,
		CH_ELEV_PARAM = CH_AZ_PARAM + NUM_CHANNELS,
		CH_STATE_PARAM = CH_ELEV_PARAM + NUM_CHANNELS,
		PARAMS_LEN = CH_STATE_PARAM + NUM_CHANNELS
	};
	enum InputId {
		AMBI_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		CH_OUTPUT,
		POLY_OUTPUT = CH_OUTPUT + NUM_CHANNELS,
		OUTPUTS_LEN
	};
	enum LightId {
		CH_LED_LIGHT,
		PAGE_LED_LIGHT = CH_LED_LIGHT + NUM_CHANNELS,
		LIGHTS_LEN = PAGE_LED_LIGHT + NUM_PAGES
	};

	Decoder decoder;
	Decoder::Options options;
	dsp::ClockDivider controlDivider;
	dsp::ClockDivider lightDivider;

	/** Page of the channel table on show: UI state, saved with the patch. */
	int page = 0;
	/** sp4t::AmbisonicDisplay::ViewMode */
	int viewMode = sp4t::AmbisonicDisplay::VIEW_XY_Z_PROJECTIONS;
	int lfeMode = LFE_SILENT;

	/** Highest used channel, i.e. how many the POLY output carries. */
	int numChannels = 0;
	/** Channels carrying the LFE feed, read once per control tick. */
	bool lfeChannel[NUM_CHANNELS] = {};

	/** Peak of each channel output since the last light update. */
	float peaks[NUM_CHANNELS] = {};

	/** 4th-order Linkwitz-Riley low-pass of the LFE feed. */
	dsp::BiquadFilter lfeLowPass[2];

	/** Set while the decoding is skipped, i.e. while there is nothing to
	 * decode. */
	bool idle = false;
	int flushSamples = 0;
	int flushLength = 0;

	// Designing a decoding matrix takes a few milliseconds and allocates, so
	// it is left to a worker thread, as in the Ears Out module.
	std::thread worker;
	std::mutex workerMutex;
	std::condition_variable workerCv;
	Decoder::Options workerOptions;
	bool workerDirty = false;
	bool workerQuit = false;

	SP4TSpeakersOut() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configButton(PREV_LAYOUT_PARAM, "Previous layout");
		configButton(NEXT_LAYOUT_PARAM, "Next layout");
		configButton(PREV_PAGE_PARAM, "Previous page");
		configButton(NEXT_PAGE_PARAM, "Next page");
		// "Randomize" would press them, which turns the page or loads a layout.
		for (int p = PREV_LAYOUT_PARAM; p <= NEXT_PAGE_PARAM; p++)
			paramQuantities[p]->randomizeEnabled = false;

		const LayoutPreset& preset = LAYOUT_PRESETS[DEFAULT_PRESET];
		for (int c = 0; c < NUM_CHANNELS; c++) {
			bool used = c < preset.numChannels;
			float az = used ? preset.channels[c].azimuth : 0.f;
			float elev = used ? preset.channels[c].elevation : 0.f;
			int state = !used ? CHANNEL_UNUSED : (preset.channels[c].lfe ? CHANNEL_LFE : CHANNEL_SPEAKER);

			configParam(CH_AZ_PARAM + c, -180.f, 180.f, az, string::f("Channel %d azimuth", c + 1), "°");
			configParam(CH_ELEV_PARAM + c, -90.f, 90.f, elev, string::f("Channel %d elevation", c + 1), "°");
			configSwitch(CH_STATE_PARAM + c,
			             0.f,
			             (float) (NUM_CHANNEL_STATES - 1),
			             (float) state,
			             string::f("Channel %d", c + 1),
			             {"Unused", "Speaker", "LFE", "LFE"});

			// A layout is typed in, not rolled: "Randomize" leaves it alone.
			paramQuantities[CH_AZ_PARAM + c]->randomizeEnabled = false;
			paramQuantities[CH_ELEV_PARAM + c]->randomizeEnabled = false;
			paramQuantities[CH_STATE_PARAM + c]->randomizeEnabled = false;

			configOutput(CH_OUTPUT + c, string::f("Channel %d", c + 1));
			configLight(CH_LED_LIGHT + c, string::f("Channel %d level", c + 1));
		}

		configInput(AMBI_INPUT, "Ambisonic (ACN/SN3D)");
		configOutput(POLY_OUTPUT, "All channels (polyphonic)");
		for (int p = 0; p < NUM_PAGES; p++)
			configLight(PAGE_LED_LIGHT + p, string::f("Table page %d", p + 1));

		controlDivider.setDivision(16);
		lightDivider.setDivision(256);

		worker = std::thread(&SP4TSpeakersOut::workerLoop, this);
	}

	~SP4TSpeakersOut() {
		{
			std::lock_guard<std::mutex> lock(workerMutex);
			workerQuit = true;
		}
		workerCv.notify_one();
		worker.join();
	}

	void workerLoop() {
		std::unique_lock<std::mutex> lock(workerMutex);
		while (true) {
			workerCv.wait(lock, [this] { return workerQuit || workerDirty; });
			if (workerQuit)
				break;
			Decoder::Options pending = workerOptions;
			workerDirty = false;
			lock.unlock();
			decoder.build(pending);
			lock.lock();
		}
	}

	void requestBuild() {
		{
			std::lock_guard<std::mutex> lock(workerMutex);
			workerOptions = options;
			workerDirty = true;
		}
		workerCv.notify_one();
	}

	int getChannelState(int channel) { return (int) std::round(params[CH_STATE_PARAM + channel].getValue()); }

	/** Reads the channel parameters into the decoding options, and asks for a
	 * new set of matrices if they changed. */
	void updateLayout() {
		Decoder::Options newOptions = options;
		newOptions.numChannels = 0;
		for (int c = 0; c < NUM_CHANNELS; c++) {
			int state = getChannelState(c);
			newOptions.isSpeaker[c] = state == CHANNEL_SPEAKER;
			lfeChannel[c] = isLfe(state);
			newOptions.azimuth[c] = params[CH_AZ_PARAM + c].getValue();
			newOptions.elevation[c] = params[CH_ELEV_PARAM + c].getValue();
			if (state != CHANNEL_UNUSED)
				newOptions.numChannels = c + 1;
		}
		numChannels = newOptions.numChannels;
		setOptions(newOptions);
	}

	void setOptions(const Decoder::Options& newOptions) {
		if (newOptions == options)
			return;
		options = newOptions;
		requestBuild();
	}

	/** Also called when the module is added to the engine, i.e. this is where
	 * the first set of matrices is requested. */
	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		Decoder::Options newOptions = options;
		newOptions.sampleRate = e.sampleRate;
		setOptions(newOptions);

		flushLength = (int) (FLUSH_TIME * e.sampleRate);
		float f = LFE_CUTOFF / e.sampleRate;
		for (int i = 0; i < 2; i++)
			lfeLowPass[i].setParameters(dsp::BiquadFilter::LOWPASS, f, LR4_Q, 1.f);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		page = 0;
		viewMode = sp4t::AmbisonicDisplay::VIEW_XY_Z_PROJECTIONS;
		lfeMode = LFE_SILENT;

		Decoder::Options defaults;
		defaults.sampleRate = options.sampleRate;
		options = defaults;
		updateLayout();
		requestBuild();
		decoder.reset();
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process() || idle) {
			updateLayout();
			// The order of the input signal is given by the number of channels
			// of the polyphonic cable, i.e., 4 (FOA), 9 (HOA-2) or 16 (HOA-3).
			int channels = inputs[AMBI_INPUT].getChannels();
			decoder.setOrder((int) std::floor(std::sqrt((float) channels)) - 1);
		}

		// Decoding starts as soon as there is something to decode, whether or
		// not an output is patched, so that the LEDs answer the input. This is
		// what every decoder of the plugin does.
		bool connected = inputs[AMBI_INPUT].isConnected() && numChannels > 0;

		// Keep decoding until the crossover filters have rung out, so that
		// unplugging a cable does not cut the output short.
		if (connected)
			flushSamples = flushLength;
		else if (flushSamples > 0)
			flushSamples--;

		if (!connected && flushSamples == 0) {
			if (!idle) {
				idle = true;
				decoder.reset();
				lfeLowPass[0].reset();
				lfeLowPass[1].reset();
				for (int c = 0; c < NUM_CHANNELS; c++) {
					peaks[c] = 0.f;
					lights[CH_LED_LIGHT + c].setBrightness(0.f);
					outputs[CH_OUTPUT + c].setVoltage(0.f);
				}
				outputs[POLY_OUTPUT].setChannels(std::max(numChannels, 1));
				outputs[POLY_OUTPUT].clearVoltages();
			}
			updatePageLights();
			return;
		}
		idle = false;

		float in[Decoder::MAX_NUM_SH] = {};
		for (int c = 0; c < decoder.getNumSH(); c++)
			in[c] = inputs[AMBI_INPUT].getVoltage(c);

		float out[Decoder::MAX_CHANNELS];
		decoder.process(in, out);

		// The decoder leaves the rows of the LFE channels silent, so they are
		// filled here: all of them carry the same feed.
		float lfe = 0.f;
		if (lfeMode == LFE_OMNI)
			lfe = lfeLowPass[1].process(lfeLowPass[0].process(in[0]));
		for (int c = 0; c < numChannels; c++) {
			if (lfeChannel[c])
				out[c] = lfe;
		}

		outputs[POLY_OUTPUT].setChannels(std::max(numChannels, 1));
		for (int c = 0; c < NUM_CHANNELS; c++) {
			float v = c < numChannels ? out[c] : 0.f;
			outputs[CH_OUTPUT + c].setVoltage(v);
			if (c < numChannels)
				outputs[POLY_OUTPUT].setVoltage(v, c);
			peaks[c] = std::fmax(peaks[c], std::fabs(v));
		}

		// The LEDs show the peak of the audio-rate channel outputs, full scale
		// at 5 V with a cube-root response so that quiet signals still show.
		if (lightDivider.process()) {
			float deltaTime = args.sampleTime * lightDivider.getDivision();
			for (int c = 0; c < NUM_CHANNELS; c++) {
				float brightness = std::cbrt(std::fmin(peaks[c] / 5.f, 1.f));
				lights[CH_LED_LIGHT + c].setBrightnessSmooth(brightness, deltaTime);
				peaks[c] = 0.f;
			}
		}
		updatePageLights();
	}

	void updatePageLights() {
		for (int p = 0; p < NUM_PAGES; p++)
			lights[PAGE_LED_LIGHT + p].setBrightness(p == page ? 1.f : 0.f);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "page", json_integer(page));
		json_object_set_new(rootJ, "viewMode", json_integer(viewMode));
		json_object_set_new(rootJ, "lfeMode", json_integer(lfeMode));
		json_object_set_new(rootJ, "method", json_integer(options.method));
		json_object_set_new(rootJ, "lowMaxRE", json_boolean(options.lowMaxRE));
		json_object_set_new(rootJ, "highMaxRE", json_boolean(options.highMaxRE));
		json_object_set_new(rootJ, "dualBand", json_boolean(options.dualBand));
		json_object_set_new(rootJ, "crossover", json_real(options.crossover));
		json_object_set_new(rootJ, "lowOrder", json_integer(options.lowOrder));
		json_object_set_new(rootJ, "highOrder", json_integer(options.highOrder));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		Decoder::Options newOptions = options;

		json_t* pageJ = json_object_get(rootJ, "page");
		if (pageJ)
			page = math::clamp((int) json_integer_value(pageJ), 0, NUM_PAGES - 1);
		json_t* viewModeJ = json_object_get(rootJ, "viewMode");
		if (viewModeJ)
			viewMode = math::clamp((int) json_integer_value(viewModeJ), 0, sp4t::AmbisonicDisplay::VIEW_MODES_LEN - 1);
		json_t* lfeModeJ = json_object_get(rootJ, "lfeMode");
		if (lfeModeJ)
			lfeMode = math::clamp((int) json_integer_value(lfeModeJ), 0, (int) NUM_LFE_MODES - 1);

		json_t* methodJ = json_object_get(rootJ, "method");
		if (methodJ) {
			int method = (int) json_integer_value(methodJ);
			if (method >= 0 && method < Decoder::NUM_METHODS)
				newOptions.method = (Decoder::Method) method;
		}
		json_t* lowMaxREJ = json_object_get(rootJ, "lowMaxRE");
		if (lowMaxREJ)
			newOptions.lowMaxRE = json_boolean_value(lowMaxREJ);
		json_t* highMaxREJ = json_object_get(rootJ, "highMaxRE");
		if (highMaxREJ)
			newOptions.highMaxRE = json_boolean_value(highMaxREJ);
		json_t* dualBandJ = json_object_get(rootJ, "dualBand");
		if (dualBandJ)
			newOptions.dualBand = json_boolean_value(dualBandJ);
		json_t* crossoverJ = json_object_get(rootJ, "crossover");
		if (crossoverJ)
			newOptions.crossover = (float) json_number_value(crossoverJ);
		json_t* lowOrderJ = json_object_get(rootJ, "lowOrder");
		if (lowOrderJ)
			newOptions.lowOrder = math::clamp((int) json_integer_value(lowOrderJ), 1, (int) Decoder::MAX_ORDER);
		json_t* highOrderJ = json_object_get(rootJ, "highOrder");
		if (highOrderJ)
			newOptions.highOrder = math::clamp((int) json_integer_value(highOrderJ), 1, (int) Decoder::MAX_ORDER);

		setOptions(newOptions);
	}
};


namespace {

/** State of a channel; without a module (the browser) the default layout. */
int channelState(SP4TSpeakersOut* module, int channel) {
	if (module)
		return module->getChannelState(channel);
	const LayoutPreset& preset = LAYOUT_PRESETS[DEFAULT_PRESET];
	if (channel >= preset.numChannels)
		return CHANNEL_UNUSED;
	return preset.channels[channel].lfe ? CHANNEL_LFE : CHANNEL_SPEAKER;
}

/** Angle of a channel; without a module, the default layout's. */
float channelAngle(SP4TSpeakersOut* module, int channel, bool elevation) {
	if (module)
		return module->params[(elevation ? SP4TSpeakersOut::CH_ELEV_PARAM : SP4TSpeakersOut::CH_AZ_PARAM) + channel]
		    .getValue();
	const LayoutPreset& preset = LAYOUT_PRESETS[DEFAULT_PRESET];
	if (channel >= preset.numChannels)
		return 0.f;
	return elevation ? preset.channels[channel].elevation : preset.channels[channel].azimuth;
}

/** Index of the preset the channels match, or -1 for a custom layout. */
int matchingPreset(SP4TSpeakersOut* module) {
	if (!module)
		return DEFAULT_PRESET;
	for (int p = 0; p < NUM_LAYOUT_PRESETS; p++) {
		const LayoutPreset& preset = LAYOUT_PRESETS[p];
		bool match = true;
		for (int c = 0; c < NUM_CHANNELS && match; c++) {
			int state = module->getChannelState(c);
			if (c >= preset.numChannels) {
				match = state == CHANNEL_UNUSED;
				continue;
			}
			if (preset.channels[c].lfe) {
				match = isLfe(state);
				continue;
			}
			if (state != CHANNEL_SPEAKER) {
				match = false;
				continue;
			}
			match = std::fabs(module->params[SP4TSpeakersOut::CH_AZ_PARAM + c].getValue() - preset.channels[c].azimuth)
			            < 0.05f
			        && std::fabs(module->params[SP4TSpeakersOut::CH_ELEV_PARAM + c].getValue()
			                     - preset.channels[c].elevation)
			               < 0.05f;
		}
		if (match)
			return p;
	}
	return -1;
}

/** Sets a parameter, recording the change so that it can be undone. */
void setParam(SP4TSpeakersOut* module, int paramId, float value, history::ComplexAction* group) {
	float oldValue = module->params[paramId].getValue();
	if (oldValue == value)
		return;
	module->params[paramId].setValue(value);

	history::ParamChange* change = new history::ParamChange;
	change->moduleId = module->id;
	change->paramId = paramId;
	change->oldValue = oldValue;
	change->newValue = value;
	group->push(change);
}

/** Runs `edit` over the parameters as one undoable step. */
template <typename Edit> void editChannels(SP4TSpeakersOut* module, const std::string& name, const Edit& edit) {
	history::ComplexAction* group = new history::ComplexAction;
	group->name = name;
	edit(group);
	if (group->isEmpty())
		delete group;
	else
		APP->history->push(group);
}

void applyPreset(SP4TSpeakersOut* module, int index) {
	if (!module || index < 0 || index >= NUM_LAYOUT_PRESETS)
		return;
	const LayoutPreset& preset = LAYOUT_PRESETS[index];
	editChannels(module, string::f("load %s layout", preset.name), [&](history::ComplexAction* group) {
		for (int c = 0; c < NUM_CHANNELS; c++) {
			bool used = c < preset.numChannels;
			int state = !used ? CHANNEL_UNUSED : (preset.channels[c].lfe ? CHANNEL_LFE : CHANNEL_SPEAKER);
			setParam(module, SP4TSpeakersOut::CH_AZ_PARAM + c, used ? preset.channels[c].azimuth : 0.f, group);
			setParam(module, SP4TSpeakersOut::CH_ELEV_PARAM + c, used ? preset.channels[c].elevation : 0.f, group);
			setParam(module, SP4TSpeakersOut::CH_STATE_PARAM + c, (float) state, group);
		}
	});
}

void clearChannel(SP4TSpeakersOut* module, int channel) {
	if (!module)
		return;
	editChannels(module, "clear channel", [&](history::ComplexAction* group) {
		setParam(module, SP4TSpeakersOut::CH_STATE_PARAM + channel, (float) CHANNEL_UNUSED, group);
	});
}

/** Selects the screen font on `vg`, or returns false when it is not loaded. */
bool selectValueFont(NVGcontext* vg, float fontSize) {
	std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system(sp4t::LABEL_FONT));
	if (!font)
		return false;
	nvgFontFaceId(vg, font->handle);
	nvgFontSize(vg, fontSize);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	return true;
}

/** Where a centred string starts, the font being already selected. */
float centredLeft(NVGcontext* vg, const std::string& text, float centreX) {
	float bounds[4];
	nvgTextBounds(vg, 0, 0, text.c_str(), NULL, bounds);
	return centreX - (bounds[2] - bounds[0]) / 2.f;
}

/** Draws `text` centred in a cell, and returns the x it starts at. */
float drawCentered(NVGcontext* vg, const std::string& text, Vec centre, float fontSize, NVGcolor color) {
	if (!selectValueFont(vg, fontSize))
		return centre.x;
	float left = centredLeft(vg, text, centre.x);
	nvgFillColor(vg, color);
	nvgText(vg, left, centre.y, text.c_str(), NULL);
	return left;
}

} // namespace


struct SpeakerTable;


/** One editable angle of the channel table.
 *
 * The text is only a view of the parameter: it is refreshed from it while the
 * field is not being edited, and parsed back into it on Enter or when the
 * field loses the focus, which is also when an out-of-range value is clamped
 * and an unparsable one goes back to what it was.
 */
struct AngleField : LedDisplayTextField {
	SP4TSpeakersOut* module = nullptr;
	SpeakerTable* table = nullptr;
	int channel = 0;
	bool elevation = false;

	AngleField() {
		fontPath = asset::system(sp4t::LABEL_FONT);
		bgColor = nvgRGBA(0, 0, 0, 0);
		color = sp4t::SCREEN_TEXT;
		textOffset = Vec(0, 0);
	}

	int state() const { return channelState(module, channel); }
	bool editable() const { return module && !isLfe(state()); }
	bool focused() const { return APP->event->selectedWidget == this; }

	int angleParam() const {
		return (elevation ? SP4TSpeakersOut::CH_ELEV_PARAM : SP4TSpeakersOut::CH_AZ_PARAM) + channel;
	}
	int otherParam() const {
		return (elevation ? SP4TSpeakersOut::CH_AZ_PARAM : SP4TSpeakersOut::CH_ELEV_PARAM) + channel;
	}
	float limit() const { return elevation ? 90.f : 180.f; }

	/** Takes the text back from the parameter. */
	void refresh() {
		if (state() != CHANNEL_SPEAKER) {
			text = "";
			cursor = selection = 0;
			return;
		}
		std::string value = string::f("%.1f", channelAngle(module, channel, elevation));
		if (value == text)
			return;
		text = value;
		cursor = selection = (int) text.size();
	}

	void commit() {
		if (!module || !editable())
			return;
		std::string trimmed = string::trim(text);
		if (trimmed.empty()) {
			// An emptied field gives the channel up.
			if (state() != CHANNEL_UNUSED)
				clearChannel(module, channel);
			refresh();
			return;
		}

		float value = 0.f;
		try {
			size_t end = 0;
			value = std::stof(trimmed, &end);
			if (string::trim(trimmed.substr(end)) != "")
				throw std::invalid_argument("trailing characters");
		}
		catch (const std::exception&) {
			// Unparsable: back to the value the parameter still holds.
			refresh();
			return;
		}

		value = math::clamp(value, -limit(), limit());
		bool wasUnused = state() == CHANNEL_UNUSED;
		editChannels(module, "set speaker angle", [&](history::ComplexAction* group) {
			setParam(module, angleParam(), value, group);
			// A row is never half set: the other angle starts at zero.
			if (wasUnused) {
				setParam(module, otherParam(), 0.f, group);
				setParam(module, SP4TSpeakersOut::CH_STATE_PARAM + channel, (float) CHANNEL_SPEAKER, group);
			}
		});
		refresh();
	}

	void step() override {
		if (!focused())
			refresh();
		LedDisplayTextField::step();
	}

	void onButton(const ButtonEvent& e) override {
		// The angles of an LFE channel are not editable: the click is left to
		// the row, which answers a right one with its own menu.
		if (!editable())
			return;
		LedDisplayTextField::onButton(e);
	}

	void onAction(const ActionEvent& e) override { commit(); }

	void onDeselect(const DeselectEvent& e) override {
		commit();
		LedDisplayTextField::onDeselect(e);
	}

	void onSelectKey(const SelectKeyEvent& e) override;

	/** The text is drawn centred, so the hit test cannot use the left-aligned
	 * one of the base class. */
	int getTextPosition(Vec mousePos) override {
		NVGcontext* vg = APP->window->vg;
		if (text.empty() || !vg || !selectValueFont(vg, VALUE_FONT_SIZE))
			return 0;
		float left = centredLeft(vg, text, box.size.x / 2.f);
		std::vector<NVGglyphPosition> glyphs(text.size());
		int count = nvgTextGlyphPositions(vg, left, 0.f, text.c_str(), NULL, glyphs.data(), (int) glyphs.size());
		for (int i = 0; i < count; i++) {
			if (mousePos.x < (glyphs[i].minx + glyphs[i].maxx) / 2.f)
				return i;
		}
		return (int) text.size();
	}

	void drawLayer(const DrawArgs& args, int layer) override;
};


/** The LFE toggle of a row: a click, or Space once the cell has the focus. */
struct LfeCell : OpaqueWidget {
	SP4TSpeakersOut* module = nullptr;
	SpeakerTable* table = nullptr;
	int channel = 0;

	int state() const { return channelState(module, channel); }

	void toggle() {
		if (!module)
			return;
		int current = state();
		int next;
		if (current == CHANNEL_SPEAKER)
			next = CHANNEL_LFE_WITH_ANGLES;
		else if (current == CHANNEL_UNUSED)
			next = CHANNEL_LFE;
		else
			// Back to the angles the channel had, if it had any.
			next = current == CHANNEL_LFE_WITH_ANGLES ? CHANNEL_SPEAKER : CHANNEL_UNUSED;

		editChannels(module, "toggle LFE channel", [&](history::ComplexAction* group) {
			setParam(module, SP4TSpeakersOut::CH_STATE_PARAM + channel, (float) next, group);
		});
	}

	void onButton(const ButtonEvent& e) override;
	void onSelectKey(const SelectKeyEvent& e) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};


/** One row of the channel table: the channel number, its two angles and its
 * LFE toggle. A row is bound to a channel, which changes with the page. */
struct TableRow : OpaqueWidget {
	SP4TSpeakersOut* module = nullptr;
	SpeakerTable* table = nullptr;
	int channel = 0;
	AngleField* azField = nullptr;
	AngleField* elevField = nullptr;
	LfeCell* lfeCell = nullptr;

	void setChannel(int c) {
		channel = c;
		azField->channel = c;
		elevField->channel = c;
		lfeCell->channel = c;
		// The text follows the channel at once: a field that is deselected
		// after the page has turned would otherwise commit the text of the
		// channel it used to show into the one it now shows.
		azField->refresh();
		elevField->refresh();
	}

	void onButton(const ButtonEvent& e) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};


/** The channel table: four rows on the page the module is showing. */
struct SpeakerTable : widget::Widget {
	SP4TSpeakersOut* module = nullptr;
	TableRow* rows[ROWS_PER_PAGE] = {};

	SpeakerTable(SP4TSpeakersOut* module) : module(module) {
		box.pos = mm2px(Vec(TABLE_X, TABLE_Y));
		box.size = mm2px(Vec(TABLE_W, TABLE_H));

		for (int r = 0; r < ROWS_PER_PAGE; r++) {
			TableRow* row = new TableRow;
			row->module = module;
			row->table = this;
			row->box.pos = Vec(0, mm2px(r * ROW_H));
			row->box.size = Vec(box.size.x, mm2px(ROW_H));

			AngleField* az = new AngleField;
			az->module = module;
			az->table = this;
			az->box.size = mm2px(Vec(COLUMN_AZ.width, ROW_H));
			az->box.pos = Vec(mm2px(COLUMN_AZ.centre - COLUMN_AZ.width / 2.f - TABLE_X), 0);
			row->azField = az;
			row->addChild(az);

			AngleField* elev = new AngleField;
			elev->module = module;
			elev->table = this;
			elev->elevation = true;
			elev->box.size = mm2px(Vec(COLUMN_ELEV.width, ROW_H));
			elev->box.pos = Vec(mm2px(COLUMN_ELEV.centre - COLUMN_ELEV.width / 2.f - TABLE_X), 0);
			row->elevField = elev;
			row->addChild(elev);

			LfeCell* lfe = new LfeCell;
			lfe->module = module;
			lfe->table = this;
			lfe->box.size = mm2px(Vec(COLUMN_LFE.width, ROW_H));
			lfe->box.pos = Vec(mm2px(COLUMN_LFE.centre - COLUMN_LFE.width / 2.f - TABLE_X), 0);
			row->lfeCell = lfe;
			row->addChild(lfe);

			rows[r] = row;
			addChild(row);
		}
		updateRows();
	}

	int getPage() const { return module ? module->page : 0; }

	/** Binds the rows to the channels of the page on show. */
	void updateRows() {
		int page = getPage();
		for (int r = 0; r < ROWS_PER_PAGE; r++)
			rows[r]->setChannel(page * ROWS_PER_PAGE + r);
	}

	void setPage(int page) {
		if (!module)
			return;
		module->page = math::clamp(page, 0, NUM_PAGES - 1);
		updateRows();
	}

	void step() override {
		// The page can also be turned by the panel buttons or a patch load.
		if (rows[0]->channel != getPage() * ROWS_PER_PAGE)
			updateRows();
		Widget::step();
	}

	/** Moves the editing focus to the next (or previous) editable angle,
	 * skipping the LFE rows and turning the page when it runs off one. */
	void focusNeighbour(int channel, bool elevation, bool backwards) {
		if (!module)
			return;
		// Two editable fields per channel that is not an LFE one.
		int from = channel * 2 + (elevation ? 1 : 0);
		for (int step = 1; step <= NUM_CHANNELS * 2; step++) {
			int at = from + (backwards ? -step : step);
			at = (at % (NUM_CHANNELS * 2) + NUM_CHANNELS * 2) % (NUM_CHANNELS * 2);
			int targetChannel = at / 2;
			if (isLfe(module->getChannelState(targetChannel)))
				continue;
			setPage(targetChannel / ROWS_PER_PAGE);
			TableRow* row = rows[targetChannel % ROWS_PER_PAGE];
			AngleField* field = (at % 2) ? row->elevField : row->azField;
			APP->event->setSelectedWidget(field);
			field->selectAll();
			return;
		}
	}
};


void AngleField::onSelectKey(const SelectKeyEvent& e) {
	if (e.action == GLFW_PRESS && e.key == GLFW_KEY_TAB && table) {
		// Committed first, so that the row the focus leaves is up to date and
		// the next one is picked from the layout as it then stands.
		commit();
		table->focusNeighbour(channel, elevation, (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT);
		e.consume(this);
		return;
	}
	LedDisplayTextField::onSelectKey(e);
}


void AngleField::drawLayer(const DrawArgs& args, int layer) {
	if (layer != 1) {
		LedDisplayTextField::drawLayer(args, layer);
		return;
	}

	int rowState = state();
	Vec centre = box.size.div(2);

	// A row that carries no angle shows a placeholder instead, dimmer when the
	// channel is not used at all than for the "/" of an LFE one.
	if (!focused() && rowState != CHANNEL_SPEAKER) {
		bool lfe = isLfe(rowState);
		drawCentered(args.vg, lfe ? "/" : "---", centre, VALUE_FONT_SIZE, lfe ? sp4t::SCREEN_TEXT_DIM : UNUSED_COLOR);
		return;
	}

	float left = drawCentered(args.vg, text, centre, VALUE_FONT_SIZE, color);

	// Caret and selection, which the centred text has to be measured for.
	if (!focused())
		return;
	std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system(sp4t::LABEL_FONT));
	if (!font)
		return;
	nvgFontFaceId(args.vg, font->handle);
	nvgFontSize(args.vg, VALUE_FONT_SIZE);
	nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

	std::vector<NVGglyphPosition> glyphs(text.size() + 1);
	int count = nvgTextGlyphPositions(args.vg, left, centre.y, text.c_str(), NULL, glyphs.data(), (int) glyphs.size());
	auto xOf = [&](int index) {
		if (index <= 0)
			return left;
		if (index >= count)
			return count > 0 ? glyphs[count - 1].maxx : left;
		return glyphs[index].x;
	};

	int begin = std::min(cursor, selection);
	int end = std::max(cursor, selection);
	if (begin != end) {
		NVGcolor highlight = color;
		highlight.a = 0.3f;
		nvgBeginPath(args.vg);
		nvgRect(args.vg, xOf(begin), 2.f, xOf(end) - xOf(begin), box.size.y - 4.f);
		nvgFillColor(args.vg, highlight);
		nvgFill(args.vg);
	}
	nvgBeginPath(args.vg);
	nvgRect(args.vg, xOf(cursor), 3.f, 1.f, box.size.y - 6.f);
	nvgFillColor(args.vg, color);
	nvgFill(args.vg);
}


void LfeCell::onButton(const ButtonEvent& e) {
	if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
		APP->event->setSelectedWidget(this);
		toggle();
		e.consume(this);
		return;
	}
	OpaqueWidget::onButton(e);
}


void LfeCell::onSelectKey(const SelectKeyEvent& e) {
	if (e.action == GLFW_PRESS) {
		if (e.key == GLFW_KEY_SPACE || e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER) {
			toggle();
			e.consume(this);
			return;
		}
		if (e.key == GLFW_KEY_TAB && table) {
			// Leaves along the angles, as if the focus came from this row.
			table->focusNeighbour(
				channel, (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT, (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT);
			e.consume(this);
			return;
		}
	}
	OpaqueWidget::onSelectKey(e);
}


void LfeCell::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		int rowState = state();
		Vec centre = box.size.div(2);
		if (isLfe(rowState))
			drawCentered(args.vg, "LFE", centre, VALUE_FONT_SIZE, LFE_COLOR);
		else if (rowState == CHANNEL_SPEAKER)
			drawCentered(args.vg, "/", centre, VALUE_FONT_SIZE, sp4t::SCREEN_TEXT_DIM);
		else
			drawCentered(args.vg, "---", centre, VALUE_FONT_SIZE, UNUSED_COLOR);
	}
	OpaqueWidget::drawLayer(args, layer);
}


void TableRow::onButton(const ButtonEvent& e) {
	// The cells come first: a right click on an editable angle belongs to the
	// text field, which has a menu of its own.
	Widget::onButton(e);
	if (e.getTarget())
		return;

	if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT && module) {
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel(string::f("Channel %d", channel + 1)));
		int target = channel;
		SP4TSpeakersOut* m = module;
		menu->addChild(createMenuItem("Clear channel", "", [m, target]() { clearChannel(m, target); }));
		e.consume(this);
		return;
	}

	// Otherwise the row swallows the click, as an OpaqueWidget would.
	e.stopPropagating();
	if (e.button == GLFW_MOUSE_BUTTON_LEFT)
		e.consume(this);
}


void TableRow::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		int rowState = channelState(module, channel);
		NVGcolor color = rowState == CHANNEL_UNUSED ? UNUSED_COLOR : sp4t::SCREEN_TEXT_DIM;
		drawCentered(args.vg,
		             string::f("%d", channel + 1),
		             Vec(mm2px(COLUMN_CH.centre - TABLE_X), box.size.y / 2.f),
		             VALUE_FONT_SIZE,
		             color);
	}
	OpaqueWidget::drawLayer(args, layer);
}


/** Name of the layout on show, which opens the preset menu when clicked. */
struct LayoutChoice : LedDisplayChoice {
	SP4TSpeakersOut* module = nullptr;

	LayoutChoice() {
		fontPath = asset::system(sp4t::LABEL_FONT);
		bgColor = nvgRGBA(0, 0, 0, 0);
		color = sp4t::SCREEN_TEXT;
		textOffset = Vec(0, 0);
	}

	void step() override {
		int preset = matchingPreset(module);
		text = preset < 0 ? "Custom" : LAYOUT_PRESETS[preset].name;
		LedDisplayChoice::step();
	}

	void onAction(const ActionEvent& e) override {
		if (!module)
			return;
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel("Speaker layout"));
		SP4TSpeakersOut* m = module;
		int current = matchingPreset(m);
		for (int p = 0; p < NUM_LAYOUT_PRESETS; p++)
			menu->addChild(createCheckMenuItem(
				LAYOUT_PRESETS[p].name, "", [=]() { return p == current; }, [=]() { applyPreset(m, p); }));
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			drawCentered(args.vg, text, box.size.div(2), 11.f, color);
		OpaqueWidget::drawLayer(args, layer);
	}
};


/** The LFE note and the empty-layout message, over the 3D view. */
struct ScreenNote : TransparentWidget {
	SP4TSpeakersOut* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			TransparentWidget::drawLayer(args, layer);
			return;
		}

		std::string lfe;
		int speakers = 0;
		for (int c = 0; c < NUM_CHANNELS; c++) {
			int state = channelState(module, c);
			if (state == CHANNEL_SPEAKER)
				speakers++;
			else if (isLfe(state))
				lfe += (lfe.empty() ? "" : " ") + string::f("%d", c + 1);
		}

		if (speakers == 0)
			sp4t::drawText(args.vg,
			               sp4t::LABEL_FONT,
			               10.f,
			               sp4t::SCREEN_TEXT_DIM,
			               NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE,
			               box.size.div(2),
			               "NO SPEAKER CHANNEL");
		if (!lfe.empty())
			sp4t::drawText(args.vg,
			               sp4t::LABEL_FONT,
			               8.f,
			               LFE_COLOR,
			               NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM,
			               Vec(5.f, box.size.y - 4.f),
			               ("LFE: " + lfe).c_str());

		TransparentWidget::drawLayer(args, layer);
	}
};


struct SP4TSpeakersOutWidget : ModuleWidget {
	sp4t::AmbisonicDisplay* display;
	SpeakerTable* table;
	dsp::BooleanTrigger buttonTriggers[4];

	SP4TSpeakersOutWidget(SP4TSpeakersOut* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TSpeakersOut.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TSpeakersOut-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		display = createWidget<sp4t::AmbisonicDisplay>(mm2px(Vec(1.728, 11.5)));
		display->box.size = mm2px(Vec(45.6, 34.039));
		addChild(display);

		ScreenNote* note = new ScreenNote;
		note->module = module;
		note->box.pos = display->box.pos;
		note->box.size = display->box.size;
		addChild(note);

		LayoutChoice* layout = new LayoutChoice;
		layout->module = module;
		layout->box.pos = mm2px(Vec(10.728, 52.8));
		layout->box.size = mm2px(Vec(27.6, 6.0));
		addChild(layout);

		addParam(createParamCentered<SP4TButton>(mm2px(Vec(4.478, 55.8)), module, SP4TSpeakersOut::PREV_LAYOUT_PARAM));
		addParam(createParamCentered<SP4TButton>(mm2px(Vec(44.578, 55.8)), module, SP4TSpeakersOut::NEXT_LAYOUT_PARAM));

		table = new SpeakerTable(module);
		addChild(table);

		addInput(createInputCentered<SP4TAmbiPort>(mm2px(Vec(8.756, 114.0)), module, SP4TSpeakersOut::AMBI_INPUT));
		addParam(createParamCentered<SP4TButton>(mm2px(Vec(22.6, 114.0)), module, SP4TSpeakersOut::PREV_PAGE_PARAM));
		addParam(createParamCentered<SP4TButton>(mm2px(Vec(42.6, 114.0)), module, SP4TSpeakersOut::NEXT_PAGE_PARAM));

		static const float pageX[NUM_PAGES] = {27.5f, 30.9f, 34.3f, 37.7f};
		for (int p = 0; p < NUM_PAGES; p++) {
			SP4TSignalLight* light = createLightCentered<SP4TSignalLight>(
				mm2px(Vec(pageX[p], 114.0)), module, SP4TSpeakersOut::PAGE_LED_LIGHT + p);
			light->haloSpread = 1.5f;
			addChild(light);
		}

		// The channel outputs, two per row, numbered row by row.
		for (int c = 0; c < NUM_CHANNELS; c++) {
			float x = (c % 2) ? 64.6f : 54.35f;
			float ledX = (c % 2) ? 61.5f : 51.25f;
			float y = 19.4f + (c / 2) * 11.25f;
			addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(x, y)), module, SP4TSpeakersOut::CH_OUTPUT + c));
			addChild(createLightCentered<SP4TSignalLight>(
				mm2px(Vec(ledX, 13.55f + (c / 2) * 11.25f)), module, SP4TSpeakersOut::CH_LED_LIGHT + c));
		}

		addOutput(
			createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(59.475, 114.0)), module, SP4TSpeakersOut::POLY_OUTPUT));
	}

	/** The panel buttons are read here rather than in the audio thread: they
	 * turn the page and load layouts, which belongs to the UI (and, for a
	 * layout, to the undo history). */
	void step() override {
		SP4TSpeakersOut* module = getModule<SP4TSpeakersOut>();
		if (module) {
			if (buttonTriggers[0].process(module->params[SP4TSpeakersOut::PREV_LAYOUT_PARAM].getValue() > 0.f))
				stepPreset(module, -1);
			if (buttonTriggers[1].process(module->params[SP4TSpeakersOut::NEXT_LAYOUT_PARAM].getValue() > 0.f))
				stepPreset(module, 1);
			if (buttonTriggers[2].process(module->params[SP4TSpeakersOut::PREV_PAGE_PARAM].getValue() > 0.f))
				table->setPage((module->page + NUM_PAGES - 1) % NUM_PAGES);
			if (buttonTriggers[3].process(module->params[SP4TSpeakersOut::NEXT_PAGE_PARAM].getValue() > 0.f))
				table->setPage((module->page + 1) % NUM_PAGES);

			display->viewMode = module->viewMode;
		}

		// Also filled without a module, so that the browser shows the layout a
		// new module starts with rather than an empty screen.
		display->sources.clear();
		for (int c = 0; c < NUM_CHANNELS; c++) {
			if (channelState(module, c) != CHANNEL_SPEAKER)
				continue;
			sp4t::Direction direction = {channelAngle(module, c, false), channelAngle(module, c, true)};
			display->sources.push_back({direction, string::f("%d", c + 1), SPEAKER_COLOR});
		}
		ModuleWidget::step();
	}

	/** Steps through the presets, starting at either end from a custom layout. */
	void stepPreset(SP4TSpeakersOut* module, int delta) {
		int current = matchingPreset(module);
		int next;
		if (current < 0)
			next = delta > 0 ? 0 : NUM_LAYOUT_PRESETS - 1;
		else
			next = (current + delta + NUM_LAYOUT_PRESETS) % NUM_LAYOUT_PRESETS;
		applyPreset(module, next);
	}

	void appendContextMenu(Menu* menu) override {
		SP4TSpeakersOut* module = getModule<SP4TSpeakersOut>();

		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Speaker layout", "", [=](Menu* sub) {
			int current = matchingPreset(module);
			for (int p = 0; p < NUM_LAYOUT_PRESETS; p++)
				sub->addChild(createCheckMenuItem(
					LAYOUT_PRESETS[p].name, "", [=]() { return p == current; }, [=]() { applyPreset(module, p); }));
		}));

		menu->addChild(createIndexSubmenuItem(
			"Decoding method",
			{"AllRAD", "EPAD", "SAD"},
			[=]() { return (int) module->options.method; },
			[=](int index) {
				Decoder::Options options = module->options;
				options.method = (Decoder::Method) index;
				module->setOptions(options);
			}));

		// Single-band decoding uses the high band's matrix, so its weighting is
		// the one on offer until the crossover splits the two.
		if (!module->options.dualBand) {
			menu->addChild(createBoolMenuItem(
				"Max-rE weighting",
				"",
				[=]() { return module->options.highMaxRE; },
				[=](bool enabled) {
					Decoder::Options options = module->options;
					options.highMaxRE = enabled;
					module->setOptions(options);
				}));
		}

		menu->addChild(createBoolMenuItem(
			"Dual-band decoding",
			"",
			[=]() { return module->options.dualBand; },
			[=](bool enabled) {
				Decoder::Options options = module->options;
				options.dualBand = enabled;
				module->setOptions(options);
			}));

		if (module->options.dualBand) {
			std::vector<std::string> freqLabels;
			int currentFreq = 0;
			for (int i = 0; i < NUM_CROSSOVER_FREQS; i++) {
				freqLabels.push_back(string::f("%.0f Hz", CROSSOVER_FREQS[i]));
				if (CROSSOVER_FREQS[i] == module->options.crossover)
					currentFreq = i;
			}
			menu->addChild(createIndexSubmenuItem(
				"Crossover frequency",
				freqLabels,
				[=]() { return currentFreq; },
				[=](int index) {
					Decoder::Options options = module->options;
					options.crossover = CROSSOVER_FREQS[index];
					module->setOptions(options);
				}));

			menu->addChild(createBoolMenuItem(
				"Low band max-rE",
				"",
				[=]() { return module->options.lowMaxRE; },
				[=](bool enabled) {
					Decoder::Options options = module->options;
					options.lowMaxRE = enabled;
					module->setOptions(options);
				}));
			menu->addChild(createBoolMenuItem(
				"High band max-rE",
				"",
				[=]() { return module->options.highMaxRE; },
				[=](bool enabled) {
					Decoder::Options options = module->options;
					options.highMaxRE = enabled;
					module->setOptions(options);
				}));

			std::vector<std::string> orderLabels = {"1st", "2nd", "3rd"};
			menu->addChild(createIndexSubmenuItem(
				"Low band order",
				orderLabels,
				[=]() { return module->options.lowOrder - 1; },
				[=](int index) {
					Decoder::Options options = module->options;
					options.lowOrder = index + 1;
					module->setOptions(options);
				}));
			menu->addChild(createIndexSubmenuItem(
				"High band order",
				orderLabels,
				[=]() { return module->options.highOrder - 1; },
				[=](int index) {
					Decoder::Options options = module->options;
					options.highOrder = index + 1;
					module->setOptions(options);
				}));
		}

		menu->addChild(createIndexPtrSubmenuItem("LFE feed", {"Silent", "Omni (W) below 120 Hz"}, &module->lfeMode));
		menu->addChild(
			createIndexPtrSubmenuItem("3D view mode", sp4t::AmbisonicDisplay::getViewModeLabels(), &module->viewMode));
	}
};


Model* modelNun4takSP4TSpeakersOut = createModel<SP4TSpeakersOut, SP4TSpeakersOutWidget>("SP4T-Speakers-Out");
