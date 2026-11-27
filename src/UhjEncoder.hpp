#pragma once

#include "hiir/PhaseHalfPiFpu.h"


namespace sp4t {


/** Two-channel UHJ stereo encoder (Gerzon, 1985).
 *
 * Input signals follow the ambiX convention (ACN channel ordering, SN3D
 * normalization); only the first-order horizontal components W, X and Y are
 * used, height and all higher-order components being discarded.
 *
 *     S = 0.9396926 * W + 0.1855740 * X
 *     D = j(-0.3420201 * W + 0.5098604 * X) + 0.6554516 * Y
 *     L = 0.5 * (S + D)
 *     R = 0.5 * (S - D)
 *
 * with W, X, Y in FuMa B-format and j a wide-band +90 degree phase shift.
 *
 * No filter shifts a single signal by 90 degrees at every frequency, so the
 * shift is realized as a pair of allpass chains whose outputs are 90 degrees
 * apart over the audio band, and every term goes through one of them:
 *
 *     L = 0.5 * (A(S) + 0.6554516 * A(Y) + B(T))
 *     R = 0.5 * (A(S) - 0.6554516 * A(Y) - B(T))
 *
 * where T is the shifted term and B leads A by 90 degrees. The chains add no
 * fixed latency, unlike an FIR Hilbert transformer; their group delay is
 * frequency-dependent, negligible above a few hundred Hz and largest near the
 * low edge of the band. Both outputs go through chains of the same kind, so L
 * and R stay aligned and the mono sum is unaffected.
 *
 * No memory is allocated after construction.
 */
struct UhjEncoder {
	enum {
		/** Input channels read, i.e., first order (W, X, Y, Z with Z unused). */
		NUM_SH = 4,
		NUM_CHANNELS = 2,
		/** Coefficients of the phase-shift network. Half of them go to each of
		 * the two allpass chains, so this has to be even. 16 keeps the pair
		 * within 0.04 degree of 90 at 44.1, 48 and 96 kHz, 0.11 at 192 kHz. */
		NUM_COEFS = 16,
	};

	/** Uniform gain applied to both outputs.
	 *
	 * The standard coefficients are used as-is, which keeps UHJ's known stereo
	 * image and mono sum, but they put a front source at about 0.45 times the
	 * input W level on each channel, 5.6 dB below what SP4T Stereo Out decodes
	 * the same source to. Two brings the two modules within half a dB of each
	 * other; being uniform it changes nothing about the phase relationship
	 * between L and R, i.e., the output stays a UHJ signal.
	 */
	static const float MAKEUP_GAIN;

	/** Lowest frequency at which the two allpass chains are 90 degrees apart,
	 * in Hz. The band they are designed for is symmetrical, so it also ends
	 * that far below Nyquist. */
	static const float BAND_EDGE;

	explicit UhjEncoder(float sampleRate = 48000.f);

	/** Clears the allpass chains. */
	void reset();

	/** Redesigns the phase-shift network for a new sample rate and clears the
	 * chains. Designing for the sample rate in use keeps the 90 degree
	 * difference down to BAND_EDGE whatever the rate, where coefficients
	 * computed once for 48 kHz would move the accurate band up with it. */
	void setSampleRate(float sampleRate);
	float getSampleRate() const { return sampleRate; }

	/** Encodes one sample.
	 *
	 * @param in  NUM_SH input samples (ambiX)
	 * @param out the left and right output samples
	 */
	void process(const float* in, float* out);

private:
	using Chain = hiir::PhaseHalfPiFpu<NUM_COEFS>;

	float sampleRate = 0.f;

	/** The two terms taking the A (reference) chain, and the shifted one
	 * taking B. Fed one sample each per process() call, which is what keeps
	 * the three chains in step. */
	Chain chainS;
	Chain chainY;
	Chain chainT;
};


} // namespace sp4t
