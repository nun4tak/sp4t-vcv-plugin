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
