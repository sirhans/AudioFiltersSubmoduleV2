// Released to the public domain. Use, distribute and modify without restrictions.
//
//  BMOptimizedReverb.h
//
//  Stereo, wet-only feedback delay network: four delays per unit (default: nine),
//  with a normalized Hadamard feedback mix and per-delay RT60 decay gains.
//  Unit count, sample rate and memory capacity are fixed at initialization.
//  Design notes and build commands: BMOptimizedReverb.md in the project root.
//
//  All memory is allocated by init. Setters publish pending values without
//  touching the active network. Process applies them at a buffer boundary:
//  RT60 changes retain the tail; delay-time changes redraw and clear it.
//  Complete configurations also clear the tail and use a separate, single-
//  producer mailbox. Optional offline bass analysis is in BMOptimizedReverbBass.h.
//  Changes can click. Smooth transitions, if wanted, belong to the caller.
//
//  One thread calls process. The RT60/delay setters may run concurrently; the latest request
//  for each setting wins. A min/max delay pair is always applied together;
//  separate setter calls are not a transaction. Init, free, clear and the test
//  helper require exclusive access. Treat all struct fields as private.
//
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
//

#ifndef BMOptimizedReverb_h
#define BMOptimizedReverb_h

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BMOR_DEFAULT_NUMDELAYUNITS 9
#define BMOR_DEFAULT_RT60 1.2f
#define BMOR_DEFAULT_MINDELAY 0.007f               // seconds
#define BMOR_DEFAULT_MAXDELAY 0.100f               // seconds
#define BMOR_DEFAULT_DELAY_SEED 0x9E3779B9u
// the longest stretch of samples processed in one pass through the network
#define BMOR_MAX_BLOCK 256
#define BMOR_NUM_SIGN_PATTERNS 16

// A complete request. Pattern zero reproduces the default output signs.
// The remaining patterns change signs only; delay lengths and routing stay fixed.
typedef struct BMOptimizedReverbConfiguration {
	float minDelay_seconds, maxDelay_seconds, rt60;
	uint32_t delaySeed;                   // nonzero; normally BMOR_DEFAULT_DELAY_SEED
	uint32_t signPattern;                 // [0, BMOR_NUM_SIGN_PATTERNS)
} BMOptimizedReverbConfiguration;

// Cached routing and coefficients for one four-delay Hadamard mix.
typedef struct BMOptimizedReverbGroup {
	size_t readDelay[4], writeDelay[4];
	float sign[4], gain[4];
	float previousMix[4];                 // last sample's mix, written on the next sample
} BMOptimizedReverbGroup;

typedef struct BMOptimizedReverb {
	size_t numDelayUnits, numDelays;        // immutable after init; numDelays = 4 * numDelayUnits
	float sampleRate, maxDelayCapacity_seconds;  // immutable after init
	float rt60, minDelay_seconds, maxDelay_seconds;
	float inputAttenuation;
	uint32_t delaySeed;                  // deterministic delay-layout seed

	// one delay line per delay, all in one allocation: line d is at
	// lines + lineStart[d], lineLength[d] samples long, and pos[d] is where
	// its oldest sample is, which is the next to be read and then overwritten
	float *lines;
	size_t lineCapacity;                  // samples allocated to each line
	// One allocation, partitioned into five arrays of numDelays indices.
	size_t *lineStart, *lineLength, *pos, *lengthScratch, *orderScratch;
	size_t minLineLength;

	float *decayGain;      // per delay, from the RT60 and its length
	float *outputSign;     // +1 or -1; even delays feed the left output, odd ones the right
	BMOptimizedReverbGroup *group;        // numDelayUnits groups
	float **linePointers;                 // current contiguous span of each delay
	size_t untilWrap;                     // samples until the first line reaches its end: a block ends there

	// scratch: four rows of BMOR_MAX_BLOCK samples (the first group's delay outputs), and the scaled input
	float *firstGroupScratch, *inL, *inR;

	// Atomic request payloads, accessed only by the implementation. Zero = none.
	uint32_t pendingRT60Bits;
	uint64_t pendingDelayTimeBits;

	// Three-slot mailbox for complete configurations: one slot belongs to each
	// thread, and one is exchanged atomically. Neither thread waits for the other.
	BMOptimizedReverbConfiguration configurations[3];
	uint32_t configurationWriter, configurationReader, configurationMiddle;
} BMOptimizedReverb;


/** Initialize at a positive finite sample rate, with capacity in seconds.
 * Capacity must be finite and >= BMOR_DEFAULT_MAXDELAY. The default delay
 * range must contain at least 4 * numDelayUnits sample positions, with min >= 2.
 * Invalid arguments assert in debug builds and return false in release.
 * Allocation failure returns false. After false, only free or init is valid.
 */
bool BMOptimizedReverb_init(BMOptimizedReverb *This, float sampleRate, float maxDelayCapacity_seconds);

/** Same initialization with a caller-selected positive unit count.
 * Each unit adds four delays. Both odd and even counts are supported.
 * BMOptimizedReverb_init uses BMOR_DEFAULT_NUMDELAYUNITS (nine).
 */
bool BMOptimizedReverb_initWithNumDelayUnits(BMOptimizedReverb *This, float sampleRate,
                                           float maxDelayCapacity_seconds, size_t numDelayUnits);
void BMOptimizedReverb_free(BMOptimizedReverb *This);

/** Produce wet stereo output for any number of samples.
 * Corresponding input/output buffers may be identical; inputL == inputR is
 * also supported. Output buffers must be distinct and must not partially
 * overlap inputs or each other. A zero-length call applies pending settings.
 */
void BMOptimizedReverb_process(BMOptimizedReverb *This,
                              const float *inputL, const float *inputR,
                              float *outputL, float *outputR, size_t numSamples);

/** Request a positive finite RT60, in seconds to decay by 60 dB.
 * Applied at the next process boundary, without clearing the tail.
 * Invalid setter arguments assert in debug builds and return false in release.
 */
bool BMOptimizedReverb_setRT60DecayTime(BMOptimizedReverb *This, float rt60);

/** Request finite delay limits in seconds: 0 < min < max / 2, max <= capacity.
 * Applied at the next process boundary, clearing the tail. Draws are
 * deterministic for the same times, seed and sample rate. After truncating to
 * integer samples, require min >= 2 and max - min + 1 >= 4 * numDelayUnits. Invalid requests
 * assert in debug builds and return false in release; limits are never widened.
 */
bool BMOptimizedReverb_setDelayTimes(BMOptimizedReverb *This, float minDelay_seconds, float maxDelay_seconds);

/** Check a complete request without changing active or pending state. */
bool BMOptimizedReverb_configurationIsValid(const BMOptimizedReverb *This,
                                           const BMOptimizedReverbConfiguration *configuration);

/** Publish a complete configuration, including its output-sign pattern.
 * Call from ONE control thread; process may run concurrently. Latest request
 * wins. The audio thread installs the whole request at a buffer boundary and
 * clears the tail. Invalid requests return false without changing anything.
 * Use this API instead of the separate RT60/delay setters for measured bass
 * configurations: either separate setter can invalidate a previous measurement.
 * If both APIs have pending requests, the complete configuration takes priority.
 */
bool BMOptimizedReverb_setConfiguration(BMOptimizedReverb *This,
                                      const BMOptimizedReverbConfiguration *configuration);

/** Silence delay storage and pending feedback. Requires exclusive access.
 * Pending parameter requests remain pending.
 */
void BMOptimizedReverb_clear(BMOptimizedReverb *This);

/** Test helper: install BMReverb buffer lengths and output signs in its order.
 * Supply numDelays lengths in [2, lineCapacity + 1] and signs (+1 or -1).
 * Requires exclusive access. Discards pending complete configurations and delay times, applies pending
 * RT60, and clears the tail. Each internal line is bufferLength - 1 samples.
 */
void BMOptimizedReverb_setDelayLengthsForTest(BMOptimizedReverb *This, const size_t *lengths, const float *signs);

#ifdef __cplusplus
}
#endif

#endif /* BMOptimizedReverb_h */
