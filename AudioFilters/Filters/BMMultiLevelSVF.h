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
	
    size_t numLevels;
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
 *BMMultiLevelSVF_setBypass
 *
 * @abstract Set one level to unity gain (output = input, exactly), so an
 * unused level in a multi-level cascade passes the signal through unchanged.
 */
void BMMultiLevelSVF_setBypass(BMMultiLevelSVF *This, size_t level);

/*!
 *BMMultiLevelSVF_setBellBiquadQ
 *
 * @abstract Bell filter with the same width as BMMultiLevelBiquad_setBellQ
 * with the same Q. The biquad bells (Rusty Allred's formulae) map Q to a
 * bandwidth through BMMultiLevelBiquad_QToBW, so for the same nominal Q they
 * are wider than the SVF (Simper / RBJ) bells of setBell. Use this when
 * replacing a biquad bell so the voicing does not change.
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
 * BMMultiLevelBiquad_setBellWithSkirt for the same Q. See setBellBiquadQ.
 */
void BMMultiLevelSVF_setBellWithSkirtBiquadQ(BMMultiLevelSVF *This, double fc, double bellGainDb, double skirtGainDb, double biquadQ, size_t level);

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


/*!
 *BMMultiLevelSVF_copyStateFromBiquadHelper
 *
 * The biquadHelper is a BMMultiLevelBiquad struct that functions as a model
 * for this SVF to follow. First we set the biquad filter to the desired
 * configuration. Then we copy that configuration over to the SVF. This allows
 * us to use code written for the biquad filter in the SVF filter without
 * deriving new formulae for the filter coefficients.
 *
 * The key to this is the formula in _setFromBiquad() that converts biquad
 * filter coefficient values into SVF coefficients and results in an SVF filter
 * with the same transfer function.
 *
 */
//void BMMultiLevelSVF_copyStateFromBiquadHelper(BMMultiLevelSVF *This);


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
