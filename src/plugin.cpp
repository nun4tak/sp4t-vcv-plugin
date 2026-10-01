#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelNun4takSP4TStereoIn);
	p->addModel(modelNun4takSP4TStereoOut);
	p->addModel(modelNun4takSP4TGain);
}
