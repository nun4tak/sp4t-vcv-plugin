#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelNun4takSP4TStereoIn);
	p->addModel(modelNun4takSP4TStereoOut);
	p->addModel(modelNun4takSP4TEarsOut);
	p->addModel(modelNun4takSP4TRotate);
	p->addModel(modelNun4takSP4TSplit);
	p->addModel(modelNun4takSP4TMerge);
	p->addModel(modelNun4takSP4TGain);
	p->addModel(modelNun4takSP4TMute);
	p->addModel(modelNun4takSP4TFlip);
	p->addModel(modelNun4takSP4TSmear);
	p->addModel(modelNun4takSP4TSource4In);
	p->addModel(modelNun4takSP4TSplitInsert);
	p->addModel(modelNun4takSP4TBlurSharp);
}
