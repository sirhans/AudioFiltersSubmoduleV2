//
//  BMMultiLevelSVF.h
//  BMAudioFilters
//
//  Created by Nguyen Minh Tien on 1/9/19
//
//Source: https://cytomic.com/files/dsp/SvfLinearTrapezoidalSin.pdf

#ifndef BMMultiLevelSVF_h
#define BMMultiLevelSVF_h

#include <stdio.h>
#include "../AudioFilter.h"
#include "../CrossPlatform/BMLock.h"
//#import <pthread/pthread.h>
#include "BMMultiLevelBiquad.h"

/*!
 *BMSVFGainMix
 *
 * @abstract One level's output mix split for per-sample gain modulation:
 * m = base + G * gain, m and base and gain being (high, band, low) weights
 * and G the level's linear gain parameter. Set by every setter; see
 * BMMultiLevelSVF_processBufferMonoGain for what G means per filter type.
 */
typedef struct BMSVFGainMix {
	float base[3];
	float gain[3];
} BMSVFGainMix;

typedef struct BMMultiLevelSVF{
    float *g0;
    float *g1;
	float *g2;
	float *m0;
	float *m1;
	float *m2;
	float *k;
	// targets are the values that we are sweeping to in the current buffer
	float *g0_target;
	float *g1_target;
	float *g2_target;
	float *m0_target;
	float *m1_target;
	float *m2_target;
	float *k_target;
	// pending are the values we will be sweeping to in the next buffer
	float *g0_pending;
	float *g1_pending;
	float *g2_pending;
	float *m0_pending;
	float *m1_pending;
	float *m2_pending;
	float *k_pending;
	// interpolation arrays are used for sample-by-sample filter coefficient
	// update during filter sweeps
	float *g0_interp;
	float *g1_interp;
	float *g2_interp;
	float *m0_interp;
	float *m1_interp;
	float *m2_interp;
	float *k_interp;
	// these are the state variables
    float *ic1eq;
    float *ic2eq;
    // per-level split of the output mix for per-sample gain modulation
    // (BMMultiLevelSVF_processBufferMonoGain): the mix is
    // m = base + G gain, with G the level's linear gain parameter
    BMSVFGainMix *gainMix;
    BMSVFGainMix *gainMix_pending;
    // double-precision state for BMMultiLevelSVF_processBufferStereoD
    // (same [level] / [numLevels + level] layout as ic1eq / ic2eq)
    double *ic1eqD;
    double *ic2eqD;
	
    size_t numLevels;
    // BMMultiLevelSVF_setNumActiveLevels: the process functions run levels
    // 0 ..< numActiveLevels only. Equal to numLevels unless set.
    size_t numActiveLevels;
    size_t numChannels;
	size_t oversampleFactor;
    double sampleRate;
    bool shouldUpdateParam, updateImmediately, needsClearStateVariables;
	bool filterSweep;
	//os_unfair_lock lock;
	BMLock lock;
//	BMMultiLevelBiquad biquadHelper; // we have this so that we can reuse some functions such as the ones for plotting transfer functions
}BMMultiLevelSVF;




/*!
 *BMMultiLevelSVF_init
 *
 * This function does simple oversampling by processing each sample of input
 * several times while writing the output only once. This type of
 * oversampling does nothing to improve the audio quality but it is helpful for
 * preventing the warping of the filter frequency response curves that occurs
 * near the Nyquist frequency as a result of the bilinear transform.
 *
 * @param This pointer to a BMMultilevelSVF struct
 * @param numLevels the number of second order filters required
 * @param sampleRate audio sample rate
 * @param isStereo set true for stereo processing
 */
void BMMultiLevelSVF_init(BMMultiLevelSVF *This,
						  size_t numLevels,
						  float sampleRate,
						  bool isStereo);



/*!
 *BMMultiLevelSVF_free
 */
void BMMultiLevelSVF_free(BMMultiLevelSVF *This);


/*!
 *BMMultiLevelSVF_processBufferMono
 */
void BMMultiLevelSVF_processBufferMono(BMMultiLevelSVF *This, const float* input, float* output, size_t numSamples);

/*!
 *BMMultiLevelSVF_processBufferStereo
 */
void BMMultiLevelSVF_processBufferStereo(BMMultiLevelSVF *This, const float* inputL,const float* inputR, float* outputL, float* outputR, size_t numSamples);

/*!
 *BMMultiLevelSVF_processBufferMonoGain
 *
 * @abstract processBufferMono with a per-sample linear gain on any level.
 * Static coefficients only (filter sweep must be off); parameter updates
 * are picked up at the start of the buffer like everywhere else. Mono only:
 * per-sample modulation is a per-voice job in this app, and the stereo
 * filters are static.
 *
 * What the gain means depends on the level's filter type. For the
 * fixed-pole first-order shelves (BMMultiLevelSVF_setHighShelfFirstOrderFixedPole
 * and the low shelf) it is the shelf gain itself, 10^(dB/20), replacing the
 * gain given to the setter sample by sample; the pole stays at fc, only the
 * output mix moves, so any modulation rate is safe and there is no
 * transient beyond the mix change. For every other filter type it is an
 * output gain on that level's output (the setter's static equivalent is 1).
 * Levels with a NULL gain array run with their static coefficients.
 *
 * @param gain  numLevels pointers; gain[level] is either NULL or a
 *              numSamples-long array of linear gain values for that level.
 *              gain itself may be NULL, which is plain static processing.
 */
void BMMultiLevelSVF_processBufferMonoGain(BMMultiLevelSVF *This,
										   const float *input, float *output,
										   const float *const *gain,
										   size_t numSamples);

/*!
 *BMMultiLevelSVF_processBufferStereoD
 *
 * @abstract Double-precision variant of processBufferStereo: double input,
 * output and state (the coefficients are the same floats). Static
 * coefficients only (filter sweep must be off). Parameter updates from
 * the setters are picked up at the start of the buffer with the state
 * preserved, so changing a cutoff while processing is click-free. Used by
 * the double-precision gain stage in BMSaturator2. In-place is fine.
 */
void BMMultiLevelSVF_processBufferStereoD(BMMultiLevelSVF *This, const double* inputL, const double* inputR, double* outputL, double* outputR, size_t numSamples);

/*!
 *BMMultiLevelSVF_processBufferMonoSplit
 *
 * @abstract Two outputs from one filter. Runs a single-level filter once and
 * writes its lowpass output to lowOut and highGain times its highpass output
 * to highOut. The state-variable filter computes the lowpass, bandpass and
 * highpass responses from the same state in every tick, so this gives exactly
 * (bit for bit) the same result as two separate filters with the same fc and
 * Q set to lowpass (mix 0,0,1) and highpass (mix highGain,0,0), for the cost
 * of one. Used by the crossovers. The level's own mix coefficients are
 * ignored; only fc and Q matter, so any 12 dB setter can configure it.
 *
 * @param This      an initialised filter with numLevels == 1
 * @param input     input buffer
 * @param lowOut    lowpass output (may alias input)
 * @param highOut   highpass output, scaled by highGain (may alias input)
 * @param highGain  1 or -1 for the crossovers; any float is allowed
 * @param numSamples length of the buffers
 */
void BMMultiLevelSVF_processBufferMonoSplit(BMMultiLevelSVF *This,
											const float *input,
											float *lowOut, float *highOut,
											float highGain,
											size_t numSamples);

/*!
 *BMMultiLevelSVF_processBufferStereoSplit
 *
 * @abstract Stereo version of BMMultiLevelSVF_processBufferMonoSplit
 */
void BMMultiLevelSVF_processBufferStereoSplit(BMMultiLevelSVF *This,
											  const float *inL, const float *inR,
											  float *lowL, float *lowR,
											  float *highL, float *highR,
											  float highGain,
											  size_t numSamples);



/**************************
   Filter setup functions
 **************************/

/*!
 *BMMultiLevelSVF_setLowpass
 */
void BMMultiLevelSVF_setLowpass12dB(BMMultiLevelSVF *This, double fc, size_t level);

/*!
 *BMMultiLevelSVF_setLowpass12dBwithQ
 */
void BMMultiLevelSVF_setLowpass12dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level);

// section Qs of the fourth-order Butterworth filter: 1/(2 cos(pi/8)) and 1/(2 cos(3 pi/8))
#define BM_SVF_BUTTERWORTH4_Q1 0.54119610014619698
#define BM_SVF_BUTTERWORTH4_Q2 1.30656296487637653

/*!
 *BMMultiLevelSVF_setLowpass24dBwithQ
 *
 * @abstract A fourth-order lowpass with one resonance control, made of two
 * 12 dB sections at the same cutoff on two levels of the cascade. The first
 * section stays at the gentle Butterworth value, Q = 1/(2 cos(pi/8)) = 0.5412,
 * and the second carries the resonance, Q = q * 1.8478. At q = 1/sqrt 2 the
 * pair is exactly the fourth-order Butterworth, and because 0.5412 * 1.8478
 * = 1 the gain at the cutoff is exactly q for every setting, the same rule
 * the 12 dB filter obeys, so the control feels the same: one clean resonant
 * peak that grows with q.
 *
 * @param This pointer to an initialised struct
 * @param fc cutoff frequency in Hz
 * @param q resonance; 1/sqrt(2) is Butterworth (maximally flat)
 * @param level1 the level for the gentle section
 * @param level2 the level for the resonant section; any other level, usually level1 + 1
 */
void BMMultiLevelSVF_setLowpass24dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level1, size_t level2);

/*!
 *BMMultiLevelSVF_setHighpass24dBwithQ
 *
 * @abstract The highpass counterpart of BMMultiLevelSVF_setLowpass24dBwithQ.
 */
void BMMultiLevelSVF_setHighpass24dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level1, size_t level2);

/*!
 *BMMultiLevelSVF_setNumActiveLevels
 *
 * @abstract Process only the first numActiveLevels levels of the cascade. A
 * bypassed level still costs a full pass over the buffer, so a caller that
 * allocates spare levels (for example a second level per band for the 24 dB
 * types) uses this to pay only for the ones in use. Set the coefficients of
 * a level before bringing it into use; its state starts from rest. Setters
 * and parameter updates still work on every level. Zero active levels pass
 * the input through unchanged, including the per-level gain path. The split
 * functions pass the input to the low output and write zero to the high output.
 */
void BMMultiLevelSVF_setNumActiveLevels(BMMultiLevelSVF *This, size_t numActiveLevels);

/*!
 *BMMultiLevelSVF_setLowpass24dB
 *
 * 	@abstract This filter requires 2 levels
 *
 *	@param This pointer to an initialized struct
 *	@param fc filter cutoff frequency
 *	@param levelStart the first level used for this filter
 *	@param levelEnd the last level used for this filter
 *
 */
void BMMultiLevelSVF_setLowpass24dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd);

/*!
 *BMMultiLevelSVF_setLowpass36dB
 *
 * 	@abstract This filter requires 3 consecutive, contiguous levels
 *
 *	@param This pointer to an initialized struct
 *	@param fc filter cutoff frequency
 *	@param levelStart the first level used for this filter
 *	@param levelEnd the last level used for this filter
 */
void BMMultiLevelSVF_setLowpass36dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd);

/*!
 *BMMultiLevelSVF_setLowpass48dB
 *
 * 	@abstract This filter requires 4 consecutive, contiguous levels
 *
 *	@param This pointer to an initialized struct
 *	@param fc filter cutoff frequency
 *	@param levelStart the first level used for this filter
 *	@param levelEnd the last level used for this filter
 */
void BMMultiLevelSVF_setLowpass48dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd);

/*!
 *BMMultiLevelSVF_setLowpass60dB
 *
 * 	@abstract This filter requires 5 consecutive, contiguous levels
 *
 *	@param This pointer to an initialized struct
 *	@param fc filter cutoff frequency
 *	@param levelStart the first level used for this filter
 *	@param levelEnd the last level used for this filter
 */
void BMMultiLevelSVF_setLowpass60dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd);

/*!
 *BMMultiLevelSVF_setHighpass24dB
 *
 * 	@abstract This filter requires 2 levels
 *
 *	@param This pointer to an initialized struct
 *	@param fc filter cutoff frequency
 *	@param levelStart the first level used for this filter
 *	@param levelEnd the last level used for this filter
 */
void BMMultiLevelSVF_setHighpass24dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd);

/*!
 *BMMultiLevelSVF_setHighpass36dB
 *
 * 	@abstract This filter requires 3 consecutive, contiguous levels
 *
 *	@param This pointer to an initialized struct
 *	@param fc filter cutoff frequency
 *	@param levelStart the first level used for this filter
 *	@param levelEnd the last level used for this filter
 */
void BMMultiLevelSVF_setHighpass36dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd);

/*!
 *BMMultiLevelSVF_setHighpass48dB
 *
 * 	@abstract This filter requires 4 consecutive, contiguous levels
 *
 *	@param This pointer to an initialized struct
 *	@param fc filter cutoff frequency
 *	@param levelStart the first level used for this filter
 *	@param levelEnd the last level used for this filter
 */
void BMMultiLevelSVF_setHighpass48dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd);

/*!
 *BMMultiLevelSVF_setHighpass60dB
 *
 * 	@abstract This filter requires 5 consecutive, contiguous levels
 *
 *	@param This pointer to an initialized struct
 *	@param fc filter cutoff frequency
 *	@param levelStart the first level used for this filter
 *	@param levelEnd the last level used for this filter
 */
void BMMultiLevelSVF_setHighpass60dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd);



/*!
 *BMMultiLevelSVF_setBandpass
 */
void BMMultiLevelSVF_setBandpass(BMMultiLevelSVF *This, double fc, double q, size_t level);

/*!
 *BMMultiLevelSVF_setHighpass
 */
void BMMultiLevelSVF_setHighpass12dB(BMMultiLevelSVF *This, double fc, size_t level);

/*!
 *BMMultiLevelSVF_setHighpassQ
 */
void BMMultiLevelSVF_setHighpass12dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level);

/*!
 *BMMultiLevelSVF_setLowpass6dB
 *
 * @abstract First-order (6 dB/octave) lowpass, -3 dB at fc. Exactly the
 * bilinear-transformed one-pole filter (same response as
 * BMMultiLevelBiquad_setLowPass6db), realised on one SVF level.
 */
void BMMultiLevelSVF_setLowpass6dB(BMMultiLevelSVF *This, double fc, size_t level);

/*!
 *BMMultiLevelSVF_setHighpass6dB
 *
 * @abstract First-order (6 dB/octave) highpass, -3 dB at fc. Exactly the
 * bilinear-transformed one-pole filter (same response as
 * BMMultiLevelBiquad_setHighPass6db), realised on one SVF level.
 */
void BMMultiLevelSVF_setHighpass6dB(BMMultiLevelSVF *This, double fc, size_t level);

/*!
 *BMMultiLevelSVF_setLowpass6dBwithQ
 *
 * @abstract 6 dB/octave lowpass with a resonance at fc, on one SVF level:
 * the lowpass plus the bandpass of a section of Q q / sqrt 2. q = 1/sqrt 2
 * is BMMultiLevelSVF_setLowpass6dB exactly (the zero cancels one pole); the
 * gain at fc is q, as for the 12 and 24 dB types, and the slope far from fc
 * stays 6 dB/octave. The response peaks once q > 0.8165.
 */
void BMMultiLevelSVF_setLowpass6dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level);

/*!
 *BMMultiLevelSVF_setHighpass6dBwithQ
 *
 * @abstract The highpass counterpart of BMMultiLevelSVF_setLowpass6dBwithQ.
 */
void BMMultiLevelSVF_setHighpass6dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level);

/*!
 *BMMultiLevelSVF_setBypass
 *
 * @abstract Set one level to unity gain (output = input, exactly), so an
 * unused level in a multi-level cascade passes the signal through unchanged.
 */
void BMMultiLevelSVF_setBypass(BMMultiLevelSVF *This, size_t level);

/*!
 *BMMultiLevelSVF_setGain
 *
 * @abstract Set one level to a flat gain (output = input * 10^(gainDb/20)
 * at every frequency): setBypass with the output mix scaled, so in sweep
 * mode a gain change ramps like any other coefficient change.
 */
void BMMultiLevelSVF_setGain(BMMultiLevelSVF *This, double gainDb, size_t level);

/*!
 *BMMultiLevelSVF_setBellBiquadQ
 *
 * @abstract The native symmetric bell of BMMultiLevelSVF_setBell with its
 * width chosen to match BMMultiLevelBiquad_setBellQ at the same nominal Q
 * (the biquad bells, Rusty Allred's formulae, map Q to a bandwidth through
 * BMMultiLevelBiquad_QToBW and are wider than the SVF's RBJ bells for the
 * same number). NOT the biquad's transfer function: the shape stays the
 * SVF's (a cut is the exact inverse of the boost), only the width is
 * matched, to about 0.01 dB for Q >= 0.5 and 0.15 dB for very wide bells.
 * For a voicing written in Allred Q that should become a native SVF bell
 * (BMSaturator2's amp models). For the biquad's exact bell use
 * BMMultiLevelSVF_setBellQAsBiquad; see the guide to the setter families
 * further down.
 *
 * @param fc       centre frequency in Hz
 * @param gainDb   bell gain in decibels
 * @param biquadQ  the Q you would have given to BMMultiLevelBiquad_setBellQ
 */
void BMMultiLevelSVF_setBellBiquadQ(BMMultiLevelSVF *This, double fc, double gainDb, double biquadQ, size_t level);

/*!
 *BMMultiLevelSVF_setBellWithSkirtBiquadQ
 *
 * @abstract Same as setBellWithSkirt but with the width of
 * BMMultiLevelBiquad_setBellWithSkirt for the same Q. See setBellBiquadQ;
 * the exact biquad version is BMMultiLevelSVF_setBellWithSkirtAsBiquad.
 */
void BMMultiLevelSVF_setBellWithSkirtBiquadQ(BMMultiLevelSVF *This, double fc, double bellGainDb, double skirtGainDb, double biquadQ, size_t level);

/*!
 *BMMultiLevelSVF_QFromBiquadQAtSampleRate
 *
 * @abstract The Q that BMMultiLevelSVF_setBell needs to give a bell the
 * same width as BMMultiLevelBiquad_setBellQ with `biquadQ` (Allred's
 * bandwidth-based Q, BMMultiLevelBiquad_QToBW): the conversion the BiquadQ
 * setters above apply. Depends on fc and the sample rate (the biquad's
 * bandwidth narrows toward Nyquist) and on the bell's gain relative to its
 * skirt, through A = 10^(|relativeGainDb| / 40); a cut converts like the
 * boost of the same size, as RBJ's symmetric bell requires.
 */
double BMMultiLevelSVF_QFromBiquadQAtSampleRate(double fc, double biquadQ, double relativeGainDb, double sampleRate);

/*!
 *BMMultiLevelSVF_setLinkwitzRileyLP
 *
 * @abstract Second-order Linkwitz-Riley lowpass (Butterworth first order
 * squared, Q = 1/2, -6 dB at fc) on one level. Same response as
 * BMMultiLevelBiquad_setLinkwitzRileyLP.
 */
void BMMultiLevelSVF_setLinkwitzRileyLP(BMMultiLevelSVF *This, double fc, size_t level);

/*!
 *BMMultiLevelSVF_setLinkwitzRileyHP
 *
 * @abstract Second-order Linkwitz-Riley highpass on one level, with the
 * output sign inverted exactly like BMMultiLevelBiquad_setLinkwitzRileyHP, so
 * that lowpass + highpass sums to an allpass.
 */
void BMMultiLevelSVF_setLinkwitzRileyHP(BMMultiLevelSVF *This, double fc, size_t level);

/*!
 *BMMultiLevelSVF_setLinkwitzRileyLP4thOrder
 *
 * @abstract Fourth-order Linkwitz-Riley lowpass (two cascaded Butterworth
 * second-order sections) on levels firstLevel and firstLevel + 1.
 */
void BMMultiLevelSVF_setLinkwitzRileyLP4thOrder(BMMultiLevelSVF *This, double fc, size_t firstLevel);

/*!
 *BMMultiLevelSVF_setLinkwitzRileyHP4thOrder
 *
 * @abstract Fourth-order Linkwitz-Riley highpass on levels firstLevel and
 * firstLevel + 1. No sign inversion is needed at fourth order.
 */
void BMMultiLevelSVF_setLinkwitzRileyHP4thOrder(BMMultiLevelSVF *This, double fc, size_t firstLevel);

/*!
 *BMMultiLevelSVF_setAllpass
 */
void BMMultiLevelSVF_setAllpass(BMMultiLevelSVF *This, double fc, double q, size_t level);

/*!
 *BMMultiLevelSVF_setBell
 */
void BMMultiLevelSVF_setBell(BMMultiLevelSVF *This, double fc, double gainDb, double q, size_t level);

/*!
 *BMMultiLevelSVF_setBellWithSkirt
 */
void BMMultiLevelSVF_setBellWithSkirt(BMMultiLevelSVF *This, double fc, double bellGainDb, double skirtGainDb, double q, size_t level);

/*!
 *BMMultiLevelSVF_setLowShelf
 *
 * @abstract set shelf with default slope = 1
 */
void BMMultiLevelSVF_setLowShelf(BMMultiLevelSVF *This, double fc, double gainDb, size_t level);

/*!
 *BMMultiLevelSVF_setLowShelfS
 *
 * @param fc cutoff frequency
 * @param gainDb shelf gain in decibels
 * @param S slope in [0.5,1]
 */
void BMMultiLevelSVF_setLowShelfS(BMMultiLevelSVF *This, double fc, double gainDb, double S, size_t level);

/*!
 *BMMultiLevelSVF_setHighShelf
 *
 * @abstract set shelf with default slope = 1
 */
void BMMultiLevelSVF_setHighShelf(BMMultiLevelSVF *This, double fc, double gainDb, size_t level);

/*!
 *BMMultiLevelSVF_setHighShelfS
 *
 * @param fc cutoff frequency
 * @param gainDb shelf gain in decibels
 * @param S slope in [0.5,1]
 */
void BMMultiLevelSVF_setHighShelfS(BMMultiLevelSVF *This, double fc, double gainDb, double S, size_t level);

/*!
 *BMMultiLevelSVF_setLowShelfAdjustableSlope
 *
 * @abstract Low shelf with the Robert Bristow-Johnson cookbook definition of
 * the slope parameter. Identical response to
 * BMMultiLevelBiquad_setLowShelfAdjustableSlope with the same arguments.
 * The gain at fc is half the shelf gain (in dB). slope = 1 is the steepest
 * monotonic shelf (Allred's second-order shelf); slope = 0.5 is Allred's
 * first-order shelf. See
 * https://bmtechjournal.wordpress.com/2019/10/22/how-to-use-rbj-shelving-filters/
 *
 * @param fc      corner frequency (midpoint of the transition, in dB)
 * @param gainDb  shelf gain in decibels
 * @param slope   RBJ shelf slope, in (0, 1]
 */
void BMMultiLevelSVF_setLowShelfAdjustableSlope(BMMultiLevelSVF *This, double fc, double gainDb, double slope, size_t level);

/*!
 *BMMultiLevelSVF_setHighShelfAdjustableSlope
 *
 * @abstract High shelf with the RBJ cookbook slope parameter. Identical
 * response to BMMultiLevelBiquad_setHighShelfAdjustableSlope with the same
 * arguments. See BMMultiLevelSVF_setLowShelfAdjustableSlope.
 */
void BMMultiLevelSVF_setHighShelfAdjustableSlope(BMMultiLevelSVF *This, double fc, double gainDb, double slope, size_t level);

/*!
 *BMMultiLevelSVF_setLowShelfQ
 *
 * @abstract The RBJ cookbook low shelf parametrised by the Q of its
 * second-order section instead of the slope: the same filter as
 * BMMultiLevelSVF_setLowShelfAdjustableSlope with
 * 1/Q = sqrt((A + 1/A)(1/slope - 1) + 2), A = 10^(gainDb/40)
 * (BMMultiLevelSVF_shelfQFromSlope). Q = 1/sqrt(2) is slope 1, the
 * steepest monotonic shelf, for any gain; Q = 1/(sqrt(A) + 1/sqrt(A)) is
 * slope 0.5, the first-order shelf; above 1/sqrt(2) the shelf overshoots
 * around fc (a resonant shelf). The gain at fc is half the shelf gain in
 * dB. Q is the section's own Q, so it does not change with the gain,
 * which makes it the natural parameter for a user control.
 *
 * @param fc      corner frequency (midpoint of the transition, in dB)
 * @param gainDb  shelf gain in decibels
 * @param Q       Q of the section, > 0
 */
void BMMultiLevelSVF_setLowShelfQ(BMMultiLevelSVF *This, double fc, double gainDb, double Q, size_t level);

/*!
 *BMMultiLevelSVF_setHighShelfQ
 *
 * @abstract The RBJ cookbook high shelf with the section Q as the
 * parameter. See BMMultiLevelSVF_setLowShelfQ.
 */
void BMMultiLevelSVF_setHighShelfQ(BMMultiLevelSVF *This, double fc, double gainDb, double Q, size_t level);

/*!
 *BMMultiLevelSVF_shelfQFromSlope
 *
 * @abstract The section Q of the RBJ shelf with the given gain and slope:
 * 1/sqrt((A + 1/A)(1/slope - 1) + 2), A = 10^(gainDb/40). The AdjustableSlope
 * setters are the Q setters with this Q.
 */
double BMMultiLevelSVF_shelfQFromSlope(double gainDb, double slope);

/*!
 *BMMultiLevelSVF_setHighShelfFirstOrder
 *
 * @abstract First-order high shelf, identical response to
 * BMMultiLevelBiquad_setHighShelfFirstOrder: for boost the pole is at fc and
 * the zero at fc / G (G the linear gain); for cut the zero is at fc and the
 * pole at G fc, so a cut is the exact inverse of the boost by the same
 * number of dB. The section's spare pole and zero cancel. For a gain that
 * is modulated, prefer BMMultiLevelSVF_setHighShelfFirstOrderFixedPole.
 *
 * @param fc      corner frequency: the pole (boost) or the zero (cut)
 * @param gainDb  shelf gain in decibels
 */
void BMMultiLevelSVF_setHighShelfFirstOrder(BMMultiLevelSVF *This, double fc, double gainDb, size_t level);

/*!
 *BMMultiLevelSVF_setLowShelfFirstOrder
 *
 * @abstract First-order low shelf, identical response to
 * BMMultiLevelBiquad_setLowShelfFirstOrder. See
 * BMMultiLevelSVF_setHighShelfFirstOrder.
 */
void BMMultiLevelSVF_setLowShelfFirstOrder(BMMultiLevelSVF *This, double fc, double gainDb, size_t level);

/*!
 *BMMultiLevelSVF_setHighShelfFirstOrderFixedPole
 *
 * @abstract First-order high shelf whose pole stays at fc for any gain:
 * H(s) = (G s + 1) / (s + 1), zero at fc / G. Boost is the same as
 * BMMultiLevelSVF_setHighShelfFirstOrder; cut is not its inverse (the zero
 * moves instead of the pole). The point of this variant is modulation: g and
 * k never depend on the gain, so calling this setter again with a new gain
 * changes only the output mix coefficients, linearly in G. The recursion and
 * its state are untouched, there is nothing to recompute but the gain, and
 * with filter sweep on the linear coefficient ramp across a buffer is exactly
 * a linear ramp of G. Any update rate is safe. For per-sample gain use
 * BMMultiLevelSVF_processBufferMonoGain, where this level's
 * gain array is the shelf gain.
 *
 * @param fc      pole frequency
 * @param gainDb  shelf gain in decibels
 */
void BMMultiLevelSVF_setHighShelfFirstOrderFixedPole(BMMultiLevelSVF *This, double fc, double gainDb, size_t level);

/*!
 *BMMultiLevelSVF_setLowShelfFirstOrderFixedPole
 *
 * @abstract First-order low shelf with the pole fixed at fc:
 * H(s) = (s + G) / (s + 1), zero at G fc. See
 * BMMultiLevelSVF_setHighShelfFirstOrderFixedPole.
 */
void BMMultiLevelSVF_setLowShelfFirstOrderFixedPole(BMMultiLevelSVF *This, double fc, double gainDb, size_t level);

/*!
 *BMMultiLevelSVF_impulseResponse
 */
void BMMultiLevelSVF_impulseResponse(BMMultiLevelSVF *This,size_t frameCount);


/*!
 *BMMultiLevelSVF_enableFilterSweep
 *
 * @abstract enable or disable smooth interpolation of parameters for filter sweeps. When sweeps are enabled, the filter interpolates the coefficient update so that it reaches the new filter coefficient values on the first sample of the following call to the process function. This means that if you want the filter to take more than one audio buffer to smoothly move toward a new configuration then you need to gradually update the filter parameters each time you call the BMMultiLevelSVF process function. This is a good way to do a filter sweep because the coefficient interpolation that happens automatically in this class is linear. The linear interpolation is efficient for sample by sample processing and for slow sweeps it is sufficiently accurate but if we were to use linear interpolation over longer sweeps it could lead to unexpected results midway through the sweep, because the filter coefficients generally don't have a liner relationship to parameters like cutoff frequency and Q.
 *
 * @param This    pointer to an initialised struct
 * @param sweepOn set true for filter sweep mode
 */
void BMMultiLevelSVF_enableFilterSweep(BMMultiLevelSVF *This, bool sweepOn);


/**************************************
   BMMultiLevelBiquad's setters on the SVF
 **************************************/

/*
 * Three families of setters exist on this filter. Which to use:
 *
 * 1. The native setters above (setLowpass12dBwithQ, setBell, setBellWithSkirt,
 *    setLowShelfQ, setHighShelfAdjustableSlope, setLowpass6dB, ...): the
 *    SVF's own designs, straight from the analog prototypes (Cytomic /
 *    RBJ cookbook), with parameters in their textbook meaning: Q is the
 *    section's Q, a bell is symmetric (a cut is the inverse of the boost at
 *    the same Q), 12 dB filters are Butterworth by default. For new code.
 *
 * 2. The ...AsBiquad setters below: one for each BMMultiLevelBiquad setter,
 *    with the biquad setter's exact argument list (order, types, meaning:
 *    a bandwidth in Hz for setBellAsBiquad, Allred's Q for setBellQAsBiquad
 *    and setBellWithSkirtAsBiquad, the cookbook slope for the AdjustableSlope
 *    shelves) and exactly its transfer function: they call the biquad's own
 *    BMMultiLevelBiquad_design* function and convert the coefficients
 *    (BMMultiLevelSVF_fromBiquadCoefs, exact). To replace a BMMultiLevelBiquad
 *    by a BMMultiLevelSVF without changing any filter setting, rename each
 *    BMMultiLevelBiquad_setX(...) call to BMMultiLevelSVF_setXAsBiquad(...).
 *    BiquadMimicTests checks every pair. Not mirrored: setGain /
 *    setGainInstant (use a BMSmoothGain), setActiveOnLevel (use setBypass),
 *    setNormalizedBell.
 *
 * 3. setBellBiquadQ and setBellWithSkirtBiquadQ: the native, symmetric bell
 *    (family 1) with its width chosen to match the biquad bell of the given
 *    Allred Q (BMMultiLevelSVF_QFromBiquadQAtSampleRate). Not the biquad's
 *    transfer function: it keeps the SVF's boost/cut symmetry and matches the
 *    biquad's width to about 0.01 dB for Q >= 0.5 (0.15 dB for very wide
 *    bells). For voicings specified in Allred Q that should stay native SVF
 *    bells (BMSaturator2's amp models, whose user-editable copies in the
 *    synth are native bells).
 */

void BMMultiLevelSVF_setBypassAsBiquad(BMMultiLevelSVF *This, size_t level);
/*! BMMultiLevelBiquad_setCoefficientZ: coeff is {b0, b1, b2, a1, a2} (the first channel's five of the biquad's per-channel array). */
void BMMultiLevelSVF_setCoefficientZAsBiquad(BMMultiLevelSVF *This, size_t level, const double *coeff);
void BMMultiLevelSVF_setBellAsBiquad(BMMultiLevelSVF *This, float fc, float bandwidth, float gain_db, size_t level);
void BMMultiLevelSVF_setBellQAsBiquad(BMMultiLevelSVF *This, float fc, float Q, float gain_db, size_t level);
void BMMultiLevelSVF_setBellWithSkirtAsBiquad(BMMultiLevelSVF *This, float fc, float Q, float bellGainDb, float skirtGainDb, size_t level);
void BMMultiLevelSVF_setHighShelfAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, size_t level);
void BMMultiLevelSVF_setLowShelfAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, size_t level);
void BMMultiLevelSVF_setHighShelfAdjustableSlopeAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, float slope, size_t level);
void BMMultiLevelSVF_setLowShelfAdjustableSlopeAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, float slope, size_t level);
void BMMultiLevelSVF_setHighShelfFirstOrderAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, size_t level);
void BMMultiLevelSVF_setLowShelfFirstOrderAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, size_t level);
void BMMultiLevelSVF_setLowPass12dbAsBiquad(BMMultiLevelSVF *This, double fc, size_t level);
void BMMultiLevelSVF_setLowPassQ12dbAsBiquad(BMMultiLevelSVF *This, double fc, double q, size_t level);
void BMMultiLevelSVF_setHighPass12dbAsBiquad(BMMultiLevelSVF *This, double fc, size_t level);
void BMMultiLevelSVF_setHighPass12dbNegAsBiquad(BMMultiLevelSVF *This, double fc, size_t level);
void BMMultiLevelSVF_setHighPassQ12dbAsBiquad(BMMultiLevelSVF *This, double fc, double q, size_t level);
void BMMultiLevelSVF_setHighOrderBWLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel, size_t numLevels);
void BMMultiLevelSVF_setLegendreLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel, size_t numLevels);
void BMMultiLevelSVF_setCriticallyDampedLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel, size_t numLevels);
void BMMultiLevelSVF_setBesselLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel, size_t numLevels);
void BMMultiLevelSVF_setLowPass6dbAsBiquad(BMMultiLevelSVF *This, double fc, size_t level);
void BMMultiLevelSVF_setHighPass6dbAsBiquad(BMMultiLevelSVF *This, double fc, size_t level);
void BMMultiLevelSVF_setHighPassLowPassAsBiquad(BMMultiLevelSVF *This, double highPassFc, double lowPassFc, size_t level);
void BMMultiLevelSVF_setLinkwitzRileyLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t level);
void BMMultiLevelSVF_setLinkwitzRileyHPAsBiquad(BMMultiLevelSVF *This, double fc, size_t level);
void BMMultiLevelSVF_setLinkwitzRileyLP4thOrderAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel);
void BMMultiLevelSVF_setLinkwitzRileyHP4thOrderAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel);
void BMMultiLevelSVF_setAllpass2ndOrderAsBiquad(BMMultiLevelSVF *This, double c1, double c2, size_t level);
void BMMultiLevelSVF_setAllpass1stOrderAsBiquad(BMMultiLevelSVF *This, double c, size_t level);
void BMMultiLevelSVF_setCriticallyDampedPhaseCompensatorAsBiquad(BMMultiLevelSVF *This, double lowpassFC, size_t level);


/**************************************
   Biquad <-> SVF coefficient conversion
 **************************************/

/*!
 *BMSVFSectionCoefs
 *
 * @abstract The coefficients of one level of this filter: the trapezoidal SVF
 * coefficients g0, g1, g2 and damping k (see setCoefficientsHelper) and the
 * output mix m0 (high), m1 (band), m2 (low).
 */
typedef struct BMSVFSectionCoefs {
	double g0, g1, g2, k, m0, m1, m2;
} BMSVFSectionCoefs;

/* BMBiquadSectionCoefs is defined in BMMultiLevelBiquad.h */

/*!
 *BMMultiLevelSVF_fromBiquadCoefs
 *
 * @abstract Convert biquad coefficients to SVF coefficients with the same
 * transfer function. Exact for every stable biquad (including first-order
 * sections with b2 = a2 = 0). The derivation is in BMMultiLevelSVF.c above
 * the definition. Asserts 1 + a1 + a2 > 0 and 1 - a1 + a2 > 0, which every
 * stable biquad satisfies.
 */
BMSVFSectionCoefs BMMultiLevelSVF_fromBiquadCoefs(BMBiquadSectionCoefs b);

/*!
 *BMMultiLevelSVF_toBiquadCoefs
 *
 * @abstract Convert SVF coefficients to biquad coefficients with the same
 * transfer function. The exact inverse of BMMultiLevelSVF_fromBiquadCoefs.
 */
BMBiquadSectionCoefs BMMultiLevelSVF_toBiquadCoefs(BMSVFSectionCoefs c);

/*!
 *BMMultiLevelSVF_setFromBiquadCoefficients
 *
 * @abstract Set one level of the SVF to the transfer function
 * H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2). This lets us
 * use the filter design functions of BMMultiLevelBiquad on the SVF: design
 * the biquad, read its coefficients, set them here. The update goes through
 * the same pending / target mechanism as the other setters, so filter sweep
 * interpolation applies to it as usual.
 *
 * @param b0, b1, b2  feed-forward coefficients
 * @param a1, a2      feedback coefficients (a0 = 1, the vDSP convention)
 * @param level       the level to set
 */
void BMMultiLevelSVF_setFromBiquadCoefficients(BMMultiLevelSVF *This,
											   double b0, double b1, double b2,
											   double a1, double a2,
											   size_t level);

/*!
 *BMMultiLevelSVF_getBiquadCoefficients
 *
 * @abstract The biquad coefficients of one level, i.e. the direct form filter
 * with the same transfer function as that level. Reads the most recently set
 * (pending) coefficients.
 *
 * @param coefficients  output, five doubles: b0, b1, b2, a1, a2 (the layout of
 *                      one channel of BMMultiLevelBiquad.coefficients_d and of
 *                      BMMultiLevelBiquad_setCoefficientZ)
 */
void BMMultiLevelSVF_getBiquadCoefficients(BMMultiLevelSVF *This, size_t level, double *coefficients);

/*!
 *BMMultiLevelSVF_copyFromBiquad
 *
 * @abstract Set every level of this SVF to the transfer function of the
 * corresponding level of a BMMultiLevelBiquad. Use it to configure the SVF
 * with the setters that only exist for the biquad (Bessel, Legendre, high
 * order Butterworth, critically damped, packed first-order pairs...): set up
 * the biquad, then copy. Both filters must have the same number of levels.
 * The biquad's broadband gain (BMMultiLevelBiquad_setGain) is not part of its
 * coefficients and is not copied.
 */
void BMMultiLevelSVF_copyFromBiquad(BMMultiLevelSVF *This, const BMMultiLevelBiquad *biquad);

/*!
 *BMMultiLevelSVF_copyToBiquad
 *
 * @abstract Set every level of a BMMultiLevelBiquad to the transfer function
 * of the corresponding level of this SVF (the inverse of copyFromBiquad).
 * Both filters must have the same number of levels.
 */
void BMMultiLevelSVF_copyToBiquad(BMMultiLevelSVF *This, BMMultiLevelBiquad *biquad);



/**************************************
   Transfer function, group delay, phase
 **************************************/

/*!
 *BMMultiLevelSVF_tfMagVector
 *
 * @abstract Magnitude of the transfer function of the whole cascade at each
 * frequency in frequency[], written to magnitude[]. Same result as
 * BMMultiLevelBiquad_tfMagVector for the same filter (the levels are
 * converted to biquad coefficients and evaluated with the same code).
 */
void BMMultiLevelSVF_tfMagVector(BMMultiLevelSVF *This, const float *frequency, float *magnitude, size_t length);

/*!
 *BMMultiLevelSVF_tfMagVectorAtLevel
 *
 * @abstract Magnitude of the transfer function of one level only.
 */
void BMMultiLevelSVF_tfMagVectorAtLevel(BMMultiLevelSVF *This, const float *frequency, float *magnitude, size_t length, size_t level);

/*!
 *BMMultiLevelSVF_groupDelay
 *
 * @abstract Group delay in samples of the whole cascade at freq (Hz). Same
 * result as BMMultiLevelBiquad_groupDelay for the same filter.
 */
double BMMultiLevelSVF_groupDelay(BMMultiLevelSVF *This, double freq);

/*!
 *BMMultiLevelSVF_phaseResponse
 *
 * @abstract Phase response of the whole cascade at freq (Hz), in radians,
 * wrapped to (-pi, pi]. Same result as BMMiltiLevelBiquad_phaseResponse for
 * the same filter.
 */
double BMMultiLevelSVF_phaseResponse(BMMultiLevelSVF *This, double freq);


/*!
 *BMMultiLevelSVF_forceImmediateUpdate
 *
 * @abstract call this to force the filter to do the currently queued update immediately even when smooth update is on
 */
void BMMultiLevelSVF_forceImmediateUpdate(BMMultiLevelSVF *This);


/*!
 *BMMultiLevelSVF_clearBuffers
 *
 * Sets a flag that will cause the state variables to be set to zero before processing the next buffer of audio samples
 */
void BMMultiLevelSVF_clearBuffers(BMMultiLevelSVF *This);


#endif /* BMMultiLevelSVF_h */
