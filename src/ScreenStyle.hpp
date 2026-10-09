#pragma once

#include "plugin.hpp"

#include <string>


namespace sp4t {


// Screen colors (Ayu dark), shared by the module screens.
const NVGcolor SCREEN_GRID = nvgRGBA(0x6c, 0x73, 0x80, 0x58);
const NVGcolor SCREEN_GRID_HIDDEN = nvgRGBA(0x6c, 0x73, 0x80, 0x28);
const NVGcolor SCREEN_OUTLINE = nvgRGBA(0x6c, 0x73, 0x80, 0xb0);
const NVGcolor SCREEN_TEXT = nvgRGB(0xd9, 0xd7, 0xce);
const NVGcolor SCREEN_TEXT_DIM = nvgRGB(0x8a, 0x91, 0x99);

const char* const LABEL_FONT = "res/fonts/ShareTechMono-Regular.ttf";
const char* const SOURCE_FONT = "res/fonts/Nunito-Bold.ttf";


inline void drawText(NVGcontext* vg, const char* fontPath, float fontSize, NVGcolor color, int align, Vec p,
                     const char* text) {
	std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system(fontPath));
	if (!font)
		return;
	nvgFontFaceId(vg, font->handle);
	nvgFontSize(vg, fontSize);
	nvgFillColor(vg, color);
	nvgTextAlign(vg, align);
	nvgText(vg, p.x, p.y, text, NULL);
}


/** Label above a plot (e.g., FRONT, REAR) or naming an axis. */
inline void drawLabel(NVGcontext* vg, Vec p, const char* text, int align, float fontSize = 10.f) {
	drawText(vg, LABEL_FONT, fontSize, SCREEN_TEXT, align, p, text);
}


/** Small, dim label for graticules. */
inline void drawGridLabel(NVGcontext* vg, Vec p, const std::string& text, int align) {
	drawText(vg, LABEL_FONT, 7.f, SCREEN_TEXT_DIM, align, p, text.c_str());
}


} // namespace sp4t
