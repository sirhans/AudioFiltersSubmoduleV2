//
//  BMOptimizedReverb.h
//
//  BMReverb (a feedback delay network reverb), cut down to what the synth's
//  late reflections use and then made fast. What is left of BMReverb:
//
//    - the network: 9 delay units of four delays each (36 delays), the
//      block-circulant feedback mix around a ring of 9 groups of four delays
//      (BMBlockCirculantMixUnits), the randomised delay lengths between a
//      shortest and a longest delay, the randomised output tap signs, a
//      decay gain per delay from the RT60
//    - three settings: the RT60 decay time, the shortest and the longest delay
//
//  What is gone: the high and low frequency decay shelves in the delay lines,
//  the wet-output tone filter (highpass, lowpass, mid scoop), the wet/dry
//  mixer (the output is the wet signal alone), the stereo width control (it
//  was at 1, which changes nothing), the slow-decay (hold pedal) mode, the
//  choice of the number of delay units and of the sample rate after init.
//
//  The output is BMReverb's, sample for sample, for the same delay lengths
//  (BMReverb run with 9 units, fully wet, its tone filter bypassed, width 1,
//  both decay multipliers at 1): reverb_bench.c, in the Midi Tuning Synth
//  repository this was written for, checks that, and checks the block
//  processing here against the same network run a sample at a time.
//
//  Memory: everything is allocated by init, for delays of up to
//  maxDelayCapacity_seconds. Nothing is allocated afterwards: a change of the
//  delay times re-draws the delay lengths inside the memory there is.
//
//  Threads: serialise processing and setters on an instance. The RT60 takes
//  effect at once. New delay times are picked up at the start of the next
//  process call, which re-draws the delay lengths and clears the delay lines
//  (the tail is cut off). The pending flag is not thread synchronisation.
//
//  Released to the public domain. This file may be used, distributed and
//  modified freely by anyone, for any purpose, without restrictions.
//

#ifndef BMOptimizedReverb_h
#define BMOptimizedReverb_h

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BMOR_NUMDELAYUNITS 9                       // fixed
#define BMOR_NUMDELAYS (4 * BMOR_NUMDELAYUNITS)    // 36
#define BMOR_DEFAULT_RT60 1.2f
#define BMOR_DEFAULT_MINDELAY 0.007f               // seconds
#define BMOR_DEFAULT_MAXDELAY 0.100f               // seconds
// the longest stretch of samples processed in one pass through the network
#define BMOR_MAX_BLOCK 256

// A group of four delays, the four that the feedback mix combines: which delays it reads (from) and
// which it writes its mixed signals into (to: the delays one further on), the output signs of the
// first and the decay gains of the second, and the mixed signals of the last sample processed,
// which are the next to be written.
typedef struct BMOptimizedReverbGroup {
	size_t from[4], to[4];
	float sign[4], gain[4], feedback[4];
} BMOptimizedReverbGroup;

typedef struct BMOptimizedReverb {
	float sampleRate;
	float rt60, minDelay_seconds, maxDelay_seconds;
	float inputAttenuation;

	// one delay line per delay, all in one allocation: line d is at
	// lines + lineStart[d], lineLength[d] samples long, and pos[d] is where
	// its oldest sample is, which is the next to be read and then overwritten
	float *lines;
	size_t lineCapacity;                  // samples allocated to each line
	size_t lineStart[BMOR_NUMDELAYS];
	size_t lineLength[BMOR_NUMDELAYS];
	size_t pos[BMOR_NUMDELAYS];
	size_t minLineLength;

	float decayGain[BMOR_NUMDELAYS];      // per delay, from the RT60 and its length
	float outputSign[BMOR_NUMDELAYS];     // +1 or -1; even delays feed the left output, odd ones the right
	BMOptimizedReverbGroup group[BMOR_NUMDELAYUNITS];
	size_t untilWrap;                     // samples until the first line reaches its end: a block ends there

	// scratch: four rows of BMOR_MAX_BLOCK samples (the first group's delay outputs), and the scaled input
	float *rows, *inL, *inR;

	// set by BMOptimizedReverb_setDelayTimes, taken by the audio thread
	volatile bool delayTimesChanged;
} BMOptimizedReverb;


/*!
 *BMOptimizedReverb_init
 *
 * @param maxDelayCapacity_seconds the longest delay the delay times may ever be set to; memory is allocated for it
 */
void BMOptimizedReverb_init(BMOptimizedReverb *This, float sampleRate, float maxDelayCapacity_seconds);
void BMOptimizedReverb_free(BMOptimizedReverb *This);

/*!
 *BMOptimizedReverb_process
 *
 * @abstract the wet signal alone. In place is fine, and so is inputL == inputR for a mono input. Any number of samples.
 */
void BMOptimizedReverb_process(BMOptimizedReverb *This,
							   const float *inputL, const float *inputR,
							   float *outputL, float *outputR,
							   size_t numSamples);

/*!
 *BMOptimizedReverb_setRT60DecayTime
 *
 * @param rt60 seconds for the reverberation to fall 60 dB. Takes effect at once.
 */
void BMOptimizedReverb_setRT60DecayTime(BMOptimizedReverb *This, float rt60);

/*!
 *BMOptimizedReverb_setDelayTimes
 *
 * @abstract the shortest and the longest delay in the network, seconds (BMReverbSetRoomSize). The longest must be more than twice the shortest, and no more than the capacity given to init. The delay lengths are drawn anew between the two, always the same for the same two times and sample rate, and the delay lines are cleared.
 */
void BMOptimizedReverb_setDelayTimes(BMOptimizedReverb *This, float minDelay_seconds, float maxDelay_seconds);

/*!
 *BMOptimizedReverb_clear
 *
 * @abstract silence the delay lines. Not while another thread is processing.
 */
void BMOptimizedReverb_clear(BMOptimizedReverb *This);

/*!
 *BMOptimizedReverb_setDelayLengthsForTest
 *
 * @abstract give the network these delay lengths (BMReverb's bufferLengths, in its order) and these output signs instead of its own, to compare the two reverbs sample for sample. Not while another thread is processing.
 */
void BMOptimizedReverb_setDelayLengthsForTest(BMOptimizedReverb *This, const size_t *lengths, const float *signs);

#ifdef __cplusplus
}
#endif

#endif /* BMOptimizedReverb_h */
