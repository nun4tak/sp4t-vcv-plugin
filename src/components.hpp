#pragma once
#include "plugin.hpp"

// Panel components shared by the SP4T modules.
//
// Flat and matte variants of VCV Rack's Component Library graphics
// (https://github.com/VCVRack/Rack/tree/v2/res/ComponentLibrary, CC BY-NC 4.0),
// keeping the original sizes.


/** Flat variant of RoundBlackKnob (10 mm). */
struct SP4TRoundKnob : RoundKnob {
	SP4TRoundKnob() {
		shadow->opacity = 0.f;
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TRoundKnob.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/SP4TRoundKnob_bg.svg")));
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
