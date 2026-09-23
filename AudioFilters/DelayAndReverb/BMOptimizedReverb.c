//
//  BMOptimizedReverb.c
//
//  See BMOptimizedReverb.h.
//
//  How it is made fast. BMReverb takes one sample at a time through the whole
//  network: 36 scattered writes into the delay lines, 36 index increments and
//  a wrap check, a gather of 36 reads (a vDSP call per sample), the mix, a
//  rotation by memmove. Every one of those steps is short, and none of it
//  vectorises along time.
//
//  Here the network is processed in blocks of up to the shortest delay's
//  length. Within such a block nothing that is read from a delay line was
//  written in the same block, so the work can be done along time, on
//  stretches of contiguous samples, which the compiler vectorises.
//
//  The delays are taken a group of four at a time (the four that the
//  feedback mix combines), everything that concerns the group in one pass
//  over its samples (bmor_group): its share of the output, the mix, and the
//  writing of the mixed signals, with the input and the decay gain, into the
//  lines of the four delays they feed. The rotation by one delay that joins
//  the groups into a ring is a matter of which lines those are, and costs
//  nothing. A block also ends where the first delay line wraps, so that the
//  lines are read and written where they are, with no copying. Every sample
//  of every line is read once and written once per sample of audio, which
//  is the least there can be.
//
//  Measured on an M1 Pro, 2026-09-21 (reverb_bench.c, see the header), per sample of stereo
//  audio: BMReverb as the synth ran it 77 ns; the same with everything
//  stripped but still a sample at a time, no less; in blocks with separate
//  passes for the output, the mix and the writing 11.5 ns; with the passes
//  fused as above 7.7 ns (8.5 ns in the synth's buffers of 64). Where
//  BMReverb's time went, by the time profiler: the index increments and wrap
//  check 16 %, the vDSP gather 16 %, the mix with its memmove rotation 13 %,
//  the decay shelves 18 %, the rest (input, scattered writes, output sum)
//  37 %.
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

#define BMOR_U BMOR_NUMDELAYUNITS
#define BMOR_N BMOR_NUMDELAYS

static inline size_t bmor_min(size_t a, size_t b){ return a < b ? a : b; }


/* ------------------------------------------------------------------------
   Random numbers of its own: the same delay lengths every time for the same
   delay times, and the process-wide rand() is left alone.
   ------------------------------------------------------------------------ */

static uint32_t bmor_random(uint32_t *state){
	// xorshift32
	uint32_t x = *state;
	x ^= x << 13; x ^= x >> 17; x ^= x << 5;
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
		size_t t = order[i-1]; order[i-1] = order[j]; order[j] = t;
	}
}

static int bmor_compare_size_t(const void *a, const void *b){
	size_t x = *(const size_t*)a, y = *(const size_t*)b;
	return x < y ? -1 : x > y;
}

static bool bmor_contains(const size_t *list, size_t length, size_t value){
	for(size_t i = 0; i < length; i++) if(list[i] == value) return true;
	return false;
}


/*
 * BMReverbRandomsInRange: `length` different lengths in [min, max], the first
 * at min and the last at max, their mean at the middle of the range. The
 * rest start at the middle and are moved apart in pairs, one up and one
 * down by the same amount, which keeps the mean.
 */
static void bmor_lengthsInRange(size_t min, size_t max, size_t *out, size_t length, uint32_t *state){
	assert(max > min && max - min >= 2 * length);
	out[0] = min;
	out[length-1] = max;
	size_t *inner = out + 1;
	size_t innerLength = length - 2;
	size_t innerMin = min + 1, innerMax = max - 1;
	size_t middle = (innerMin + innerMax) / 2;
	for(size_t i = 0; i < innerLength; i++) inner[i] = middle;

	int64_t spread = (int64_t)(innerMax - innerMin);
	for(size_t i = 0; i < innerLength; i++){
		// a shift that keeps both of the pair in range and every length different; give up on
		// this one after a while (a range as wide as the assert asks for never gets there)
		for(int attempt = 0; attempt < 10000; attempt++){
			size_t j = bmor_randomBelow(state, innerLength);
			int64_t shift = (int64_t)bmor_randomBelow(state, (size_t)(2 * spread + 1)) - spread;
			if(j == i || shift == 0) continue;
			int64_t a = (int64_t)inner[i] + shift, b = (int64_t)inner[j] - shift;
			if(a < (int64_t)innerMin || a > (int64_t)innerMax || b < (int64_t)innerMin || b > (int64_t)innerMax) continue;
			if(a == b || bmor_contains(inner, innerLength, (size_t)a) || bmor_contains(inner, innerLength, (size_t)b)) continue;
			inner[i] = (size_t)a;
			inner[j] = (size_t)b;
			break;
		}
	}
	// the middle itself may be left several times over if a shift was given up: spread what is left
	qsort(out, length, sizeof(size_t), bmor_compare_size_t);
	for(size_t i = 1; i < length; i++) if(out[i] <= out[i-1]) out[i] = out[i-1] + 1;
}


/* ------------------------------------------------------------------------
   Setup
   ------------------------------------------------------------------------ */

static void bmor_updateDecayGains(BMOptimizedReverb *This){
	for(size_t d = 0; d < BMOR_N; d++){
		// BMReverbDelayGainFromRT60, for the delay's time: a line of n samples is a delay of n + 1
		// (the mixed signal is written one sample after it was read), which is BMReverb's bufferLength
		double delayTime = (double)(This->lineLength[d] + 1) / (double)This->sampleRate;
		This->decayGain[d] = (float)pow(10.0, -3.0 * delayTime / (double)This->rt60);
	}
	// each group's copy: the gains of the delays it writes into
	for(size_t i = 0; i < BMOR_U; i++)
		for(size_t q = 0; q < 4; q++) This->group[i].gain[q] = This->decayGain[This->group[i].to[q]];
}

static void bmor_useLineLengths(BMOptimizedReverb *This){
	// group i is delay i of each quarter of the delays, and writes into the delays one further on
	for(size_t i = 0; i < BMOR_U; i++){
		for(size_t q = 0; q < 4; q++){
			This->group[i].from[q] = q * BMOR_U + i;
			This->group[i].to[q] = (q * BMOR_U + i + 1) % BMOR_N;
			This->group[i].sign[q] = This->outputSign[q * BMOR_U + i];
		}
	}
	This->minLineLength = This->lineLength[0];
	This->untilWrap = This->lineLength[0];
	for(size_t d = 0; d < BMOR_N; d++){
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
	if(max > This->lineCapacity + 1) max = This->lineCapacity + 1;
	if(min < 2) min = 2;
	if(max < min + 2 * BMOR_N) max = min + 2 * BMOR_N;

	size_t lengths[BMOR_N];
	uint32_t state = 0x9E3779B9u;
	bmor_lengthsInRange(min, max, lengths, BMOR_N, &state);

	// Sorted, the even ones and the odd ones have nearly the same mean: the left and the right
	// channel's delays. Each channel's are put in a random order, as BMReverb does. These are
	// BMReverb's buffer lengths; the lines here are one sample shorter for the same delay.
	// The output signs: as many + as - in each channel, in a random order too.
	for(size_t channel = 0; channel < 2; channel++){
		size_t lengthOrder[BMOR_N/2], signOrder[BMOR_N/2];
		bmor_randomOrder(lengthOrder, BMOR_N/2, &state);
		bmor_randomOrder(signOrder, BMOR_N/2, &state);
		for(size_t i = 0; i < BMOR_N/2; i++){
			This->lineLength[2*i + channel] = lengths[2*lengthOrder[i] + channel] - 1;
			This->outputSign[2*i + channel] = signOrder[i] < BMOR_N/4 ? 1.0f : -1.0f;
		}
	}
	
	bmor_useLineLengths(This);
}


void BMOptimizedReverb_init(BMOptimizedReverb *This, float sampleRate, float maxDelayCapacity_seconds){
	memset(This, 0, sizeof *This);
	This->sampleRate = sampleRate;
	This->rt60 = BMOR_DEFAULT_RT60;
	This->minDelay_seconds = BMOR_DEFAULT_MINDELAY;
	This->maxDelay_seconds = BMOR_DEFAULT_MAXDELAY;
	if(maxDelayCapacity_seconds < This->maxDelay_seconds) maxDelayCapacity_seconds = This->maxDelay_seconds;

	// the input goes into every delay of its channel
	This->inputAttenuation = 1.0f / sqrtf((float)(BMOR_N / 2));

	This->lineCapacity = (size_t)ceil((double)maxDelayCapacity_seconds * (double)sampleRate) + 2 * BMOR_N;
	This->lines = calloc(BMOR_N * This->lineCapacity, sizeof(float));
	This->rows  = calloc(4 * BMOR_MAX_BLOCK, sizeof(float));
	This->inL   = calloc(BMOR_MAX_BLOCK, sizeof(float));
	This->inR   = calloc(BMOR_MAX_BLOCK, sizeof(float));

	bmor_drawDelays(This);
}


void BMOptimizedReverb_free(BMOptimizedReverb *This){
	free(This->lines); free(This->rows); free(This->inL); free(This->inR);
	This->lines = This->rows = This->inL = This->inR = NULL;
}


void BMOptimizedReverb_clear(BMOptimizedReverb *This){
	for(size_t d = 0; d < BMOR_N; d++)
		memset(This->lines + This->lineStart[d], 0, This->lineLength[d] * sizeof(float));
	for(size_t i = 0; i < BMOR_U; i++) memset(This->group[i].feedback, 0, sizeof This->group[i].feedback);
}


void BMOptimizedReverb_setRT60DecayTime(BMOptimizedReverb *This, float rt60){
	assert(rt60 > 0.0f);
	This->rt60 = rt60;
	bmor_updateDecayGains(This);
}


void BMOptimizedReverb_setDelayTimes(BMOptimizedReverb *This, float minDelay_seconds, float maxDelay_seconds){
	assert(minDelay_seconds > 0.0f);
	assert(maxDelay_seconds > 2.0f * minDelay_seconds);
	This->minDelay_seconds = minDelay_seconds;
	This->maxDelay_seconds = maxDelay_seconds;
	This->delayTimesChanged = true;
}


void BMOptimizedReverb_setDelayLengthsForTest(BMOptimizedReverb *This, const size_t *lengths, const float *signs){
	for(size_t d = 0; d < BMOR_N; d++){
		This->lineLength[d] = lengths[d] - 1;
		This->outputSign[d] = signs[d];
	}
	This->delayTimesChanged = false;
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
 * delays. The delays alternate left, right, and there are an odd number of
 * delays to a quarter, so r[0] and r[2] are of one channel (outA) and r[1]
 * and r[3] of the other (outB).
 *
 * The mixed signals go to the delays one further on (the rotation that joins
 * the groups into a ring): w[0..3] are those delays' lines, in[0..3] their
 * channels' inputs, g[0..3] their decay gains. What is written at sample k
 * is (input k + mixed k - 1) * gain: fb[0..3] are the mixed signals of the
 * sample before the block, and are left as those of the block's last sample.
 */
static inline void bmor_group(const float * restrict r0, const float * restrict r1,
							  const float * restrict r2, const float * restrict r3,
							  const float *sign,
							  float * restrict outA, float * restrict outB,
							  float * restrict w0, float * restrict w1,
							  float * restrict w2, float * restrict w3,
							  const float * restrict in0, const float * restrict in1,
							  const float * restrict in2, const float * restrict in3,
							  const float *g, float *fb, size_t n){
	const float sa = sign[0], sb = sign[1], sc = sign[2], sd = sign[3];
	const float g0 = g[0], g1 = g[1], g2 = g[2], g3 = g[3];
	
	w0[0] = (in0[0] + fb[0]) * g0;
	w1[0] = (in1[0] + fb[1]) * g1;
	w2[0] = (in2[0] + fb[2]) * g2;
	w3[0] = (in3[0] + fb[3]) * g3;
	
	for(size_t k = 0; k + 1 < n; k++){
		float a = r0[k], b = r1[k], c = r2[k], d = r3[k];
		outA[k] += sa * a + sc * c;
		outB[k] += sb * b + sd * d;
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
	outB[k] += sb * b + sd * d;
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
	for(size_t k = 0; k < n; k++){ inL[k] = attenuation * inputL[k]; inR[k] = attenuation * inputR[k]; }
	memset(outputL, 0, n * sizeof(float));
	memset(outputR, 0, n * sizeof(float));
	
	float *line[BMOR_N];
	for(size_t d = 0; d < BMOR_N; d++) line[d] = This->lines + This->lineStart[d] + This->pos[d];
	float * const out[2] = { outputL, outputR };
	const float * const in[2] = { inL, inR };
	
	// A group writes into the lines of the group after it, where that group's delay outputs are: so
	// the groups are taken from the last to the first, each after the one it writes into has been
	// read. The last group writes into the first's, around the ring: the first group's delay
	// outputs are set aside before.
	const float *first[4];
	for(size_t q = 0; q < 4; q++){
		float *copy = This->rows + q * BMOR_MAX_BLOCK;
		memcpy(copy, line[q * BMOR_U], n * sizeof(float));
		first[q] = copy;
	}
	
	for(size_t i = BMOR_U; i-- > 0; ){
		// which delays the group reads and writes, their signs and gains: worked out when the delays were set up
		const BMOptimizedReverbGroup *gr = &This->group[i];
		const float **r = i == 0 ? first : (const float *[4]){ line[gr->from[0]], line[gr->from[1]], line[gr->from[2]], line[gr->from[3]] };
		bmor_group(r[0], r[1], r[2], r[3], gr->sign, out[i & 1], out[(i + 1) & 1],
				   line[gr->to[0]], line[gr->to[1]], line[gr->to[2]], line[gr->to[3]],
				   in[gr->to[0] & 1], in[gr->to[1] & 1], in[gr->to[2] & 1], in[gr->to[3] & 1],
				   gr->gain, This->group[i].feedback, n);
	}
	
	// step the lines on; one that has reached its end starts again, and the block after this one
	// ends where the next does
	size_t untilWrap = (size_t)-1;
	for(size_t d = 0; d < BMOR_N; d++){
		size_t p = This->pos[d] + n;
		This->pos[d] = p = (p == This->lineLength[d] ? 0 : p);
		untilWrap = bmor_min(untilWrap, This->lineLength[d] - p);
	}
	This->untilWrap = untilWrap;
}


void BMOptimizedReverb_process(BMOptimizedReverb *This,
							   const float *inputL, const float *inputR,
							   float *outputL, float *outputR,
							   size_t numSamples){
	if(This->delayTimesChanged){
		This->delayTimesChanged = false;
		bmor_drawDelays(This);
	}

	// A block is no longer than the shortest delay line: nothing read in it was written in it. And
	// it ends where the first line wraps, so that inside a block every line is one stretch of memory.
	const size_t blockLength = bmor_min(BMOR_MAX_BLOCK, This->minLineLength);
	while(numSamples > 0){
		size_t n = bmor_min(bmor_min(numSamples, blockLength), This->untilWrap);
		bmor_processBlock(This, inputL, inputR, outputL, outputR, n);
		inputL += n; inputR += n; outputL += n; outputR += n;
		numSamples -= n;
	}
}
