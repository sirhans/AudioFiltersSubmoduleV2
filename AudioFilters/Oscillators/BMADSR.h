//
//  BMADSR.h
//  AudioFilters
//
//  ADSR envelope generator, one value per sample, built as a critically
//  damped lowpass filter driven by a target level.
//
//  The envelope is the output of three identical second-order sections in
//  series, each a trapezoidal state-variable lowpass with Q = 0.5, so six
//  coincident real poles. The input to the chain is a target level that
//  steps between 1 (attack), the sustain level (decay) and 0 (release).
//  Every stage of the envelope is therefore the step response of a six-pole
//  critically damped filter: it starts with zero slope, never overshoots,
//  and its spectrum falls at 36 dB per octave above the cutoff, so a fast
//  attack or release adds almost nothing above a few times 1/time. A
//  one-pole envelope starts each stage with a slope discontinuity, whose
//  6 dB per octave skirt is what makes short attacks and releases click.
//
//  The cutoff is switched at each phase boundary; the times are:
//    attack   time from 0 to 90 % of full level;
//    decay    time to move 60 dB (a factor of 1000) of the way to sustain;
//    release  time from gate-off to 60 dB down, as before.
//  Decay begins when the attack has come within 60 dB of full level, at
//  about 1.8 attack times, where the slope is small enough to drop.
//  BMADSR_isIdle returns true once the release has fallen to -100 dB, which
//  is the moment a voice can safely stop rendering.
//
//  Switching the cutoff on a running filter is done without a jump in the
//  output or its slope. Switching to a faster cutoff (gate-on during a
//  release, a decay that is faster than the attack) scales the filter's
//  velocity and the spacing between its sections by the cutoff ratio, which
//  is the state the faster filter would have in the same motion. Switching
//  to a slower cutoff drops the velocity and collapses the sections onto
//  the output value, because a slow filter inheriting a fast filter's
//  velocity would coast far past its target. That leaves a slope
//  discontinuity only when a key is released in the middle of a fast decay
//  into a much slower release, the same one a one-pole envelope has there.
//
//  Gate-on while the envelope is still moving restarts the attack from the
//  current value and slope, like an analog envelope; the output never jumps.
//
//  Created by hans anderson on 10/9/26.
//  Anyone may use this file without restrictions.
//

#ifndef BMADSR_h
#define BMADSR_h

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Number of second-order sections in the chain. Three was found by listening
 * tests on BMEnvelopeFollower to be the fewest that do not click on a fast
 * attack; more added nothing audible. */
#define BMADSR_SECTIONS 3

typedef enum BMADSRStage {
    BMADSR_IDLE = 0,
    BMADSR_ATTACK,
    BMADSR_DECAY,
    BMADSR_SUSTAIN,     /* decay finished; the filter keeps running at the decay cutoff */
    BMADSR_RELEASE
} BMADSRStage;

typedef struct BMADSR {
    float sampleRate;
    float sustain;
    float value;                        /* the last output sample */
    BMADSRStage stage;
    double target;                      /* the level the filter is approaching */
    double gAttack, gDecay, gRelease;   /* integrator gains for the three times */
    double g, a1, a2, a3;               /* coefficients of the cutoff in use */
    double ic1[BMADSR_SECTIONS];        /* first integrator of each section: the velocity */
    double ic2[BMADSR_SECTIONS];        /* second integrator of each section: the level */
} BMADSR;

/*!
 *BMADSR_init
 *
 * @param This        pointer to an uninitialised struct
 * @param sampleRate  rate at which the envelope will be processed
 */
void BMADSR_init(BMADSR *This, float sampleRate);

/*!
 *BMADSR_setAttack
 * @param seconds  time from 0 to 90 % of full level
 */
void BMADSR_setAttack(BMADSR *This, float seconds);

/*!
 *BMADSR_setDecay
 * @param seconds  time to move 60 dB toward the sustain level
 */
void BMADSR_setDecay(BMADSR *This, float seconds);

/*!
 *BMADSR_setSustain
 * @param level  sustain level in [0,1]
 */
void BMADSR_setSustain(BMADSR *This, float level);

/*!
 *BMADSR_setRelease
 * @param seconds  time from gate-off to -60 dB
 */
void BMADSR_setRelease(BMADSR *This, float seconds);

/*!
 *BMADSR_gateOn
 * @abstract start the attack from the current value and slope
 */
void BMADSR_gateOn(BMADSR *This);

/*!
 *BMADSR_gateOff
 * @abstract start the release from the current value
 */
void BMADSR_gateOff(BMADSR *This);

/*!
 *BMADSR_isIdle
 * @returns true when the envelope is off (never gated, or released below -100 dB)
 */
bool BMADSR_isIdle(const BMADSR *This);

/*!
 *BMADSR_processBuffer
 *
 * @param output      envelope values in [0,1], length numSamples
 * @param numSamples  number of samples to generate
 */
void BMADSR_processBuffer(BMADSR *This, float *output, size_t numSamples);

#ifdef __cplusplus
}
#endif

#endif /* BMADSR_h */
