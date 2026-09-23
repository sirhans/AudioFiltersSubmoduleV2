// Released to the public domain. Use, distribute and modify without restrictions.
//
//  BMOptimizedReverb.c
//
//  See BMOptimizedReverb.h.
//
//  Process contiguous blocks, ending at the next delay-line wrap. Each group
//  combines output summing, a normalized Hadamard mix, and feedback writes in
//  one pass. Reverse group traversal preserves unread samples; only the first
//  group's outputs need a scratch copy. See bmor_processBlock for that order.
//
//  A delay line is read and overwritten at the same position: the sample
//  read is the oldest, and the new one takes its place. (BMReverb writes,
//  steps, then reads, which makes its delay one sample shorter than its
//  buffer; the lines here are that one sample shorter instead.)
//
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
//

#include "BMOptimizedReverb.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

static inline size_t bmor_min(size_t a, size_t b){ return a < b ? a : b; }


/* ------------------------------------------------------------------------
   Random numbers of its own: the same delay lengths every time for the same
   delay times, and the process-wide rand() is left alone.
   ------------------------------------------------------------------------ */

static uint32_t bmor_random(uint32_t *state){
	// xorshift32
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return *state = x;
}

// uniform in [0, n)
static size_t bmor_randomBelow(uint32_t *state, size_t n){
	return (size_t)(((uint64_t)bmor_random(state) * (uint64_t)n) >> 32);
}

// a random order of 0 ... length - 1
static void bmor_randomOrder(size_t *order, size_t length, uint32_t *state){
	for(size_t i = 0; i < length; i++) order[i] = i;
	for(size_t i = length; i > 1; i--){
		size_t j = bmor_randomBelow(state, i);
		size_t t = order[i-1];
		order[i-1] = order[j];
		order[j] = t;
	}
}

/*
 * Sorted, distinct lengths with endpoints min/max and mean (min + max) / 2.
 * Divide the lower half of the range into disjoint integer intervals. Pick
 * one length per interval and reflect it into the upper half. The intervals
 * guarantee uniqueness; reflection preserves the mean. No retries or sorting.
 */
static void bmor_lengthsInRange(size_t min, size_t max, size_t *out, size_t length, uint32_t *state){
	assert(length >= 4 && length % 2 == 0);
	assert(max > min && max - min >= length - 1);
	out[0] = min;
	out[length - 1] = max;

	const size_t pairs = length / 2 - 1;  // interior pairs only
	const size_t lowerSlots = (max - min - 1) / 2;  // exclude the midpoint
	for(size_t i = 0; i < pairs; i++){
		// [begin, end) contains offsets from min, strictly below the midpoint.
		size_t begin = 1 + i * lowerSlots / pairs;
		size_t end = 1 + (i + 1) * lowerSlots / pairs;
		size_t offset = begin + bmor_randomBelow(state, end - begin);
		out[i + 1] = min + offset;
		out[length - 2 - i] = max - offset;
	}
}


/* ------------------------------------------------------------------------
   Setup
   ------------------------------------------------------------------------ */

// Atomic integer payloads keep the public struct usable from C, C++ and Swift.
// Zero means no request; valid settings are positive floats. The delay pair is
// one 64-bit value so the audio thread cannot observe mismatched min/max times.
_Static_assert(sizeof(float) == sizeof(uint32_t), "Requires 32-bit floats");
_Static_assert(__atomic_always_lock_free(sizeof(uint32_t), 0), "Requires lock-free 32-bit atomics");
_Static_assert(__atomic_always_lock_free(sizeof(uint64_t), 0), "Requires lock-free 64-bit atomics");

static uint32_t bmor_floatBits(float value){
	uint32_t bits;
	memcpy(&bits, &value, sizeof bits);
	return bits;
}

static float bmor_floatFromBits(uint32_t bits){
	float value;
	memcpy(&value, &bits, sizeof value);
	return value;
}

static uint32_t bmor_takeRT60(BMOptimizedReverb *This){
	// Avoid an atomic write on the usual path, where nothing has changed.
	if(!__atomic_load_n(&This->pendingRT60Bits, __ATOMIC_RELAXED)) return 0;
	return __atomic_exchange_n(&This->pendingRT60Bits, 0, __ATOMIC_RELAXED);
}

static uint64_t bmor_takeDelayTimes(BMOptimizedReverb *This){
	if(!__atomic_load_n(&This->pendingDelayTimeBits, __ATOMIC_RELAXED)) return 0;
	return __atomic_exchange_n(&This->pendingDelayTimeBits, 0, __ATOMIC_RELAXED);
}

static void bmor_updateDecayGains(BMOptimizedReverb *This){
	for(size_t d = 0; d < This->numDelays; d++){
		// BMReverbDelayGainFromRT60, for the delay's time: a line of n samples is a delay of n + 1
		// (the mixed signal is written one sample after it was read), which is BMReverb's bufferLength
		double delayTime = (double)(This->lineLength[d] + 1) / (double)This->sampleRate;
		This->decayGain[d] = (float)pow(10.0, -3.0 * delayTime / (double)This->rt60);
	}
	// each group's copy: the gains of the delays it writes into
	for(size_t i = 0; i < This->numDelayUnits; i++)
		for(size_t q = 0; q < 4; q++) This->group[i].gain[q] = This->decayGain[This->group[i].writeDelay[q]];
}

static void bmor_useLineLengths(BMOptimizedReverb *This){
	// group i is delay i of each quarter of the delays, and writes into the delays one further on
	for(size_t i = 0; i < This->numDelayUnits; i++){
		for(size_t q = 0; q < 4; q++){
			This->group[i].readDelay[q] = q * This->numDelayUnits + i;
			This->group[i].writeDelay[q] = (q * This->numDelayUnits + i + 1) % This->numDelays;
			This->group[i].sign[q] = This->outputSign[q * This->numDelayUnits + i];
		}
	}
	This->minLineLength = This->lineLength[0];
	This->untilWrap = This->lineLength[0];
	for(size_t d = 0; d < This->numDelays; d++){
		This->untilWrap = bmor_min(This->untilWrap, This->lineLength[d]);
		assert(This->lineLength[d] >= 1 && This->lineLength[d] <= This->lineCapacity);
		This->lineStart[d] = d * This->lineCapacity;
		This->pos[d] = 0;
		This->minLineLength = bmor_min(This->minLineLength, This->lineLength[d]);
	}
	bmor_updateDecayGains(This);
	BMOptimizedReverb_clear(This);
}

// draw the delay lengths for the delay times and the output signs, and start from silence
static void bmor_drawDelays(BMOptimizedReverb *This){
	size_t min = (size_t)(This->minDelay_seconds * This->sampleRate);
	size_t max = (size_t)(This->maxDelay_seconds * This->sampleRate);
	assert(min >= 2 && max - min >= This->numDelays - 1);
	assert(max - 1 <= This->lineCapacity);

	size_t *lengths = This->lengthScratch;
	uint32_t state = This->delaySeed;
	bmor_lengthsInRange(min, max, lengths, This->numDelays, &state);

	// Sorted, the even ones and the odd ones have nearly the same mean: the left and the right
	// channel's delays. Each channel's are put in a random order, as BMReverb does. These are
	// BMReverb's buffer lengths; the lines here are one sample shorter for the same delay.
	// The output signs: as many + as - in each channel, in a random order too.
	for(size_t channel = 0; channel < 2; channel++){
		size_t *lengthOrder = This->orderScratch;
		size_t *signOrder = This->orderScratch + This->numDelays / 2;
		bmor_randomOrder(lengthOrder, This->numDelays/2, &state);
		bmor_randomOrder(signOrder, This->numDelays/2, &state);
		for(size_t i = 0; i < This->numDelays/2; i++){
			This->lineLength[2*i + channel] = lengths[2*lengthOrder[i] + channel] - 1;
			This->outputSign[2*i + channel] = signOrder[i] < This->numDelays/4 ? 1.0f : -1.0f;
		}
	}

	bmor_useLineLengths(This);
}


// Validate before float-to-integer conversion. An inclusive [min, max] range
// must contain at least one sample position per delay; never widen the range.
static bool bmor_validDelayTimes(float sampleRate, float minTime, float maxTime, float capacity, size_t numDelays){
	if(!isfinite(sampleRate) || sampleRate <= 0.0f ||
	   !isfinite(minTime) || !isfinite(maxTime) ||
	   minTime <= 0.0f || maxTime <= 2.0f * minTime || maxTime > capacity) return false;
	double maxSamples = (double)(maxTime * sampleRate);
	if(!isfinite(maxSamples) || maxSamples > (double)(SIZE_MAX / (numDelays * sizeof(float)))) return false;
	size_t min = (size_t)(minTime * sampleRate);
	size_t max = (size_t)maxSamples;
	return min >= 2 && max >= min && max - min >= numDelays - 1;
}

// Balanced arrangements, each paired with its sign inverse. Inverting
// all taps keeps wet power unchanged and reverses its interference with dry.
// The default arrangement is already in outputSign after bmor_drawDelays.
static void bmor_selectSigns(BMOptimizedReverb *This, uint32_t pattern){
	if(pattern >= 2){
		uint32_t state = This->delaySeed ^ (0x85EBCA6Bu * (pattern / 2));
		if(!state) state = 1;
		for(size_t channel = 0; channel < 2; channel++){
			bmor_randomOrder(This->orderScratch, This->numDelays / 2, &state);
			for(size_t i = 0; i < This->numDelays / 2; i++)
				This->outputSign[2*i + channel] = This->orderScratch[i] < This->numDelays / 4 ? 1.0f : -1.0f;
		}
	}
	if(pattern & 1u)
		for(size_t d = 0; d < This->numDelays; d++) This->outputSign[d] = -This->outputSign[d];
	for(size_t i = 0; i < This->numDelayUnits; i++)
		for(size_t q = 0; q < 4; q++)
			This->group[i].sign[q] = This->outputSign[This->group[i].readDelay[q]];
}

bool BMOptimizedReverb_configurationIsValid(const BMOptimizedReverb *This,
                                           const BMOptimizedReverbConfiguration *configuration){
	return configuration && configuration->delaySeed != 0 &&
		configuration->signPattern < BMOR_NUM_SIGN_PATTERNS &&
		isfinite(configuration->rt60) && configuration->rt60 > 0.0f &&
		bmor_validDelayTimes(This->sampleRate, configuration->minDelay_seconds,
			configuration->maxDelay_seconds, This->maxDelayCapacity_seconds, This->numDelays);
}

// Low two bits identify a slot; bit 2 says that it contains a new request.
enum { BMOR_CONFIGURATION_PENDING = 4, BMOR_CONFIGURATION_SLOT = 3 };

bool BMOptimizedReverb_setConfiguration(BMOptimizedReverb *This,
                                      const BMOptimizedReverbConfiguration *configuration){
	if(!BMOptimizedReverb_configurationIsValid(This, configuration)) return false;
	This->configurations[This->configurationWriter] = *configuration;
	// Release publishes our slot; acquire takes ownership of the old middle
	// slot. The reader cannot access that slot again until we publish it.
	uint32_t old = __atomic_exchange_n(&This->configurationMiddle,
		This->configurationWriter | BMOR_CONFIGURATION_PENDING, __ATOMIC_ACQ_REL);
	This->configurationWriter = old & BMOR_CONFIGURATION_SLOT;
	return true;
}

static bool bmor_takeConfiguration(BMOptimizedReverb *This, BMOptimizedReverbConfiguration *configuration){
	if(!(__atomic_load_n(&This->configurationMiddle, __ATOMIC_RELAXED) & BMOR_CONFIGURATION_PENDING)) return false;
	uint32_t old = __atomic_exchange_n(&This->configurationMiddle,
		This->configurationReader, __ATOMIC_ACQ_REL);
	This->configurationReader = old & BMOR_CONFIGURATION_SLOT;
	*configuration = This->configurations[This->configurationReader];
	return true;
}

bool BMOptimizedReverb_init(BMOptimizedReverb *This, float sampleRate, float maxDelayCapacity_seconds){
	return BMOptimizedReverb_initWithNumDelayUnits(This, sampleRate, maxDelayCapacity_seconds,
		BMOR_DEFAULT_NUMDELAYUNITS);
}

bool BMOptimizedReverb_initWithNumDelayUnits(BMOptimizedReverb *This, float sampleRate,
                                           float maxDelayCapacity_seconds, size_t numDelayUnits){
	memset(This, 0, sizeof *This);
	// Bound index-storage arithmetic before deriving the total delay count.
	bool validCount = numDelayUnits > 0 && numDelayUnits <= SIZE_MAX / (4 * 5 * sizeof(size_t));
	assert(validCount);
	if(!validCount) return false;
	size_t numDelays = 4 * numDelayUnits;
	double capacitySamples = ceil((double)maxDelayCapacity_seconds * (double)sampleRate);
	bool valid = isfinite(maxDelayCapacity_seconds) && maxDelayCapacity_seconds > 0.0f &&
		capacitySamples <= (double)(SIZE_MAX / (numDelays * sizeof(float))) &&
		bmor_validDelayTimes(sampleRate, BMOR_DEFAULT_MINDELAY, BMOR_DEFAULT_MAXDELAY,
			maxDelayCapacity_seconds, numDelays);
	assert(valid);
	if(!valid) return false;

	This->numDelayUnits = numDelayUnits;
	This->numDelays = numDelays;
	This->sampleRate = sampleRate;
	This->rt60 = BMOR_DEFAULT_RT60;
	This->delaySeed = BMOR_DEFAULT_DELAY_SEED;
	This->configurationWriter = 0;
	This->configurationReader = 1;
	This->configurationMiddle = 2;
	This->minDelay_seconds = BMOR_DEFAULT_MINDELAY;
	This->maxDelay_seconds = BMOR_DEFAULT_MAXDELAY;
	This->maxDelayCapacity_seconds = maxDelayCapacity_seconds;

	// Allocate all delay, coefficient and scratch storage once. The input goes
	// into every delay of its channel, scaled for that channel's delay count.
	This->inputAttenuation = 1.0f / sqrtf((float)(numDelays / 2));
	This->lineCapacity = (size_t)capacitySamples;
	This->lines = calloc(numDelays * This->lineCapacity, sizeof(float));
	This->lineStart = calloc(5 * numDelays, sizeof(size_t));
	This->decayGain = calloc(2 * numDelays, sizeof(float));
	This->group = calloc(numDelayUnits, sizeof(BMOptimizedReverbGroup));
	This->linePointers = calloc(numDelays, sizeof(float *));
	This->firstGroupScratch = calloc(6 * BMOR_MAX_BLOCK, sizeof(float));
	if(!This->lines || !This->lineStart || !This->decayGain || !This->group ||
	   !This->linePointers || !This->firstGroupScratch){
		BMOptimizedReverb_free(This);
		return false;
	}
	This->lineLength = This->lineStart + numDelays;
	This->pos = This->lineLength + numDelays;
	This->lengthScratch = This->pos + numDelays;
	This->orderScratch = This->lengthScratch + numDelays;
	This->outputSign = This->decayGain + numDelays;
	This->inL = This->firstGroupScratch + 4 * BMOR_MAX_BLOCK;
	This->inR = This->inL + BMOR_MAX_BLOCK;

	bmor_drawDelays(This);
	return true;
}


void BMOptimizedReverb_free(BMOptimizedReverb *This){
	free(This->lines);
	free(This->lineStart);
	free(This->decayGain);
	free(This->group);
	free(This->linePointers);
	free(This->firstGroupScratch);
	memset(This, 0, sizeof *This);
}


void BMOptimizedReverb_clear(BMOptimizedReverb *This){
	for(size_t d = 0; d < This->numDelays; d++)
		memset(This->lines + This->lineStart[d], 0, This->lineLength[d] * sizeof(float));
	for(size_t i = 0; i < This->numDelayUnits; i++) memset(This->group[i].previousMix, 0, sizeof This->group[i].previousMix);
}


bool BMOptimizedReverb_setRT60DecayTime(BMOptimizedReverb *This, float rt60){
	bool valid = isfinite(rt60) && rt60 > 0.0f;
	assert(valid);
	if(!valid) return false;
	__atomic_store_n(&This->pendingRT60Bits, bmor_floatBits(rt60), __ATOMIC_RELAXED);
	return true;
}


bool BMOptimizedReverb_setDelayTimes(BMOptimizedReverb *This, float minDelay_seconds, float maxDelay_seconds){
	bool valid = bmor_validDelayTimes(This->sampleRate, minDelay_seconds,
		maxDelay_seconds, This->maxDelayCapacity_seconds, This->numDelays);
	assert(valid);
	if(!valid) return false;
	uint64_t pair = (uint64_t)bmor_floatBits(minDelay_seconds) |
		((uint64_t)bmor_floatBits(maxDelay_seconds) << 32);
	// The payload is the entire request; no other memory is published with it.
	__atomic_store_n(&This->pendingDelayTimeBits, pair, __ATOMIC_RELAXED);
	return true;
}


void BMOptimizedReverb_setDelayLengthsForTest(BMOptimizedReverb *This, const size_t *lengths, const float *signs){
	for(size_t d = 0; d < This->numDelays; d++){
		This->lineLength[d] = lengths[d] - 1;
		This->outputSign[d] = signs[d];
	}
	// Explicit test lengths replace pending delay times, but retain pending RT60.
	(void)bmor_takeDelayTimes(This);
	BMOptimizedReverbConfiguration discarded;
	(void)bmor_takeConfiguration(This, &discarded);
	uint32_t rt60Bits = bmor_takeRT60(This);
	if(rt60Bits) This->rt60 = bmor_floatFromBits(rt60Bits);
	bmor_useLineLengths(This);
}


/* ------------------------------------------------------------------------
   Processing, a block at a time
   ------------------------------------------------------------------------ */

/*
 * One group of four delays for a whole block, everything that concerns it in
 * one pass over its samples: its part of the output, the 4x4 Hadamard mix
 * (scaled to be orthogonal), and the writing of the results into the delay
 * lines they feed.
 *
 * r[0..3] are the group's delay outputs, one delay from each quarter of the
 * delays. For an odd unit count, r[0]/r[2] feed outA and r[1]/r[3] feed
 * outB. For an even count, all four feed outA. The output pointers are
 * always distinct, including the even-count path (which does not use outB).
 *
 * The mixed signals go to the delays one further on (the rotation that joins
 * the groups into a ring): w[0..3] are those delays' lines, in[0..3] their
 * channels' inputs, g[0..3] their decay gains. What is written at sample k
 * is (input k + mixed k - 1) * gain: fb[0..3] are the mixed signals of the
 * sample before the block, and are left as those of the block's last sample.
 */
static inline __attribute__((always_inline)) void bmor_group(const float * restrict r0, const float * restrict r1,
							  const float * restrict r2, const float * restrict r3,
							  const float *sign,
							  float * restrict outA, float * restrict outB,
							  float * restrict w0, float * restrict w1,
							  float * restrict w2, float * restrict w3,
							  const float * restrict in0, const float * restrict in1,
							  const float * restrict in2, const float * restrict in3,
							  const float *g, float *fb, bool splitChannels, size_t n){
	const float sa = sign[0], sb = sign[1], sc = sign[2], sd = sign[3];
	const float g0 = g[0], g1 = g[1], g2 = g[2], g3 = g[3];

	w0[0] = (in0[0] + fb[0]) * g0;
	w1[0] = (in1[0] + fb[1]) * g1;
	w2[0] = (in2[0] + fb[2]) * g2;
	w3[0] = (in3[0] + fb[3]) * g3;

	for(size_t k = 0; k + 1 < n; k++){
		float a = r0[k], b = r1[k], c = r2[k], d = r3[k];
		outA[k] += sa * a + sc * c;
		if(splitChannels) outB[k] += sb * b + sd * d;
		else outA[k] += sb * b + sd * d;
		float t0 = a + c, t1 = b + d, t2 = a - c, t3 = b - d;
		w0[k+1] = (in0[k+1] + 0.5f * (t0 + t1)) * g0;
		w1[k+1] = (in1[k+1] + 0.5f * (t0 - t1)) * g1;
		w2[k+1] = (in2[k+1] + 0.5f * (t2 + t3)) * g2;
		w3[k+1] = (in3[k+1] + 0.5f * (t2 - t3)) * g3;
	}

	// the block's last sample: its mixed signals are for the next block to write
	size_t k = n - 1;
	float a = r0[k], b = r1[k], c = r2[k], d = r3[k];
	outA[k] += sa * a + sc * c;
	if(splitChannels) outB[k] += sb * b + sd * d;
	else outA[k] += sb * b + sd * d;
	float t0 = a + c, t1 = b + d, t2 = a - c, t3 = b - d;
	fb[0] = 0.5f * (t0 + t1);
	fb[1] = 0.5f * (t0 - t1);
	fb[2] = 0.5f * (t2 + t3);
	fb[3] = 0.5f * (t2 - t3);
}


/*
 * A block of n samples, n no more than the shortest delay line and than any
 * line has left before it wraps: the delay outputs are read where they are,
 * and the lines are written where they were read.
 */
static void bmor_processBlock(BMOptimizedReverb *This,
							  const float *inputL, const float *inputR,
							  float *outputL, float *outputR,
							  size_t n){
	// the input as it goes into the delays. (A copy, so that the output may overwrite the input.)
	const float attenuation = This->inputAttenuation;
	float * restrict inL = This->inL, * restrict inR = This->inR;
	for(size_t k = 0; k < n; k++){
		inL[k] = attenuation * inputL[k];
		inR[k] = attenuation * inputR[k];
	}
	memset(outputL, 0, n * sizeof(float));
	memset(outputR, 0, n * sizeof(float));

	float **line = This->linePointers;
	for(size_t d = 0; d < This->numDelays; d++) line[d] = This->lines + This->lineStart[d] + This->pos[d];
	float * const out[2] = { outputL, outputR };
	const float * const in[2] = { inL, inR };

	// A group writes into the lines of the group after it, where that group's delay outputs are: so
	// the groups are taken from the last to the first, each after the one it writes into has been
	// read. The last group writes into the first's, around the ring: the first group's delay
	// outputs are set aside before.
	const float *first[4];
	for(size_t q = 0; q < 4; q++){
		float *copy = This->firstGroupScratch + q * BMOR_MAX_BLOCK;
		memcpy(copy, line[q * This->numDelayUnits], n * sizeof(float));
		first[q] = copy;
	}

	for(size_t i = This->numDelayUnits; i-- > 0; ){
		// which delays the group reads and writes, their signs and gains: worked out when the delays were set up
		const BMOptimizedReverbGroup *gr = &This->group[i];
		const float *r[4];
		for(size_t q = 0; q < 4; q++)
			r[q] = i == 0 ? first[q] : line[gr->readDelay[q]];
		// Specialize the two routings outside the sample loop for vectorization.
		if(This->numDelayUnits % 2 != 0){
			bmor_group(r[0], r[1], r[2], r[3], gr->sign, out[i & 1], out[(i + 1) & 1],
					   line[gr->writeDelay[0]], line[gr->writeDelay[1]], line[gr->writeDelay[2]], line[gr->writeDelay[3]],
					   in[gr->writeDelay[0] & 1], in[gr->writeDelay[1] & 1], in[gr->writeDelay[2] & 1], in[gr->writeDelay[3] & 1],
					   gr->gain, This->group[i].previousMix, true, n);
		} else {
			bmor_group(r[0], r[1], r[2], r[3], gr->sign, out[i & 1], out[(i + 1) & 1],
					   line[gr->writeDelay[0]], line[gr->writeDelay[1]], line[gr->writeDelay[2]], line[gr->writeDelay[3]],
					   in[gr->writeDelay[0] & 1], in[gr->writeDelay[1] & 1], in[gr->writeDelay[2] & 1], in[gr->writeDelay[3] & 1],
					   gr->gain, This->group[i].previousMix, false, n);
		}
	}

	// step the lines on; one that has reached its end starts again, and the block after this one
	// ends where the next does
	size_t untilWrap = (size_t)-1;
	for(size_t d = 0; d < This->numDelays; d++){
		size_t p = This->pos[d] + n;
		if(p == This->lineLength[d]) p = 0;
		This->pos[d] = p;
		untilWrap = bmor_min(untilWrap, This->lineLength[d] - p);
	}
	This->untilWrap = untilWrap;
}


void BMOptimizedReverb_process(BMOptimizedReverb *This,
							   const float *inputL, const float *inputR,
							   float *outputL, float *outputR,
							   size_t numSamples){
	// Only this thread changes active settings, gains or delay storage. A setter
	// racing with these exchanges leaves its request for this or the next call.
	uint32_t rt60Bits = bmor_takeRT60(This);
	uint64_t delayBits = bmor_takeDelayTimes(This);
	BMOptimizedReverbConfiguration configuration;
	if(bmor_takeConfiguration(This, &configuration)){
		This->minDelay_seconds = configuration.minDelay_seconds;
		This->maxDelay_seconds = configuration.maxDelay_seconds;
		This->rt60 = configuration.rt60;
		This->delaySeed = configuration.delaySeed;
		bmor_drawDelays(This);
		bmor_selectSigns(This, configuration.signPattern);
	} else {
		if(rt60Bits) This->rt60 = bmor_floatFromBits(rt60Bits);
		if(delayBits){
			This->minDelay_seconds = bmor_floatFromBits((uint32_t)delayBits);
			This->maxDelay_seconds = bmor_floatFromBits((uint32_t)(delayBits >> 32));
			bmor_drawDelays(This);   // rebuild gains and clear the tail
		} else if(rt60Bits){
			bmor_updateDecayGains(This);   // keep the existing tail
		}
	}

	// A block is no longer than the shortest delay line: nothing read in it was written in it. And
	// it ends where the first line wraps, so that inside a block every line is one stretch of memory.
	const size_t blockLength = bmor_min(BMOR_MAX_BLOCK, This->minLineLength);
	while(numSamples > 0){
		size_t n = bmor_min(bmor_min(numSamples, blockLength), This->untilWrap);
		bmor_processBlock(This, inputL, inputR, outputL, outputR, n);
		inputL += n;
		inputR += n;
		outputL += n;
		outputR += n;
		numSamples -= n;
	}
}
