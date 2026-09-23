//
//  BMMultibandReverb.h
//  BMAudioFilters
//
//  Four-band stereo reverb: a fourth-order Linkwitz-Riley SVF crossover,
//  one BMOptimizedReverb and a smoothed constant-power wet/dry mix per band.
//
//  Released to the public domain. This file may be used, distributed and
//  modified freely by anyone, for any purpose, without restrictions.
//

#ifndef BMMultibandReverb_h
#define BMMultibandReverb_h

// Load platform headers before BMCrossover's legacy extern "C" block, since
// Accelerate also includes C++ templates when used by an Objective-C++ client.
#include "../AudioFilter.h"
#include "BMCrossover.h"
#include "BMOptimizedReverb.h"
#include "BMWetDryMixer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BMMULTIBANDREVERB_NUM_BANDS 4
#define BMMULTIBANDREVERB_NUM_CROSSOVERS 3

typedef struct BMMultibandReverb {
    BMCrossover4way crossover;
    BMOptimizedReverb reverb[BMMULTIBANDREVERB_NUM_BANDS];
    BMWetDryMixer mixer[BMMULTIBANDREVERB_NUM_BANDS];
    float sampleRate, maxDelayCapacity_seconds;
    float crossoverFrequencies[BMMULTIBANDREVERB_NUM_CROSSOVERS];
    float *buffer;
    float *bandL[BMMULTIBANDREVERB_NUM_BANDS];
    float *bandR[BMMULTIBANDREVERB_NUM_BANDS];
    float *wetL, *wetR;
} BMMultibandReverb;

/*!
 *BMMultibandReverb_init
 *
 * @abstract Allocate and initialise before processing. Defaults: all bands
 * dry, RT60 = 1.2 s, minimum delay = 0.007 s, maximum delay = 0.100 s;
 * crossovers at 300, 3000 and 8000 Hz. At sample rates below 20 kHz all
 * three defaults are scaled by sampleRate / 20000 to stay below Nyquist.
 *
 * @param sampleRate finite, positive sample rate in Hz
 * @param maxDelayCapacity_seconds finite maximum delay available to setters,
 * at least BMOR_DEFAULT_MAXDELAY. All audio storage is allocated here.
 *
 * Threading: serialise all calls on an instance, including setters and
 * plotting. Apply control changes between process calls on the audio thread,
 * or stop processing before changing controls from another thread. Processing
 * and setters do not allocate. Init/free are not audio-thread operations.
 * Do not copy an initialised struct; it owns its allocations.
 */
void BMMultibandReverb_init(BMMultibandReverb *This, float sampleRate,
                          float maxDelayCapacity_seconds);

void BMMultibandReverb_free(BMMultibandReverb *This);

/*!
 *BMMultibandReverb_processStereo
 *
 * @abstract Split, reverberate, mix each band against its dry signal, and sum.
 * Any buffer length, including zero. Corresponding input/output channels may
 * alias; inputL == inputR is also supported for mono-to-stereo processing.
 * Output channels must be distinct and must not partially overlap any buffer.
 * A dry band still runs its reverb so its tail is available when wet is raised.
 * At wet = 0 the signal still passes through the crossover filters.
 */
void BMMultibandReverb_processStereo(BMMultibandReverb *This,
                                   const float *inputL, const float *inputR,
                                   float *outputL, float *outputR,
                                   size_t numSamples);

/*!
 * All band indices are zero-based: 0 = bass, 1 = low mid, 2 = high mid,
 * 3 = treble. Invalid setter arguments assert in debug builds and are ignored
 * in release builds. All times are seconds, and all values must be finite.
 */

/// wet in [0, 1]; smoothed by BMWetDryMixer (wet^2 + dry^2 = 1 at rest).
void BMMultibandReverb_setWet(BMMultibandReverb *This, float wet, size_t band);

/// Positive time for this band's reverb to decay by 60 dB. Preserves its tail.
void BMMultibandReverb_setRT60DecayTime(BMMultibandReverb *This, float rt60,
                                     size_t band);

/// minDelay > 0, maxDelay > 2 * minDelay, maxDelay <= capacity given to init.
/// A change redraws the delays and clears this band's tail on its next process
/// call. Use the paired setter to change both bounds together.
void BMMultibandReverb_setDelayTimes(BMMultibandReverb *This,
                                   float minDelay_seconds, float maxDelay_seconds,
                                   size_t band);

/// Change one bound while retaining the other; the same constraints apply.
void BMMultibandReverb_setMinDelay(BMMultibandReverb *This,
                                 float minDelay_seconds, size_t band);
void BMMultibandReverb_setMaxDelay(BMMultibandReverb *This,
                                 float maxDelay_seconds, size_t band);

/// Set all three boundaries in Hz: 0 < cutoff1 < cutoff2 < cutoff3 < Nyquist.
/// Coefficients take effect on the next process call, without clearing state.
/// Large abrupt changes are not click-free; advance controls gradually if needed.
void BMMultibandReverb_setCrossoverFrequencies(BMMultibandReverb *This,
                                            float cutoff1, float cutoff2,
                                            float cutoff3);

/// Set one boundary, indexed 0 ... 2, retaining its neighbours. Same constraints.
void BMMultibandReverb_setCrossoverFrequency(BMMultibandReverb *This,
                                           float frequency, size_t crossover);

/// Write the three current crossover frequencies in Hz to a caller-owned array.
void BMMultibandReverb_getCrossoverFrequencies(const BMMultibandReverb *This,
                                            float frequencies[3]);

/*!
 *BMMultibandReverb_tfMagVectors
 *
 * @abstract Get linear amplitude responses of the four crossover bands, from
 * bass to treble, at caller-supplied frequencies in Hz in [0, sampleRate/2].
 * Convert to dB with 20 * log10(magnitude), flooring zeros for display.
 * Includes every filter in the audio path (also the extra lowpasses on lower
 * bands and the preceding highpasses on upper bands), with digital frequency
 * warping. These are crossover responses, excluding reverb and wet/dry gain.
 * Reflects the latest setter values even before processing the next buffer.
 * Input/output vectors must not overlap and must each hold length floats.
 * No allocation or audio/filter-state changes. A zero length is a no-op.
 */
void BMMultibandReverb_tfMagVectors(const BMMultibandReverb *This,
                                  const float *frequencies,
                                  float *magBand1, float *magBand2,
                                  float *magBand3, float *magBand4,
                                  size_t length);

#ifdef __cplusplus
}
#endif

#endif /* BMMultibandReverb_h */
