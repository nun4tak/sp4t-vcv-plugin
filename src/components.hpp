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

	SP4TKnob() {
		shadow->opacity = 0.f;
	}

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
	SP4TRoundKnob() {
		loadSvgs("SP4TRoundKnob");
	}
};


/** Flat variant of RoundBlackKnob scaled at 117% (11.2 mm). */
struct SP4TRoundLargeKnob : SP4TKnob {
	SP4TRoundLargeKnob() {
		loadSvgs("SP4TRoundLargeKnob");
	}
};


/** Flat, slightly smaller variant of RoundSmallBlackKnob (6.8 mm), e.g., for CV attenuverters. */
struct SP4TRoundSmallKnob : SP4TKnob {
	SP4TRoundSmallKnob() {
		loadSvgs("SP4TRoundSmallKnob");
	}
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
	SP4TLatch() {
		momentary = false;
		shadow->opacity = 0.f;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TButton_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TLatch_1.svg")));
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
