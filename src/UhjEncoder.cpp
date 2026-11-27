#include "UhjEncoder.hpp"

#include <algorithm>

#include "hiir/PolyphaseIir2Designer.h"
#include "saf.h"


namespace sp4t {


const float UhjEncoder::MAKEUP_GAIN = 2.f;
const float UhjEncoder::BAND_EDGE = 18.f;


UhjEncoder::UhjEncoder(float sampleRate) {
	setSampleRate(sampleRate);
}


void UhjEncoder::reset() {
	chainS.clear_buffers();
	chainY.clear_buffers();
	chainT.clear_buffers();
}


void UhjEncoder::setSampleRate(float newSampleRate) {
	newSampleRate = std::max(newSampleRate, 4.f * BAND_EDGE);
	if (newSampleRate == sampleRate)
		return;
	sampleRate = newSampleRate;

	// The half-band design is symmetrical around a quarter of the sample rate:
	// a normalized transition bandwidth of BAND_EDGE / sampleRate puts the two
	// chains 90 degrees apart from BAND_EDGE up to Nyquist minus BAND_EDGE.
	double coefs[NUM_COEFS];
	hiir::PolyphaseIir2Designer::compute_coefs_spec_order_tbw(coefs, NUM_COEFS, BAND_EDGE / sampleRate);
	chainS.set_coefs(coefs);
	chainY.set_coefs(coefs);
	chainT.set_coefs(coefs);

	reset();
}


void UhjEncoder::process(const float* in, float* out) {
	// ambiX (ACN/SN3D) to the FuMa B-format the UHJ equations expect. At first
	// order this only reorders the channels and scales W by 1/sqrt(2). Both
	// convert in place and allocate nothing; the copy keeps them away from the
	// higher-order channels of the caller's buffer.
	float b[NUM_SH] = {in[0], in[1], in[2], in[3]};
	convertHOAChannelConvention(b, 1, 1, HOA_CH_ORDER_ACN, HOA_CH_ORDER_FUMA);
	convertHOANormConvention(b, 1, 1, HOA_NORM_SN3D, HOA_NORM_FUMA);

	// FuMa channel order: W, X, Y, Z. Z is discarded, UHJ carrying the
	// horizontal sound field only.
	const float w = b[0];
	const float x = b[1];
	const float y = b[2];

	const float s = 0.9396926f * w + 0.1855740f * x;
	const float t = -0.3420201f * w + 0.5098604f * x;

	// Of the two outputs of a chain, the first leads the second by 90 degrees,
	// so S and Y take the second (the A chain) and T the first (B).
	float ahead = 0.f, late = 0.f;
	chainS.process_sample(ahead, late, s);
	const float sumTerm = late;
	chainY.process_sample(ahead, late, 0.6554516f * y);
	const float diffTerm = late;
	chainT.process_sample(ahead, late, t);
	const float shiftedTerm = ahead;

	const float d = diffTerm + shiftedTerm;
	out[0] = 0.5f * MAKEUP_GAIN * (sumTerm + d);
	out[1] = 0.5f * MAKEUP_GAIN * (sumTerm - d);
}


} // namespace sp4t
