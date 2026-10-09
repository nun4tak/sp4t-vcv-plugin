#pragma once
#include "plugin.hpp"

// Panel components shared by the SP4T modules.
//
// Flat and matte variants of VCV Rack's Component Library graphics
// (https://github.com/VCVRack/Rack/tree/v2/res/ComponentLibrary, CC BY-NC 4.0),
// keeping the original sizes.


/** Flat round knob, with a darker body on dark panels. */
struct SP4TKnob : RoundKnob {
	std::shared_ptr<window::Svg> lightBgSvg;
	std::shared_ptr<window::Svg> darkBgSvg;
	bool dark = false;

	SP4TKnob() { shadow->opacity = 0.f; }

	/** Loads res/components/<name>.svg (rotating part) and <name>_bg.svg /
	 * <name>_bg-dark.svg (body, light and dark panels). */
	void loadSvgs(const std::string& name) {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/" + name + ".svg")));
		lightBgSvg = Svg::load(asset::plugin(pluginInstance, "res/components/" + name + "_bg.svg"));
		darkBgSvg = Svg::load(asset::plugin(pluginInstance, "res/components/" + name + "_bg-dark.svg"));
		dark = settings::preferDarkPanels;
		bg->setSvg(dark ? darkBgSvg : lightBgSvg);
	}

	void step() override {
		if (dark != settings::preferDarkPanels) {
			dark = settings::preferDarkPanels;
			bg->setSvg(dark ? darkBgSvg : lightBgSvg);
			fb->setDirty();
		}
		RoundKnob::step();
	}
};


/** Flat variant of RoundBlackKnob (9.6 mm). */
struct SP4TRoundKnob : SP4TKnob {
	SP4TRoundKnob() { loadSvgs("SP4TRoundKnob"); }
};


/** Flat variant of RoundBlackKnob scaled at 117% (11.2 mm). */
struct SP4TRoundLargeKnob : SP4TKnob {
	SP4TRoundLargeKnob() { loadSvgs("SP4TRoundLargeKnob"); }
};


/** Flat, slightly smaller variant of RoundSmallBlackKnob (6.8 mm), e.g., for CV attenuverters. */
struct SP4TRoundSmallKnob : SP4TKnob {
	SP4TRoundSmallKnob() { loadSvgs("SP4TRoundSmallKnob"); }
};


/** Flat variant of TL1105 (momentary push button), with the SP4TRoundSmallKnob
 * metrics (6.8 mm). */
struct SP4TButton : app::SvgSwitch {
	SP4TButton() {
		momentary = true;
		shadow->opacity = 0.f;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TButton_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TButton_1.svg")));
	}
};


/** Flat variant of VCVLatch (latching push button), with the SP4TButton metrics.
 *
 * Unlike VCVLatch, which only depresses the button while the mouse is held
 * (SvgSwitch::latch), the frame here follows the param value: the cap stays lit
 * as long as the button is latched on. The off frame is the released
 * SP4TButton, so only the lit frame is a file of its own.
 */
struct SP4TLatch : app::SvgSwitch {
	/** How far the glow of the lit cap spreads beyond it, in px (0: no glow). */
	float haloSpread = 3.f;
	/** Scales up Rack's halo brightness setting, as for SP4TSignalLight. */
	float haloGain = 3.f;
	/** The lit cap of SP4TLatch_1.svg (#E6E1CF), brightened: the glow also
	 * covers the cap, which it would otherwise darken. */
	NVGcolor haloColor = nvgRGB(0xff, 0xfb, 0xee);

	SP4TLatch() {
		momentary = false;
		shadow->opacity = 0.f;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TButton_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TLatch_1.svg")));
	}

	/** Glow around the cap while the button is on. */
	void drawLayer(const DrawArgs& args, int layer) override {
		app::SvgSwitch::drawLayer(args, layer);

		// Not in a framebuffer, e.g., screenshots or the module browser.
		if (layer != 1 || args.fb || haloSpread <= 0.f)
			return;
		float halo = settings::haloBrightness;
		if (halo == 0.f)
			return;
		ParamQuantity* pq = getParamQuantity();
		if (!pq || pq->getValue() < 0.5f)
			return;

		// The lit cap is 6.4/20 of the size of the button.
		Vec center = box.size.div(2);
		float core = box.size.x * 0.32f;
		float outer = core + haloSpread;
		// Only the opacity follows the halo setting: scaling the color down
		// instead would darken what the glow is drawn over.
		NVGcolor inner = haloColor;
		inner.a = std::min(halo * haloGain, 1.f);
		NVGpaint paint = nvgRadialGradient(args.vg, center.x, center.y, core, outer, inner, nvgRGBA(0, 0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, center.x - outer, center.y - outer, 2 * outer, 2 * outer);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);
	}
};


/** SP4TLatch that is momentary instead when the MOM/LATCH switch param of its
 * module, `modeParamId`, is in its MOM (0) position. */
struct SP4TModeButton : SP4TLatch {
	int modeParamId = -1;

	void step() override {
		if (module && modeParamId >= 0)
			momentary = module->params[modeParamId].getValue() < 0.5f;
		SP4TLatch::step();
	}
};


/** Flat variant of CKSSThree (vertical 3-position switch, value 0 at the top).
 *
 * The frame files are named after the handle position of CKSSThree, i.e.,
 * _0 is the bottom position and _2 the top position.
 */
struct SP4TSwitchThree : app::SvgSwitch {
	SP4TSwitchThree() {
		shadow->opacity = 0.f;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TSwitchThree_2.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TSwitchThree_1.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TSwitchThree_0.svg")));
	}
};


/** Flat variant of CKSS (vertical 2-position switch, value 0 at the top).
 *
 * Same design than SP4TSwitchThree, with a shorter body. The frame files are
 * named after the handle position of CKSS, i.e., _0 is the bottom position and
 * _1 the top position.
 */
struct SP4TSwitchTwo : app::SvgSwitch {
	SP4TSwitchTwo() {
		shadow->opacity = 0.f;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TSwitchTwo_1.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TSwitchTwo_0.svg")));
	}
};


/** Jack carrying an ambisonic signal (polyphonic cable).
 *
 * VCV Rack's PJ301M port with its outer ring in the Ayu blue used for the "4"
 * of the panel titles (#22A4E6 on light panels, #59C2FF on dark ones), so that
 * ambisonic jacks are told apart from the CV and audio ones at a glance. Only
 * that ring is recoloured, so the component keeps the stock port metrics
 * (8.0264 mm) and is a drop-in replacement for ThemedPJ301MPort.
 */
struct SP4TAmbiPort : app::ThemedSvgPort {
	SP4TAmbiPort() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TAmbiPort.svg")),
		       Svg::load(asset::plugin(pluginInstance, "res/components/SP4TAmbiPort-dark.svg")));
	}
};


/** LED showing an audio-rate signal, emitting the Ayu blue of the ambisonic
 * jacks and of the panel titles.
 *
 * Sized after the LED dots of the panels (1.134 mm), black when off, and sunk
 * in a bevelled rim. Compared to the Component Library lights, the rim and the
 * glow are drawn here because their metrics do not follow the size of the
 * light, which is too small for a glow derived from it to be seen.
 */
struct SP4TSignalLight : app::ModuleLightWidget {
	/** Width of the rim around the LED, in px. */
	float rimWidth = 0.9f;
	/** How far the glow spreads beyond the rim, in px (0: no glow). It is at
	 * full brightness up to the rim, then fades out. */
	float haloSpread = mm2px(2.3f);
	/** Scales up Rack's halo brightness setting, as the glow of such a small
	 * light is otherwise barely visible. */
	float haloGain = 3.f;

	SP4TSignalLight() {
		bgColor = nvgRGB(0x00, 0x00, 0x00);
		addBaseColor(nvgRGB(0x59, 0xc2, 0xff));
		box.size = mm2px(Vec(1.134, 1.134));
	}

	float getRadius() const { return std::min(box.size.x, box.size.y) / 2.f; }

	/** The LED and its rim, lit by `drawLight()` and `drawHalo()` on top. */
	void drawBackground(const DrawArgs& args) override {
		Vec center = box.size.div(2);
		float radius = getRadius();

		// Darker at the top, as if the LED was sunk into the panel.
		NVGpaint rim = nvgLinearGradient(args.vg,
		                                 center.x,
		                                 center.y - radius - rimWidth,
		                                 center.x,
		                                 center.y + radius + rimWidth,
		                                 nvgRGB(0x1c, 0x1f, 0x24),
		                                 nvgRGB(0x6a, 0x72, 0x80));
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, center.x, center.y, radius + rimWidth);
		nvgFillPaint(args.vg, rim);
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, center.x, center.y, radius);
		nvgFillColor(args.vg, bgColor);
		nvgFill(args.vg);
	}

	/** LightWidget::drawHalo(), with the metrics and the brightness above
	 * instead of values derived from the size of the light. */
	void drawHalo(const DrawArgs& args) override {
		// Not in a framebuffer, e.g., screenshots or the module browser.
		if (args.fb || haloSpread <= 0.f)
			return;
		float halo = settings::haloBrightness;
		if (halo == 0.f)
			return;

		Vec center = box.size.div(2);
		float core = getRadius() + rimWidth;
		float outer = core + haloSpread;
		// The lit color at full brightness, only the opacity following the
		// brightness and the halo setting: scaling the color down instead would
		// darken the LED, which the glow is drawn over.
		NVGcolor inner = baseColors.empty() ? color : baseColors[0];
		inner.a = std::min(color.a * halo * haloGain, 1.f);
		NVGpaint paint = nvgRadialGradient(args.vg, center.x, center.y, core, outer, inner, nvgRGBA(0, 0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, center.x - outer, center.y - outer, 2 * outer, 2 * outer);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);
	}
};
