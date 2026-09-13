#include "plugin.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

extern "C" {
}

using namespace rack;


// ============================================================================
//  Module
// ============================================================================
struct SP4TStereoIn : Module {
    SP4TStereoIn() {
    }

    void process(const ProcessArgs& args) override {
    }
};


// ============================================================================
//  Panel / widget — minimal placeholder layout. Adjust positions to your SVG.
// ============================================================================
struct SP4TStereoInWidget : ModuleWidget {
    SP4TStereoInWidget(SP4TStereoIn* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance,
                                           "res/SP4TStereoIn.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(
            Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(
            Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(
            Vec(box.size.x - 2 * RACK_GRID_WIDTH,
                RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    }
};

Model* modelNun4takSP4TStereoIn =
    createModel<SP4TStereoIn, SP4TStereoInWidget>("SP4TStereoIn");
