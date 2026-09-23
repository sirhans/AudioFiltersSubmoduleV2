//
//  BMLadderFilter.h
//  AudioFilters
//
//  Moog ladder lowpass: four one-pole trapezoidal lowpasses in series with
//  tanh-saturated feedback around them, 24 dB per octave.
//
//      u  = x - k tanh(y4)
//      y1 = lp(u), y2 = lp(y1), y3 = lp(y2), y4 = lp(y3)     (all at fc)
//
//  For small signals H(s) = 1 / ((1 + s/wc)^4 + k): DC gain 1/(1 + k), a
//  resonant peak of 1/(4 - k) at fc, and self-oscillation at the cutoff
//  from k = 4, which the tanh limits (the amplitude grows with the square
//  root of the excess feedback). The feedback loop is solved without a
//  unit delay: y4 = G^4 (x - k tanh(y4)) + S is a scalar equation (S is
//  the states' contribution), solved by Newton from the previous output.
//  f' = 1 + G^4 k (1 - tanh^2) >= 1, so the step is always well
//  conditioned; at a 4 x oversampled rate G^4 k is small and two or three
//  steps reach float precision. Each stage is the same one-pole as
//  BMMultiLevelSVF_setLowpass6dB, with g = tan(pi fc / fs) prewarped.
//
//  The saturation means the response depends on level: a loud input drives
//  tanh into compression, which lowers the effective feedback (the Moog
//  bass loss and resonance drop with input level). Run it at an oversampled
//  rate: the aliased 3rd harmonic of a self-oscillation folds into the
//  band from about fs / 6 cutoff upward.
//
//  Parameter updates follow BMMultiLevelSVF: setters write pending values
//  under the lock, the process functions pick them up at the start of the
//  next buffer, immediately or (with filter sweep on) as a linear ramp
//  across that buffer. For per-sample modulation there is
//  BMLadderFilter_coefs and BMLadderFilter_tick, which the buffer functions
//  are built on.
//
//  Created by hans anderson on 13/9/26.
//  Anyone may use this file without restrictions.
//

#ifndef BMLadderFilter_h
#define BMLadderFilter_h

#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "../CrossPlatform/BMLock.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The feedback at which the small-signal filter self-oscillates. */
#define BM_LADDER_SELF_OSCILLATION_FEEDBACK 4.0

/*!
 *BMLadderFilterCoefs
 *
 * @abstract What one sample of processing needs: the one-pole coefficient
 * G = g / (1 + g) with g = tan(pi fc / fs), its fourth power, and the
 * feedback amount k.
 */
typedef struct BMLadderFilterCoefs {
	float G, G4, k;
} BMLadderFilterCoefs;

/*!
 *BMLadderFilterState
 *
 * @abstract One channel's state: the four stages' integrator states and the
 * last output, which is where the next sample's feedback solve starts.
 */
typedef struct BMLadderFilterState {
	float s1, s2, s3, s4;
	float y;
} BMLadderFilterState;

typedef struct BMLadderFilter {
	BMLadderFilterCoefs coefs;     // in use
	BMLadderFilterCoefs target;    // reached at the end of this buffer (filter sweep) or at its start
	BMLadderFilterCoefs pending;   // written by the setters, under the lock
	double fc_pending, k_pending;  // the settings behind `pending`, for the transfer function
	BMLadderFilterState state[2];
	float sampleRate;
	size_t numChannels;
	bool shouldUpdateParam, updateImmediately, needsClearStateVariables, filterSweep;
	BMLock lock;
} BMLadderFilter;



/*!
 *BMLadderFilter_init
 *
 * @param This       pointer to a BMLadderFilter struct
 * @param sampleRate the rate the filter runs at (the oversampled rate, ideally)
 * @param isStereo   set true for stereo processing
 */
void BMLadderFilter_init(BMLadderFilter *This, float sampleRate, bool isStereo);

/*!
 *BMLadderFilter_setLowpass24dB
 *
 * @abstract Set the cutoff and the feedback amount. Picked up at the start
 * of the next buffer (see BMLadderFilter_enableFilterSweep).
 *
 * @param fc        cutoff frequency in Hz; clamped to 0.46 fs
 * @param feedback  k in [0, ...): 0 is four cascaded one-poles, 4 is the
 *                  self-oscillation threshold (BM_LADDER_SELF_OSCILLATION_FEEDBACK),
 *                  above it the tanh sets the oscillation amplitude
 */
void BMLadderFilter_setLowpass24dB(BMLadderFilter *This, double fc, double feedback);

/*!
 *BMLadderFilter_processBufferMono
 *
 * @abstract In-place is fine.
 */
void BMLadderFilter_processBufferMono(BMLadderFilter *This, const float *input, float *output, size_t numSamples);

/*!
 *BMLadderFilter_processBufferStereo
 *
 * @abstract The two channels share coefficients and have their own state.
 */
void BMLadderFilter_processBufferStereo(BMLadderFilter *This,
										const float *inputL, const float *inputR,
										float *outputL, float *outputR,
										size_t numSamples);

/*!
 *BMLadderFilter_enableFilterSweep
 *
 * @abstract With sweep on, a parameter update ramps the coefficients
 * linearly across the buffer in which it is picked up, reaching the new
 * values on the first sample of the following buffer, as in
 * BMMultiLevelSVF_enableFilterSweep. Off (the default), updates are
 * applied at the start of the buffer; the filter stays stable and click-free
 * either way, the ramp only smooths the response change.
 */
void BMLadderFilter_enableFilterSweep(BMLadderFilter *This, bool sweepOn);

/*!
 *BMLadderFilter_forceImmediateUpdate
 *
 * @abstract The queued update is applied at the start of the next buffer
 * even with filter sweep on.
 */
void BMLadderFilter_forceImmediateUpdate(BMLadderFilter *This);

/*!
 *BMLadderFilter_clearBuffers
 *
 * @abstract Sets a flag that clears the state before the next buffer.
 */
void BMLadderFilter_clearBuffers(BMLadderFilter *This);

/*!
 *BMLadderFilter_tfMag
 *
 * @abstract Small-signal (linear) magnitude response of a ladder with the
 * given settings, |1 / ((1 + jW)^4 + k)| with W = tan(pi f / fs) / g the
 * prewarped bilinear frequency. Exact for the linearised filter: a signal
 * small enough that tanh(y) = y. A feedback of 4 or more has a pole on
 * (or beyond) the axis, and the value at fc is infinite (or meaningless);
 * clamp the feedback below 4 for a plot.
 *
 * @param fc         cutoff frequency in Hz
 * @param feedback   k
 * @param sampleRate the rate the filter runs at
 * @param frequency  the frequency to evaluate at, in Hz
 */
double BMLadderFilter_tfMag(double fc, double feedback, double sampleRate, double frequency);

/*!
 *BMLadderFilter_tfMagVector
 *
 * @abstract BMLadderFilter_tfMag for the most recently set (pending)
 * settings of this filter, at each frequency in frequency[], written to
 * magnitude[].
 */
void BMLadderFilter_tfMagVector(BMLadderFilter *This, const float *frequency, float *magnitude, size_t length);



/**************************************
   Per-sample building blocks
 **************************************/

/*!
 *BMLadderFilter_coefs
 *
 * @abstract Coefficients from the prewarped cutoff g = tan(pi fc / fs) and
 * the feedback amount, for callers that compute coefficients per sample
 * (audio-rate modulation). Negative feedback is clamped to zero.
 */
static inline BMLadderFilterCoefs BMLadderFilter_coefs(float g, float feedback){
	BMLadderFilterCoefs c;
	c.G = g / (1.0f + g);
	c.G4 = c.G * c.G;
	c.G4 *= c.G4;
	c.k = feedback < 0.0f ? 0.0f : feedback;
	return c;
}

/*!
 *BMLadderFilter_tick
 *
 * @abstract One sample of one channel: solve the feedback loop, run the four
 * stages, return the output.
 */
static inline float BMLadderFilter_tick(BMLadderFilterState *s, const BMLadderFilterCoefs *c, float x){
	const float G = c->G, G4 = c->G4, k = c->k;
	
	// what the states alone would put out this sample:
	// each stage is lp = G in + (1 - G) s, so through four stages
	// y4 = G^4 u + (1 - G) (((s1 G + s2) G + s3) G + s4)
	const float S = (1.0f - G) * (((s->s1 * G + s->s2) * G + s->s3) * G + s->s4);
	
	// solve y = G4 (x - k tanh(y)) + S by Newton from the last output; f' >= 1
	float y = s->y;
	float t = tanhf(y);
	for(int it = 0; ; it++){
		const float f = y - G4 * (x - k * t) - S;
		const float dy = f / (1.0f + G4 * k * (1.0f - t * t));
		y -= dy;
		t = tanhf(y);
		if(it == 3 || fabsf(dy) < 1e-6f) break;
	}
	
	// run the stages with the solved feedback so their states advance
	float v = x - k * t, lp;
	v = G * (v - s->s1); lp = v + s->s1; s->s1 = lp + v;
	v = G * (lp - s->s2); lp = v + s->s2; s->s2 = lp + v;
	v = G * (lp - s->s3); lp = v + s->s3; s->s3 = lp + v;
	v = G * (lp - s->s4); lp = v + s->s4; s->s4 = lp + v;
	s->y = lp;
	return lp;
}

#ifdef __cplusplus
}
#endif

#endif /* BMLadderFilter_h */
