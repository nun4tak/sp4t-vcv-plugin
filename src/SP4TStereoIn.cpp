#include "plugin.hpp"
#include "components.hpp"
#include "AmbiEncoder.hpp"

#include <algorithm>
#include <cmath>


namespace {

const float DEG2RAD = float(M_PI) / 180.f;
const float RAD2DEG = 180.f / float(M_PI);

/** Unit vector in the ambiX frame: x front, y left, z up. */
struct SourceVector {
	float x, y, z;
};

/** Azimuth (anticlockwise from front) and elevation, in degrees. */
struct SourceDirection {
	float azimuth, elevation;
};

SourceVector directionToVector(float azimuth, float elevation) {
	float a = azimuth * DEG2RAD;
	float e = elevation * DEG2RAD;
	return {std::cos(e) * std::cos(a), std::cos(e) * std::sin(a), std::sin(e)};
}

/** Returns azimuth in [-180, 180] and elevation in [-90, 90]. */
SourceDirection vectorToDirection(const SourceVector& v) {
	return {std::atan2(v.y, v.x) * RAD2DEG, std::atan2(v.z, std::hypot(v.x, v.y)) * RAD2DEG};
}

/** Computes the directions of a stereo pair centered on (azimuth, elevation).
 *
 * Each source lies at `spread` degrees (great-circle distance) from the center,
 * along the horizontal tangent pointing toward increasing azimuth for the left
 * source.
 */
void stereoPairDirections(float azimuth, float elevation, float spread,
                          SourceDirection* left, SourceDirection* right) {
	SourceVector d = directionToVector(azimuth, elevation);
	float a = azimuth * DEG2RAD;
	SourceVector t = {-std::sin(a), std::cos(a), 0.f};
	float c = std::cos(spread * DEG2RAD);
	float s = std::sin(spread * DEG2RAD);
	*left = vectorToDirection(SourceVector{c * d.x + s * t.x, c * d.y + s * t.y, c * d.z + s * t.z});
	*right = vectorToDirection(SourceVector{c * d.x - s * t.x, c * d.y - s * t.y, c * d.z - s * t.z});
}

// Screen colors (Ayu dark)
const NVGcolor SCREEN_GRID = nvgRGBA(0x6c, 0x73, 0x80, 0x58);
const NVGcolor SCREEN_GRID_HIDDEN = nvgRGBA(0x6c, 0x73, 0x80, 0x28);
const NVGcolor SCREEN_OUTLINE = nvgRGBA(0x6c, 0x73, 0x80, 0xb0);
const NVGcolor SCREEN_TEXT = nvgRGB(0xd9, 0xd7, 0xce);
const NVGcolor SCREEN_TEXT_DIM = nvgRGB(0x8a, 0x91, 0x99);
const NVGcolor SOURCE_MONO = nvgRGB(0xff, 0xb4, 0x54);
const NVGcolor SOURCE_INACTIVE = nvgRGB(0x56, 0x5b, 0x66);
const NVGcolor SOURCE_LEFT = nvgRGB(0x59, 0xc2, 0xff);
const NVGcolor SOURCE_RIGHT = nvgRGB(0xf0, 0x71, 0x78);

} // namespace


struct SP4TStereoIn : Module {
	enum ParamId {
		AZIMUTH_PARAM,
		ELEVATION_PARAM,
		SPREAD_PARAM,
		ORDER_PARAM,
		AZIMUTH_CV_PARAM,
		ELEVATION_CV_PARAM,
		SPREAD_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AZIMUTH_INPUT,
		ELEVATION_INPUT,
		SPREAD_INPUT,
		LEFT_INPUT,
		RIGHT_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AMBI_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
	enum Projection {
		PROJECTION_ORTHOGRAPHIC_PAIR,
		PROJECTION_HAMMER,
		PROJECTION_CARTESIAN,
		PROJECTION_SPHERE_3D,
		PROJECTIONS_LEN
	};

	enum { MAX_NUM_SOURCES = 2 };

	sp4t::AmbiEncoder encoder;
	dsp::ClockDivider controlDivider;

	// Source directions shown on the screen, updated by the engine thread.
	SourceDirection centerDirection = {0.f, 0.f};
	SourceDirection leftDirection = {0.f, 0.f};
	SourceDirection rightDirection = {0.f, 0.f};
	bool stereo = false;

	int projection = PROJECTION_ORTHOGRAPHIC_PAIR;
	/** When enabled, positive azimuth values (knob or CV) move the source to the
	 * right (clockwise), opposite to the Ambisonic convention. */
	bool flipAzimuth = false;

	SP4TStereoIn() : encoder(MAX_NUM_SOURCES) {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(AZIMUTH_PARAM, -180.f, 180.f, 0.f, "Azimuth", "°");
		configParam(ELEVATION_PARAM, -90.f, 90.f, 0.f, "Elevation", "°");
		configParam(SPREAD_PARAM, 0.f, 90.f, 45.f, "Spread", "°");
		configParam(AZIMUTH_CV_PARAM, -1.f, 1.f, 0.f, "Azimuth CV attenuverter", "%", 0.f, 100.f);
		configParam(ELEVATION_CV_PARAM, -1.f, 1.f, 0.f, "Elevation CV attenuverter", "%", 0.f, 100.f);
		configParam(SPREAD_CV_PARAM, -1.f, 1.f, 0.f, "Spread CV attenuverter", "%", 0.f, 100.f);
		configSwitch(ORDER_PARAM, 0.f, 2.f, 0.f, "Ambisonic order", {"1st (FOA)", "2nd (HOA-2)", "3rd (HOA-3)"});

		configInput(AZIMUTH_INPUT, "Azimuth CV");
		configInput(ELEVATION_INPUT, "Elevation CV");
		configInput(SPREAD_INPUT, "Spread CV");
		configInput(LEFT_INPUT, "Left (mono)");
		configInput(RIGHT_INPUT, "Right");
		configOutput(AMBI_OUTPUT, "Ambisonic (ACN/SN3D)");

		controlDivider.setDivision(16);
		updateControls();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		projection = PROJECTION_ORTHOGRAPHIC_PAIR;
		flipAzimuth = false;
		encoder.reset();
		updateControls();
	}

	/** Knob value plus the CV input scaled by its attenuverter, where
	 * `fullScaleVoltage` (5 V bipolar, 10 V unipolar) spans `range` degrees. */
	float getAngle(int paramId, int cvParamId, int inputId, float range, float fullScaleVoltage) {
		float cv = inputs[inputId].getVoltage() / fullScaleVoltage;
		return params[paramId].getValue() + params[cvParamId].getValue() * cv * range;
	}

	void updateControls() {
		encoder.setOrder((int) params[ORDER_PARAM].getValue() + 1);

		// Beyond +/-180 degrees, the azimuth wraps around. The elevation doesn't
		// go past the poles (the rear is reached with the azimuth).
		float azimuth = getAngle(AZIMUTH_PARAM, AZIMUTH_CV_PARAM, AZIMUTH_INPUT, 180.f, 5.f);
		if (flipAzimuth)
			azimuth = -azimuth;
		float elevation = math::clamp(getAngle(ELEVATION_PARAM, ELEVATION_CV_PARAM, ELEVATION_INPUT, 90.f, 5.f), -90.f, 90.f);
		float spread = math::clamp(getAngle(SPREAD_PARAM, SPREAD_CV_PARAM, SPREAD_INPUT, 90.f, 10.f), 0.f, 90.f);

		// Stereo as soon as the right input is connected.
		bool isStereo = inputs[RIGHT_INPUT].isConnected();
		encoder.setNumSources(isStereo ? 2 : 1);

		SourceDirection center = vectorToDirection(directionToVector(azimuth, elevation));
		SourceDirection left = center;
		SourceDirection right = center;
		if (isStereo) {
			stereoPairDirections(azimuth, elevation, spread, &left, &right);
			encoder.setSourceDirection(0, left.azimuth, left.elevation);
			encoder.setSourceDirection(1, right.azimuth, right.elevation);
		}
		else {
			encoder.setSourceDirection(0, center.azimuth, center.elevation);
		}

		centerDirection = center;
		leftDirection = left;
		rightDirection = right;
		stereo = isStereo;
	}

	void process(const ProcessArgs& args) override {
		if (controlDivider.process())
			updateControls();

		float in[MAX_NUM_SOURCES] = {
			inputs[LEFT_INPUT].getVoltageSum(),
			inputs[RIGHT_INPUT].getVoltageSum(),
		};
		float out[sp4t::AmbiEncoder::MAX_NUM_SH];
		encoder.process(in, out);

		int channels = encoder.getNumSH();
		outputs[AMBI_OUTPUT].setChannels(channels);
		for (int c = 0; c < channels; c++)
			outputs[AMBI_OUTPUT].setVoltage(out[c], c);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "projection", json_integer(projection));
		json_object_set_new(rootJ, "flipAzimuth", json_boolean(flipAzimuth));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* projectionJ = json_object_get(rootJ, "projection");
		if (projectionJ)
			projection = math::clamp((int) json_integer_value(projectionJ), 0, PROJECTIONS_LEN - 1);

		json_t* flipAzimuthJ = json_object_get(rootJ, "flipAzimuth");
		if (flipAzimuthJ)
			flipAzimuth = json_boolean_value(flipAzimuthJ);
	}
};


/** Screen showing the source position(s) on the sphere. */
struct SP4TStereoInDisplay : TransparentWidget {
	SP4TStereoIn* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			nvgSave(args.vg);
			nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
			int projection = module ? module->projection : SP4TStereoIn::PROJECTION_ORTHOGRAPHIC_PAIR;
			switch (projection) {
				case SP4TStereoIn::PROJECTION_HAMMER: drawHammer(args.vg); break;
				case SP4TStereoIn::PROJECTION_CARTESIAN: drawCartesian(args.vg); break;
				case SP4TStereoIn::PROJECTION_SPHERE_3D: drawSphere3D(args.vg); break;
				default: drawOrthographicPair(args.vg); break;
			}
			nvgRestore(args.vg);
		}
		TransparentWidget::drawLayer(args, layer);
	}

	/** Front (+x) and rear (-x) hemispheres side by side, both as seen by the
	 * listener: the front disk has the left side on the left and the rear disk
	 * continues from the right edge of the front disk. */
	void drawOrthographicPair(NVGcontext* vg) {
		const float pad = 3.f;
		const float labelHeight = 11.f;
		float r = std::min((box.size.x - 3 * pad) / 4, (box.size.y - labelHeight - 2 * pad) / 2);
		float cy = (box.size.y + labelHeight) / 2;
		Vec front(box.size.x / 2 - pad / 2 - r, cy);
		Vec rear(box.size.x / 2 + pad / 2 + r, cy);

		drawHemisphereGrid(vg, front, r);
		drawHemisphereGrid(vg, rear, r);
		drawLabel(vg, Vec(front.x, cy - r - 2), "FRONT");
		drawLabel(vg, Vec(rear.x, cy - r - 2), "REAR");

		// Sources on the front/rear boundary (azimuth or elevation at +/-90
		// degrees, up to rounding errors) are shown on the same disk as the mono
		// source, or on the front disk if the mono source is on the boundary too.
		const float boundary = 1e-4f;
		SourceDirection center = module ? module->centerDirection : SourceDirection{0.f, 0.f};
		bool boundaryOnRear = directionToVector(center.azimuth, center.elevation).x < -boundary;

		drawSources(vg, [&](const SourceDirection& d) -> Vec {
			SourceVector v = directionToVector(d.azimuth, d.elevation);
			bool onFront = v.x > boundary || (v.x >= -boundary && !boundaryOnRear);
			if (onFront)
				return Vec(front.x - v.y * r, front.y - v.z * r);
			return Vec(rear.x + v.y * r, rear.y - v.z * r);
		});
	}

	void drawHemisphereGrid(NVGcontext* vg, Vec c, float r) {
		nvgBeginPath(vg);
		// Parallels every 45 degrees
		for (int elevation = -45; elevation <= 45; elevation += 45) {
			float y = c.y - std::sin(elevation * DEG2RAD) * r;
			float halfWidth = std::cos(elevation * DEG2RAD) * r;
			nvgMoveTo(vg, c.x - halfWidth, y);
			nvgLineTo(vg, c.x + halfWidth, y);
		}
		// Meridians every 45 degrees
		nvgMoveTo(vg, c.x, c.y - r);
		nvgLineTo(vg, c.x, c.y + r);
		for (int azimuth = 45; azimuth < 90; azimuth += 45)
			nvgEllipse(vg, c.x, c.y, std::sin(azimuth * DEG2RAD) * r, r);
		nvgStrokeColor(vg, SCREEN_GRID);
		nvgStrokeWidth(vg, 0.5f);
		nvgStroke(vg);

		nvgBeginPath(vg);
		nvgCircle(vg, c.x, c.y, r);
		nvgStrokeColor(vg, SCREEN_OUTLINE);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
	}

	/** Hammer equal-area projection of the whole sphere, azimuth 0 at the
	 * center and increasing toward the left. */
	void drawHammer(NVGcontext* vg) {
		const float pad = 3.f;
		float a = std::min((box.size.x - 2 * pad) / 2, box.size.y - 2 * pad);
		float b = a / 2;
		Vec c = box.size.div(2);

		auto project = [=](float azimuth, float elevation) -> Vec {
			float halfLambda = azimuth * DEG2RAD / 2;
			float phi = elevation * DEG2RAD;
			float denom = std::sqrt(1 + std::cos(phi) * std::cos(halfLambda));
			return Vec(c.x - a * std::cos(phi) * std::sin(halfLambda) / denom,
			           c.y - b * std::sin(phi) / denom);
		};

		nvgBeginPath(vg);
		// Meridians and parallels every 45 degrees
		for (int azimuth = -135; azimuth <= 135; azimuth += 45) {
			for (int k = 0; k <= 18; k++) {
				Vec p = project(azimuth, -90 + 10 * k);
				if (k == 0)
					nvgMoveTo(vg, p.x, p.y);
				else
					nvgLineTo(vg, p.x, p.y);
			}
		}
		for (int elevation = -45; elevation <= 45; elevation += 45) {
			for (int k = 0; k <= 36; k++) {
				Vec p = project(-180 + 10 * k, elevation);
				if (k == 0)
					nvgMoveTo(vg, p.x, p.y);
				else
					nvgLineTo(vg, p.x, p.y);
			}
		}
		nvgStrokeColor(vg, SCREEN_GRID);
		nvgStrokeWidth(vg, 0.5f);
		nvgStroke(vg);

		nvgBeginPath(vg);
		nvgEllipse(vg, c.x, c.y, a, b);
		nvgStrokeColor(vg, SCREEN_OUTLINE);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);

		drawSources(vg, [&](const SourceDirection& d) {
			return project(d.azimuth, d.elevation);
		});
	}

	/** Equirectangular (cartesian) projection over the whole screen, with the
	 * origin at the center: azimuth on the horizontal (Y) axis, increasing
	 * toward the left and +/-180 degrees at the edges, and elevation on the
	 * vertical (Z) axis, +/-90 degrees at the top/bottom edges. */
	void drawCartesian(NVGcontext* vg) {
		Vec size = box.size;
		Vec c = size.div(2);
		auto project = [=](float azimuth, float elevation) {
			return Vec(c.x - azimuth / 360 * size.x, c.y - elevation / 180 * size.y);
		};

		// Graticules every 45 degrees (the edges are the screen borders)
		nvgBeginPath(vg);
		for (int azimuth = -135; azimuth <= 135; azimuth += 45) {
			float x = project(azimuth, 0).x;
			nvgMoveTo(vg, x, 0);
			nvgLineTo(vg, x, size.y);
		}
		for (int elevation = -45; elevation <= 45; elevation += 90) {
			float y = project(0, elevation).y;
			nvgMoveTo(vg, 0, y);
			nvgLineTo(vg, size.x, y);
		}
		nvgStrokeColor(vg, SCREEN_GRID);
		nvgStrokeWidth(vg, 0.5f);
		nvgStroke(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, c.y);
		nvgLineTo(vg, size.x, c.y);
		nvgMoveTo(vg, c.x, 0);
		nvgLineTo(vg, c.x, size.y);
		nvgStrokeColor(vg, SCREEN_OUTLINE);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);

		// Labels sit next to their graticule, on the side of the origin. Azimuth
		// labels alternate between below and above the horizontal axis so they
		// don't crowd each other; elevation labels are right of the vertical
		// axis (the origin is labeled once, as an azimuth).
		const float offset = 1.5f;
		for (int azimuth = -180; azimuth <= 180; azimuth += 45) {
			float x = project(azimuth, 0).x;
			bool below = azimuth % 90 != 0;
			float y = below ? c.y + offset : c.y - offset;
			int vAlign = below ? NVG_ALIGN_TOP : NVG_ALIGN_BOTTOM;
			if (azimuth > 0)
				drawGridLabel(vg, Vec(x + offset, y), string::f("%d", azimuth), NVG_ALIGN_LEFT | vAlign);
			else
				drawGridLabel(vg, Vec(x - offset, y), string::f("%d", azimuth), NVG_ALIGN_RIGHT | vAlign);
		}
		for (int elevation = -90; elevation <= 90; elevation += 45) {
			float y = project(0, elevation).y;
			if (elevation > 0)
				drawGridLabel(vg, Vec(c.x + offset, y + offset), string::f("%d", elevation), NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			else if (elevation < 0)
				drawGridLabel(vg, Vec(c.x + offset, y - offset), string::f("%d", elevation), NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
		}

		// Keep the letters of sources at the edges (e.g., rear or poles) whole.
		const Vec margin(4, 5);
		drawSources(vg, [&](const SourceDirection& d) {
			return project(d.azimuth, d.elevation).clamp(math::Rect(margin, size.minus(margin.mult(2))));
		});
	}

	/** 3D view of the sphere and its X, Y, Z axes, seen from the front-left and
	 * above, with X (front) pointing toward the viewer at the lower left, Y
	 * (left) to the right and Z up. Graticules and sources on the hidden side
	 * of the sphere are faded.
	 *
	 * The projection is orthographic. */
	void drawSphere3D(NVGcontext* vg) {
		// Camera direction, from the center toward the eye.
		const float camAzimuth = 30 * DEG2RAD;
		const float camElevation = 30 * DEG2RAD;
		const SourceVector eye = directionToVector(camAzimuth * RAD2DEG, camElevation * RAD2DEG);
		const SourceVector right = {-std::sin(camAzimuth), std::cos(camAzimuth), 0.f};
		const SourceVector up = {-std::sin(camElevation) * std::cos(camAzimuth),
		                         -std::sin(camElevation) * std::sin(camAzimuth), std::cos(camElevation)};
		auto dot = [](const SourceVector& a, const SourceVector& b) {
			return a.x * b.x + a.y * b.y + a.z * b.z;
		};

		Vec c = box.size.div(2);
		// Radius of the sphere outline on the screen (not drawn, used for the
		// layout).
		float outlineRadius = c.y - 11;

		// Axis names, just outside the outline in the screen direction of the
		// positive axis ends.
		const SourceVector axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
		const char* axisNames[3] = {"X", "Y", "Z"};
		const float labelDistance = outlineRadius + 5;
		const float labelHalfHeight = 3.5f;
		auto axisDirection = [=](const SourceVector& axis) {
			return Vec(dot(axis, right), -dot(axis, up)).normalize();
		};

		// Centers the plot vertically on the screen, including the axis names
		// (the Z name sticks out above the sphere).
		float top = -outlineRadius;
		float bottom = outlineRadius;
		for (const SourceVector& axis : axes) {
			float y = axisDirection(axis).y * labelDistance;
			top = std::min(top, y - labelHalfHeight);
			bottom = std::max(bottom, y + labelHalfHeight);
		}
		c.y -= (top + bottom) / 2;

		auto project = [=](const SourceVector& v) {
			return Vec(c.x + outlineRadius * dot(v, right), c.y - outlineRadius * dot(v, up));
		};
		auto isVisible = [=](const SourceVector& v) {
			return dot(v, eye) > 0;
		};

		// Graticules: parallels and meridians every 45 degrees, including the
		// great circles in the XY, XZ and YZ planes.
		using Curve = std::function<SourceVector(float)>;
		auto parallel = [](float elevation) -> Curve {
			float e = elevation * DEG2RAD;
			return [=](float t) {
				return SourceVector{std::cos(e) * std::cos(t), std::cos(e) * std::sin(t), std::sin(e)};
			};
		};
		auto meridian = [](float azimuth) -> Curve {
			float a = azimuth * DEG2RAD;
			return [=](float t) {
				return SourceVector{std::cos(t) * std::cos(a), std::cos(t) * std::sin(a), std::sin(t)};
			};
		};
		const std::vector<Curve> graticules = {
			parallel(-45), parallel(0), parallel(45),
			meridian(0), meridian(45), meridian(90), meridian(135),
		};

		// Strokes the curves as polylines, keeping only the segments on the
		// hidden or the visible side of the sphere.
		const int numSegments = 72;
		auto strokeCurves = [&](const std::vector<Curve>& curves, bool visible, NVGcolor color, float width) {
			nvgBeginPath(vg);
			for (const Curve& curve : curves) {
				SourceVector prev = curve(0);
				bool drawing = false;
				for (int k = 1; k <= numSegments; k++) {
					SourceVector v = curve(2 * float(M_PI) * k / numSegments);
					SourceVector mid = {prev.x + v.x, prev.y + v.y, prev.z + v.z};
					float norm = std::sqrt(dot(mid, mid));
					SourceVector midUnit = {mid.x / norm, mid.y / norm, mid.z / norm};
					if (isVisible(midUnit) == visible) {
						if (!drawing) {
							Vec p = project(prev);
							nvgMoveTo(vg, p.x, p.y);
						}
						Vec p = project(v);
						nvgLineTo(vg, p.x, p.y);
					}
					drawing = isVisible(midUnit) == visible;
					prev = v;
				}
			}
			nvgStrokeColor(vg, color);
			nvgStrokeWidth(vg, width);
			nvgStroke(vg);
		};
		strokeCurves(graticules, false, SCREEN_GRID_HIDDEN, 0.5f);
		strokeCurves(graticules, true, SCREEN_GRID, 0.5f);

		// Axes as diameters of the sphere, with an arrow at the positive end
		// whose tip touches the sphere surface.
		const float arrowLength = 4.f;
		const float arrowHalfWidth = 1.8f;
		for (const SourceVector& axis : axes) {
			Vec tip = project(axis);
			Vec u = tip.minus(c).normalize();
			Vec n(-u.y, u.x);
			Vec base = tip.minus(u.mult(arrowLength));

			nvgBeginPath(vg);
			Vec p0 = project(SourceVector{-axis.x, -axis.y, -axis.z});
			nvgMoveTo(vg, p0.x, p0.y);
			nvgLineTo(vg, base.x, base.y);
			nvgStrokeColor(vg, SCREEN_OUTLINE);
			nvgStrokeWidth(vg, 0.8f);
			nvgStroke(vg);

			nvgBeginPath(vg);
			nvgMoveTo(vg, tip.x, tip.y);
			nvgLineTo(vg, base.x + n.x * arrowHalfWidth, base.y + n.y * arrowHalfWidth);
			nvgLineTo(vg, base.x - n.x * arrowHalfWidth, base.y - n.y * arrowHalfWidth);
			nvgClosePath(vg);
			nvgFillColor(vg, SCREEN_OUTLINE);
			nvgFill(vg);
		}

		for (int i = 0; i < 3; i++)
			drawAxisLabel(vg, c.plus(axisDirection(axes[i]).mult(labelDistance)), axisNames[i]);

		drawSources(vg, [&](const SourceDirection& d) {
			SourceVector v = directionToVector(d.azimuth, d.elevation);
			return ProjectedSource(project(v), isVisible(v) ? 1.f : 0.5f);
		});
	}

	/** Projected position of a source; `alpha` below 1 fades hidden sources. */
	struct ProjectedSource {
		Vec pos;
		float alpha;
		ProjectedSource(Vec pos, float alpha = 1.f) : pos(pos), alpha(alpha) {}
	};

	template <typename Project>
	void drawSources(NVGcontext* vg, const Project& project) {
		// The module browser preview shows a centered mono source.
		SourceDirection center = module ? module->centerDirection : SourceDirection{0.f, 0.f};
		if (module && module->stereo) {
			drawSourceLetter(vg, project(center), "M", SOURCE_INACTIVE);
			drawSourceLetter(vg, project(module->leftDirection), "L", SOURCE_LEFT);
			drawSourceLetter(vg, project(module->rightDirection), "R", SOURCE_RIGHT);
		}
		else {
			drawSourceLetter(vg, project(center), "M", SOURCE_MONO);
		}
	}

	/** Draws a source as a letter centered on its projected position. */
	void drawSourceLetter(NVGcontext* vg, ProjectedSource p, const char* letter, NVGcolor color) {
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
		if (!font)
			return;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, 11.f);
		nvgFillColor(vg, nvgTransRGBAf(color, p.alpha));
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, p.pos.x, p.pos.y, letter, NULL);
	}

	void drawLabel(NVGcontext* vg, Vec p, const char* text) {
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font)
			return;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, 10.f);
		nvgFillColor(vg, SCREEN_TEXT);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgText(vg, p.x, p.y, text, NULL);
	}

	void drawAxisLabel(NVGcontext* vg, Vec p, const char* text) {
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font)
			return;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, 10.f);
		nvgFillColor(vg, SCREEN_TEXT);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, p.x, p.y, text, NULL);
	}

	/** Small, dim label for graticules. */
	void drawGridLabel(NVGcontext* vg, Vec p, const std::string& text, int align) {
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font)
			return;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, 7.f);
		nvgFillColor(vg, SCREEN_TEXT_DIM);
		nvgTextAlign(vg, align);
		nvgText(vg, p.x, p.y, text.c_str(), NULL);
	}
};


struct SP4TStereoInWidget : ModuleWidget {
	SP4TStereoInWidget(SP4TStereoIn* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SP4TStereoIn.svg"),
		                     asset::plugin(pluginInstance, "res/SP4TStereoIn-dark.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		SP4TStereoInDisplay* display = createWidget<SP4TStereoInDisplay>(mm2px(Vec(2.5, 11.5)));
		display->box.size = mm2px(Vec(45.8, 27.3));
		display->module = module;
		addChild(display);

		// One row per angle (CV input, CV attenuverter, knob), spanning the panel
		// width with 4 mm margins.
		const float rowY[] = {54.427f, 73.167f, 91.907f};
		const int knobIds[] = {SP4TStereoIn::AZIMUTH_PARAM, SP4TStereoIn::ELEVATION_PARAM, SP4TStereoIn::SPREAD_PARAM};
		const int cvParamIds[] = {SP4TStereoIn::AZIMUTH_CV_PARAM, SP4TStereoIn::ELEVATION_CV_PARAM, SP4TStereoIn::SPREAD_CV_PARAM};
		const int inputIds[] = {SP4TStereoIn::AZIMUTH_INPUT, SP4TStereoIn::ELEVATION_INPUT, SP4TStereoIn::SPREAD_INPUT};
		for (int i = 0; i < 3; i++) {
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(8.013, rowY[i])), module, inputIds[i]));
			addParam(createParamCentered<SP4TRoundSmallKnob>(mm2px(Vec(22.846, rowY[i])), module, cvParamIds[i]));
			addParam(createParamCentered<SP4TRoundLargeKnob>(mm2px(Vec(40.233, rowY[i])), module, knobIds[i]));
		}

		// Bottom row (4 mm margins): audio inputs, order switch, Ambisonic output
		addParam(createParamCentered<SP4TSwitchThree>(mm2px(Vec(26.925, 114.0)), module, SP4TStereoIn::ORDER_PARAM));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(8.013, 114.0)), module, SP4TStereoIn::LEFT_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(18.336, 114.0)), module, SP4TStereoIn::RIGHT_INPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(41.3, 114.0)), module, SP4TStereoIn::AMBI_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		SP4TStereoIn* module = getModule<SP4TStereoIn>();

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("3D view mode",
			{"Orthographic (front / rear)", "Hammer", "Cartesian", "3D sphere"},
			&module->projection));
		menu->addChild(createBoolPtrMenuItem("Flip azimuth (positive = right)", "", &module->flipAzimuth));
	}
};


Model* modelNun4takSP4TStereoIn = createModel<SP4TStereoIn, SP4TStereoInWidget>("SP4T-Stereo-In");
