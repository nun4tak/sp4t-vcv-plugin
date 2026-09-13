#pragma once
#include "plugin.hpp"

// Panel components shared by the SP4T modules.
//
// Flat and matte variants of VCV Rack's Component Library graphics
// (https://github.com/VCVRack/Rack/tree/v2/res/ComponentLibrary, CC BY-NC 4.0),
// keeping the original sizes.


/** Flat variant of RoundBlackKnob (9.6 mm), with a darker body on dark panels. */
struct SP4TRoundKnob : RoundKnob {
	std::shared_ptr<window::Svg> lightBgSvg;
	std::shared_ptr<window::Svg> darkBgSvg;
	bool dark = false;

	SP4TRoundKnob() {
		shadow->opacity = 0.f;
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TRoundKnob.svg")));
		lightBgSvg = Svg::load(asset::plugin(pluginInstance, "res/components/SP4TRoundKnob_bg.svg"));
		darkBgSvg = Svg::load(asset::plugin(pluginInstance, "res/components/SP4TRoundKnob_bg-dark.svg"));
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


/** Flat variant of CKSSThree (vertical 3-position switch, value 0 at the bottom). */
struct SP4TSwitchThree : app::SvgSwitch {
	SP4TSwitchThree() {
		shadow->opacity = 0.f;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TSwitchThree_0.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TSwitchThree_1.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TSwitchThree_2.svg")));
	}
};
