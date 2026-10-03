#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelNun4takSP4TStereoIn;
extern Model* modelNun4takSP4TStereoOut;
extern Model* modelNun4takSP4TEarsOut;
extern Model* modelNun4takSP4TRotate;
extern Model* modelNun4takSP4TSplit;
extern Model* modelNun4takSP4TMerge;
extern Model* modelNun4takSP4TGain;
extern Model* modelNun4takSP4TMute;
