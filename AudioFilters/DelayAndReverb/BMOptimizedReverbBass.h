// Released to the public domain. Use, distribute and modify without restrictions.
// Optional, offline bass analysis for BMOptimizedReverb. Requires Accelerate.
// This file may be used, distributed and modified freely without restrictions.
#ifndef BMOptimizedReverbBass_h
#define BMOptimizedReverbBass_h

#include "BMOptimizedReverb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BMOptimizedReverbBassResult {
    BMOptimizedReverbConfiguration configuration;
    double minimumGainDB, maximumGainDB; // extrema over every wet gain in [0, .5]
    double worstDeviationDB;             // max(abs(minimum), abs(maximum))
    double dcLeft, dcRight;              // signed wet DC amplitudes, diagnostic only
    double candidateDeviationDB[BMOR_NUM_SIGN_PATTERNS];
    size_t impulseSamples;
} BMOptimizedReverbBassResult;

/** Choose the best of SIXTEEN balanced output-sign patterns at fixed delays.
 * Measures a mono impulse into both channels, averages L/R power uniformly
 * over FFT bins in [lowHz, highHz], and includes coherent interference with
 * dry. Mixer convention: wet gain w, dry gain sqrt(1-w*w), 0 <= w <= .5.
 * Minimize the worst absolute band-average deviation over that entire mix
 * range. Ties choose the first pattern. There is no acceptance threshold,
 * gain correction, or retry loop; every valid measurement chooses a winner.
 *
 * Run on a CONTROL/WORKER thread: allocates temporary storage and renders
 * eight impulse responses (inverse sign pairs share one FFT). The live reverb
 * is untouched; only its immutable sample rate, count and capacity are read.
 * On success, publish result.configuration with setConfiguration. A crossover,
 * delay or RT60 change requires a new measurement; results describe steady
 * state, not the transient during a parameter change.
 *
 * Returns false for invalid arguments, allocation failure or an unsupported
 * analysis size; leaves result and the live reverb unchanged. Requires
 * 0 <= lowHz < highHz <= Nyquist. Renders at least two seconds and two RT60
 * periods plus maximum delay, rounded up to a power of two, capped at 2^23
 * samples (96 MiB of FFT buffers). This is a sampled estimate of band-average
 * gain, not a bound on individual frequencies or arbitrary stereo inputs.
 */
bool BMOptimizedReverb_selectBassConfiguration(const BMOptimizedReverb *This,
    const BMOptimizedReverbConfiguration *requested, float lowHz, float highHz,
    BMOptimizedReverbBassResult *result);

#ifdef __cplusplus
}
#endif
#endif
