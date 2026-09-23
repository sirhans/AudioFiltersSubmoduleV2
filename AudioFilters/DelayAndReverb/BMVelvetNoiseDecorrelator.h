//
//  BMVelvetNoiseDecorrelator.h
//  Saturator
//
//  Created by TienNM on 12/4/17
//  Rewritten by Hans on 9 October 2019
//  Anyone may use this file without restrictions
//

#ifndef BMVelvetNoiseDecorrelator_h
#define BMVelvetNoiseDecorrelator_h

#include <stdio.h>
#include "BMMultiTapDelay.h"
#include "BMSmoothSwitch.h"
#include "../MathUtilities/BMRandom.h"

// Historical seed, retained for callers that explicitly want the old pattern.
// Ordinary initializers now assign distinct per-instance seeds.
#define BM_VND_DEFAULT_SEED 20260912

typedef struct BMVelvetNoiseDecorrelator {
    BMMultiTapDelay multiTapDelay;
    float sampleRate, wetMix, rt60, maxDelayTimeS;
    // even tap density only: the first wet tap is placed after this time.
    // The default is maxDelayTimeS / numWetTaps (one grid cell).
    float minDelayTimeS;
    // BMVelvetNoiseDecorrelator_setDryAndWetTapGains: when set, the gains are
    // dryGain for the dry tap and wetTapGain * sign * decay for the wet taps,
    // with no normalisation; setWetMix switches back to the mix scale.
    bool useExplicitGains;
    // explicit gains with the wet taps normalised: see BMVelvetNoiseDecorrelator_setDryGainAndWetEnergy
    bool normaliseWetTaps;
    // dry and wet scaled together to a total energy of 1: see BMVelvetNoiseDecorrelator_setDryAndWetAtUnitEnergy
    bool unitTotalEnergy;
    float dryGain, wetTapGain;
	size_t *delayLengthsL, *delayLengthsR;
	float *gainsL, *gainsR;
    float fadeInSamples;
	bool hasDryTap, evenTapDensity;
	size_t numWetTaps;
    float lastTapGainL;
    float lastTapGainR;
    bool resetNumTaps;
    bool resetRT60DecayTime;
    bool resetFadeIn;
    float* tempBuffer;
    size_t numInput;
    BMSmoothSwitch offSwitchL;
    BMSmoothSwitch offSwitchR;
    BMRandom rng;
} BMVelvetNoiseDecorrelator;




/*!
 *BMVelvetNoiseDecorrelator_init
 *
 * @abstract Owns its randomizer; automatic seeds differ across instances within
 * the process (until the 32-bit counter wraps). Use initWithRandomizerState
 * for a reproducible stream independent of other initializations.
 *
 * @param This  pointer to an unitialised struct
 * @param maxDelaySeconds maximum delay time in seconds
 * @param numTaps number of delay taps on each channel including the one used to pass the dry signal through
 * @param rt60DecayTimeSeconds controls the decay envelope used to slope the volume of the taps
 * @param hasDryTap set true if you want to mix wet and dry signal; false for 100% wet
 * @param sampleRate sample rate in Hz
 */
void BMVelvetNoiseDecorrelator_init(BMVelvetNoiseDecorrelator *This,
									float maxDelaySeconds,
									size_t numTaps,
									float rt60DecayTimeSeconds,
									bool hasDryTap,
									float sampleRate);

/*!
 *BMVelvetNoiseDecorrelator_initWithRandomizerState
 *
 * @abstract Advanced initialization with an owned copy of a caller's initialized
 * BMRandom. Copies its entire current state, not just its seed. Does not advance
 * or retain the caller's object; it may be modified or discarded after this call.
 * Identical state, settings and subsequent calls produce identical taps.
 *
 * @param minDelaySeconds -1 for the usual minimum delay. For even tap density,
 * a nonnegative value selects the explicit range used by initWithDelayRange.
 * @param evenTapDensity false for init's uniform random placement; true for
 * initWithEvenTapDensity's one-tap-per-cell placement.
 * @param randomizerState non-NULL, initialized with BMRandom_init or otherwise
 * advanced to the desired position in its stream. The other arguments match init.
 */
void BMVelvetNoiseDecorrelator_initWithRandomizerState(BMVelvetNoiseDecorrelator *This,
                                                float minDelaySeconds,
                                                float maxDelaySeconds,
                                                size_t numTaps,
                                                float rt60DecayTimeSeconds,
                                                bool hasDryTap,
                                                bool evenTapDensity,
                                                float sampleRate,
                                                const BMRandom *randomizerState);

/*!
*BMVelvetNoiseDecorrelator_initWithEvenTapDensity
*
* @param This  pointer to an unitialised struct
* @param maxDelaySeconds maximum delay time in seconds
* @param numTaps number of delay taps on each channel including the one used to pass the dry signal through
* @param rt60DecayTimeSeconds controls the decay envelope used to slope the volume of the taps
* @param hasDryTap set true if you want to mix wet and dry signal; false for 100% wet
* @param sampleRate sample rate in Hz
*/
void BMVelvetNoiseDecorrelator_initWithEvenTapDensity(BMVelvetNoiseDecorrelator *This,
													  float maxDelaySeconds,
													  size_t numTaps,
													  float rt60DecayTimeSeconds,
													  bool hasDryTap,
													  float sampleRate);

/*!
 *BMVelvetNoiseDecorrelator_initWithDelayRange
 *
 * @abstract Even tap density with the wet taps spread between minDelaySeconds
 * and maxDelaySeconds (one tap per grid cell, jittered). Otherwise as
 * BMVelvetNoiseDecorrelator_initWithEvenTapDensity.
 */
void BMVelvetNoiseDecorrelator_initWithDelayRange(BMVelvetNoiseDecorrelator *This,
												  float minDelaySeconds,
												  float maxDelaySeconds,
												  size_t numTaps,
												  float rt60DecayTimeSeconds,
												  bool hasDryTap,
												  float sampleRate);

/*!
 *BMVelvetNoiseDecorrelator_setWetMix
 */
void BMVelvetNoiseDecorrelator_setWetMix(BMVelvetNoiseDecorrelator *This, float wetMix01);

/*!
 *BMVelvetNoiseDecorrelator_setDryAndWetTapGains
 *
 * @abstract Set the gains directly instead of through the wet mix: the dry
 * tap gets dryGain and each wet tap gets wetTapGain times its random sign
 * times its RT60 decay, so the first wet tap is about wetTapGain relative to
 * a dry tap of 1. Nothing is normalised. Regenerates the tap signs (the
 * seeded stream continues). Requires a dry tap.
 */
void BMVelvetNoiseDecorrelator_setDryAndWetTapGains(BMVelvetNoiseDecorrelator *This, float dryGain, float wetTapGain);

/*!
 *BMVelvetNoiseDecorrelator_setDryGainAndWetEnergy
 *
 * @abstract Like BMVelvetNoiseDecorrelator_setDryAndWetTapGains, but with the
 * wet taps' level taken apart from their shape: the wet taps of each channel
 * (signs, RT60 decay, fade-in) are first normalised so that the root of the
 * sum of their squared gains is 1, and then scaled by wetGain. wetGain is
 * therefore the energy gain of the whole burst of wet taps, whatever the
 * decay time, the delay range and the number of taps are: changing those
 * changes how the reflections are spread, not how loud they are, which the
 * first-tap gain of setDryAndWetTapGains does not give (a longer decay or
 * more taps adds energy there). The dry tap gets dryGain. The setting
 * persists through the decorrelator's own regenerations of its gains (RT60,
 * seed, randomise). BMVelvetNoiseDecorrelator_setWetTapGain changes wetGain
 * while it runs. Regenerates the tap signs (the seeded stream continues).
 * Requires a dry tap.
 */
void BMVelvetNoiseDecorrelator_setDryGainAndWetEnergy(BMVelvetNoiseDecorrelator *This, float dryGain, float wetGain);

/*!
 *BMVelvetNoiseDecorrelator_setDryAndWetAtUnitEnergy
 *
 * @abstract Like BMVelvetNoiseDecorrelator_setDryGainAndWetEnergy, but dryGain
 * and wetGain give only the balance between the dry tap and the burst of wet
 * taps: the two are scaled together, by 1 / sqrt(dryGain^2 + wetGain^2), so
 * that in each channel the sum of all the squared tap gains, dry tap
 * included, is 1. For noise-like input the output then has the input's
 * energy whatever the balance is, so a wet control built on this changes
 * how much of the sound is reflections without changing how loud it is, and
 * the decorrelator is as loud as its bypass. (For tones the level still
 * varies with frequency: the taps are a comb filter.)
 * BMVelvetNoiseDecorrelator_setWetTapGain changes wetGain while it runs, and
 * in this mode rescales the dry tap with it. Requires a dry tap.
 */
void BMVelvetNoiseDecorrelator_setDryAndWetAtUnitEnergy(BMVelvetNoiseDecorrelator *This, float dryGain, float wetGain);

/*!
 *BMVelvetNoiseDecorrelator_getNumTaps
 *
 * @returns the number of taps per channel, the dry tap included if there is
 * one: the length of the arrays the two functions below fill.
 */
size_t BMVelvetNoiseDecorrelator_getNumTaps(const BMVelvetNoiseDecorrelator *This);

/*!
 *BMVelvetNoiseDecorrelator_getTapTimes
 *
 * @abstract Copy out the delay time of every tap, in seconds, for the left
 * and the right channel. With a dry tap, it is element 0, at time 0. For
 * display and for tests; call it from the thread that configures the
 * decorrelator, not while another thread is reconfiguring it.
 *
 * @param timesL array of length BMVelvetNoiseDecorrelator_getNumTaps
 * @param timesR array of the same length
 */
void BMVelvetNoiseDecorrelator_getTapTimes(const BMVelvetNoiseDecorrelator *This, float *timesL, float *timesR);

/*!
 *BMVelvetNoiseDecorrelator_getTapGains
 *
 * @abstract Copy out the gain of every tap, for the left and the right
 * channel, in the order of BMVelvetNoiseDecorrelator_getTapTimes. The gains
 * are linear and signed: a wet tap's sign is its velvet-noise sign, its
 * magnitude the RT60 decay at its time, the fade-in if there is one, and the
 * wet mix or the explicit wet tap gain. With a dry tap, it is element 0.
 *
 * @param gainsL array of length BMVelvetNoiseDecorrelator_getNumTaps
 * @param gainsR array of the same length
 */
void BMVelvetNoiseDecorrelator_getTapGains(const BMVelvetNoiseDecorrelator *This, float *gainsL, float *gainsR);

/*!
 *BMVelvetNoiseDecorrelator_setWetTapGain
 *
 * @abstract Change the wet tap gain set by _setDryAndWetTapGains while the
 * decorrelator runs, for a wet control: the wet taps are rescaled in place,
 * so their signs, times and decay stay as they are and only their level
 * moves (setDryAndWetTapGains would draw new signs on every call, and the
 * reflections would sound different after each). The dry tap is untouched,
 * except at unit total energy (_setDryAndWetAtUnitEnergy), where it is
 * rescaled with the wet taps to keep the total at 1.
 * Requires explicit gains and wetTapGain > 0; the new gains reach the audio
 * thread through the multi-tap delay's own queued update.
 */
void BMVelvetNoiseDecorrelator_setWetTapGain(BMVelvetNoiseDecorrelator *This, float wetTapGain);


void BMVelvetNoiseDecorrelator_setRT60DecayTime(BMVelvetNoiseDecorrelator *This, float rt60DT);

/*!
 *BMVelvetNoiseDecorrelator_randomiseAll
 */
void BMVelvetNoiseDecorrelator_randomiseAll(BMVelvetNoiseDecorrelator *This);

/*!
 *BMVelvetNoiseDecorrelator_setSeed
 *
 * @abstract Re-seed the random number generator and regenerate the tap times
 * and signs. The delay picks the new times up at the end of its next buffer.
 */
void BMVelvetNoiseDecorrelator_setSeed(BMVelvetNoiseDecorrelator *This, uint32_t seed);

void BMVelvetNoiseDecorrelator_setFadeIn(BMVelvetNoiseDecorrelator *This,float fadeInS);

/*!
 *BMVelvetNoiseDecorrelator_free
 */
void BMVelvetNoiseDecorrelator_free(BMVelvetNoiseDecorrelator *This);




/*!
 *BMVelvetNoiseDecorrelator_processBufferStereo
 */
void BMVelvetNoiseDecorrelator_processBufferStereo(BMVelvetNoiseDecorrelator *This,
                                                   float* inputL,
                                                   float* inputR,
                                                   float* outputL,
                                                   float* outputR,
                                                   size_t length);

void BMVelvetNoiseDecorrelator_processMultiChannelInput(BMVelvetNoiseDecorrelator *This,
                                                    float** inputL,
                                                    float** inputR,
                                                    float* outputL,
                                                    float* outputR,
                                                    size_t length);

void BMVelvetNoiseDecorrelator_processBufferStereoWithFinalOutput(BMVelvetNoiseDecorrelator *This,
                                                    float* inputL,
                                                    float* inputR,
                                                    float* outputL,
                                                    float* outputR,
                                                    float* finalOutputL,
                                                    float* finalOutputR,
                                                    size_t length);
/*!
 *BMVelvetNoiseDecorrelator_processBufferMonoToStereo
 */
void BMVelvetNoiseDecorrelator_processBufferMonoToStereo(BMVelvetNoiseDecorrelator *This,
                                                   float* inputL,
                                                   float* outputL, float* outputR,
                                                   size_t length);


void BMVelvetNoiseDecorrelator_setNumTaps(BMVelvetNoiseDecorrelator *This, size_t numTaps);

#endif /* BMVelvetNoiseDecorrelator_h */
