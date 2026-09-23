//
//  BMMultiLevelSVF.c
//  BMAudioFilters
//
//  Created by Nguyen Minh Tien on 1/9/19.
//

#include "BMMultiLevelSVF.h"
#include <stdlib.h>
#include <assert.h>
#include "../Constants.h"
#include <string.h>
#include "../MathUtilities/BMComplexMath.h"

//#define SVF_PARAM_COUNT 3
#define SVF_GOLDEN_RATIO (1.0 + sqrt(5.0)) / 2.0

static inline void BMMultiLevelSVF_processBufferAtLevel(BMMultiLevelSVF *This,
                                                        size_t level, size_t channel,
                                                        const float* input,
                                                        float* output,
                                                        size_t numSamples);



static inline void BMMultiLevelSVF_updateSVFParam(BMMultiLevelSVF *This);



void BMMultiLevelSVF_init(BMMultiLevelSVF *This,
						  size_t numLevels,
						  float sampleRate,
						  bool isStereo){
	
	// init the biquad helper. This allows us to set biquad filters using the
	// functions in BMMultiLevelBiquad and copy them into the SVF.
//	BMMultiLevelBiquad_init(&This->biquadHelper, numLevels, sampleRate, isStereo, false, false);
	
//	assert(oversampleFactor >= 1);
//	This->oversampleFactor = oversampleFactor;
	This->sampleRate = sampleRate;
	This->numChannels = isStereo? 2 : 1;
	This->numLevels = numLevels;
	This->numActiveLevels = numLevels;
	This->filterSweep = false;
	This->shouldUpdateParam = false;
	This->updateImmediately = false;
	This->needsClearStateVariables = false;
	//This->lock = OS_UNFAIR_LOCK_INIT;
	BMLock_init(&This->lock);
	//If stereo -> we need totalnumlevel = numlevel *2
	size_t totalNumLevels = numLevels * This->numChannels;

	This->g0 = malloc(sizeof(float) * numLevels);
	This->g1 = malloc(sizeof(float) * numLevels);
	This->g2 = malloc(sizeof(float) * numLevels);
	This->m0 = malloc(sizeof(float) * numLevels);
	This->m1 = malloc(sizeof(float) * numLevels);
	This->m2 = malloc(sizeof(float) * numLevels);
	This->k  = malloc(sizeof(float) * numLevels);
	This->gainMix = calloc(numLevels, sizeof(BMSVFGainMix));
	This->gainMix_pending = calloc(numLevels, sizeof(BMSVFGainMix));
	
	This->g0_target = malloc(sizeof(float) * numLevels);
	This->g1_target = malloc(sizeof(float) * numLevels);
	This->g2_target = malloc(sizeof(float) * numLevels);
	This->m0_target = malloc(sizeof(float) * numLevels);
	This->m1_target = malloc(sizeof(float) * numLevels);
	This->m2_target = malloc(sizeof(float) * numLevels);
	This->k_target  = malloc(sizeof(float) * numLevels);
	
	This->g0_pending = malloc(sizeof(float) * numLevels);
	This->g1_pending = malloc(sizeof(float) * numLevels);
	This->g2_pending = malloc(sizeof(float) * numLevels);
	This->m0_pending = malloc(sizeof(float) * numLevels);
	This->m1_pending = malloc(sizeof(float) * numLevels);
	This->m2_pending = malloc(sizeof(float) * numLevels);
	This->k_pending = malloc(sizeof(float) * numLevels);

	
	This->ic1eq = malloc(sizeof(float)* totalNumLevels);
	This->ic2eq = malloc(sizeof(float)* totalNumLevels);
	This->ic1eqD = calloc(totalNumLevels, sizeof(double));
	This->ic2eqD = calloc(totalNumLevels, sizeof(double));
	for(int i=0;i<totalNumLevels;i++){
		This->ic1eq[i] = 0.0;
		This->ic2eq[i] = 0.0;
	}
	
	
	This->g0_interp = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->g1_interp = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->g2_interp = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->m0_interp = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->m1_interp = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->m2_interp = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->k_interp  = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
}




void BMMultiLevelSVF_free(BMMultiLevelSVF *This){
	free(This->g0);
	free(This->g1);
	free(This->g2);
	free(This->m0);
	free(This->m1);
	free(This->m2);
	free(This->k);
	free(This->gainMix);
	free(This->gainMix_pending);
	This->gainMix = NULL;
	This->gainMix_pending = NULL;
	This->g0 = NULL;
	This->g1 = NULL;
	This->g2 = NULL;
	This->m0 = NULL;
	This->m1 = NULL;
	This->m2 = NULL;
	This->k = NULL;
	
	free(This->g0_target);
	free(This->g1_target);
	free(This->g2_target);
	free(This->m0_target);
	free(This->m1_target);
	free(This->m2_target);
	free(This->k_target);
	This->g0_target = NULL;
	This->g1_target = NULL;
	This->g2_target = NULL;
	This->m0_target = NULL;
	This->m1_target = NULL;
	This->m2_target = NULL;
	This->k_target = NULL;
	
	free(This->g0_pending);
	free(This->g1_pending);
	free(This->g2_pending);
	free(This->m0_pending);
	free(This->m1_pending);
	free(This->m2_pending);
	free(This->k_pending);
	This->g0_pending = NULL;
	This->g1_pending = NULL;
	This->g2_pending = NULL;
	This->m0_pending = NULL;
	This->m1_pending = NULL;
	This->m2_pending = NULL;
	This->k_pending = NULL;
    
    free(This->ic1eq);
    free(This->ic2eq);
    This->ic1eq = NULL;
    This->ic2eq = NULL;
    free(This->ic1eqD);
    free(This->ic2eqD);
    This->ic1eqD = NULL;
    This->ic2eqD = NULL;
    
	free(This->g0_interp);
	free(This->g1_interp);
	free(This->g2_interp);
	free(This->m0_interp);
	free(This->m1_interp);
	free(This->m2_interp);
	free(This->k_interp);
	This->g0_interp = NULL;
	This->g1_interp = NULL;
	This->g2_interp = NULL;
	This->m0_interp = NULL;
	This->m1_interp = NULL;
	This->m2_interp = NULL;
	This->k_interp = NULL;
}


void BMMultiLevelSVF_enableFilterSweep(BMMultiLevelSVF *This, bool sweepOn){
	This->filterSweep = sweepOn;
}

void BMMultiLevelSVF_forceImmediateUpdate(BMMultiLevelSVF *This){
	This->updateImmediately = true;
}

void BMMultiLevelSVF_clearStateVariables(BMMultiLevelSVF *This){
	// set all state variables to zero
	memset(This->ic1eq,0,sizeof(float)*This->numLevels*This->numChannels);
	memset(This->ic2eq,0,sizeof(float)*This->numLevels*This->numChannels);
	memset(This->ic1eqD,0,sizeof(double)*This->numLevels*This->numChannels);
	memset(This->ic2eqD,0,sizeof(double)*This->numLevels*This->numChannels);
	
	This->needsClearStateVariables = false;
}

void BMMultiLevelSVF_clearBuffers(BMMultiLevelSVF *This){
	This->needsClearStateVariables = true;
}


void BMMultiLevelSVF_stageTwoParameterUpdate(BMMultiLevelSVF *This, size_t level);


#pragma mark - static (non-sweep) fast path

/*
 * Fast path for the non-sweep case. The original code processed one level at
 * a time over the whole buffer, reading the state variables through pointers
 * into This->ic1eq / ic2eq. Because those pointers have the same type as the
 * output buffer, the compiler had to assume that writing output[i] might
 * modify the state, so it reloaded the state from memory on every sample and
 * the store-to-load round trip sat inside the filter's recurrence. On top of
 * that each level was a separate pass over the buffer, so the CPU could never
 * overlap the work of neighbouring levels.
 *
 * Here all levels of one buffer are processed in a single pass, sample by
 * sample, with the state variables in local variables (registers). The
 * per-level arithmetic is written with exactly the same expressions and the
 * same evaluation order as the original tick, so the output is bit-for-bit
 * identical to the old level-by-level code (verified for mono and stereo).
 *
 * The level loop is instantiated for each fixed level count up to
 * BM_SVF_MAX_FUSED_LEVELS so that it can be fully unrolled and the state
 * arrays turned into registers. Larger level counts use the same code with a
 * runtime loop, which is still correct, just not as fast.
 */

#define BM_SVF_MAX_FUSED_LEVELS 8

typedef struct BMSVFCoefs {
	float g0, g1, g2, m0, m1, m2, k;
} BMSVFCoefs;


// one second-order section, one channel. Same expressions as the original
// per-level loop (based on Tick 2 in the Cytomic paper).
static inline float BMMultiLevelSVF_tick(float v0, const BMSVFCoefs *c,
										 float *ic1eq, float *ic2eq){
	float t0 = v0 - *ic2eq;
	float t1 = (c->g0 * t0) + (c->g1 * *ic1eq);
	float t2 = (c->g2 * t0) + (c->g0 * *ic1eq);
	float v1 = t1 + *ic1eq;
	float v2 = t2 + *ic2eq;
	float high = v0 - (c->k * v1) - v2;
	float band = v1;
	float low = v2;
	float out = (c->m0 * high) + (c->m1 * band) + (c->m2 * low);
	*ic1eq += 2.0f * t1;
	*ic2eq += 2.0f * t2;
	return out;
}


// same tick for two channels at once (left in lane 0, right in lane 1). The
// coefficients are shared between channels; the states are per channel.
static inline simd_float2 BMMultiLevelSVF_tick2(simd_float2 v0, const BMSVFCoefs *c,
												simd_float2 *ic1eq, simd_float2 *ic2eq){
	simd_float2 t0 = v0 - *ic2eq;
	simd_float2 t1 = (c->g0 * t0) + (c->g1 * *ic1eq);
	simd_float2 t2 = (c->g2 * t0) + (c->g0 * *ic1eq);
	simd_float2 v1 = t1 + *ic1eq;
	simd_float2 v2 = t2 + *ic2eq;
	simd_float2 high = v0 - (c->k * v1) - v2;
	simd_float2 band = v1;
	simd_float2 low = v2;
	simd_float2 out = (c->m0 * high) + (c->m1 * band) + (c->m2 * low);
	*ic1eq += 2.0f * t1;
	*ic2eq += 2.0f * t2;
	return out;
}


// all levels, one channel, one pass. L must be a compile-time constant at the
// call site for the level loops to unroll.
static inline void BMMultiLevelSVF_staticMonoL(const BMSVFCoefs *c,
											   float *s1, float *s2,
											   const float *input, float *output,
											   size_t numSamples, const size_t L){
	float ic1[BM_SVF_MAX_FUSED_LEVELS], ic2[BM_SVF_MAX_FUSED_LEVELS];
#pragma clang loop unroll(full)
	for(size_t l=0; l<L; l++){ ic1[l] = s1[l]; ic2[l] = s2[l]; }
	
	for(size_t i=0; i<numSamples; i++){
		float v = input[i];
#pragma clang loop unroll(full)
		for(size_t l=0; l<L; l++)
			v = BMMultiLevelSVF_tick(v, &c[l], &ic1[l], &ic2[l]);
		output[i] = v;
	}
	
#pragma clang loop unroll(full)
	for(size_t l=0; l<L; l++){ s1[l] = ic1[l]; s2[l] = ic2[l]; }
}


// all levels, both channels, one pass
static inline void BMMultiLevelSVF_staticStereoL(const BMSVFCoefs *c,
												 float *s1L, float *s2L,
												 float *s1R, float *s2R,
												 const float *inL, const float *inR,
												 float *outL, float *outR,
												 size_t numSamples, const size_t L){
	simd_float2 ic1[BM_SVF_MAX_FUSED_LEVELS], ic2[BM_SVF_MAX_FUSED_LEVELS];
#pragma clang loop unroll(full)
	for(size_t l=0; l<L; l++){
		ic1[l] = simd_make_float2(s1L[l], s1R[l]);
		ic2[l] = simd_make_float2(s2L[l], s2R[l]);
	}
	
	for(size_t i=0; i<numSamples; i++){
		simd_float2 v = simd_make_float2(inL[i], inR[i]);
#pragma clang loop unroll(full)
		for(size_t l=0; l<L; l++)
			v = BMMultiLevelSVF_tick2(v, &c[l], &ic1[l], &ic2[l]);
		outL[i] = v.x;
		outR[i] = v.y;
	}
	
#pragma clang loop unroll(full)
	for(size_t l=0; l<L; l++){
		s1L[l] = ic1[l].x; s1R[l] = ic1[l].y;
		s2L[l] = ic2[l].x; s2R[l] = ic2[l].y;
	}
}


// run L (<= BM_SVF_MAX_FUSED_LEVELS) levels through a fixed-L instantiation so
// that the level loops unroll completely and the states stay in registers
static inline void BMMultiLevelSVF_dispatchMono(const BMSVFCoefs *c, float *s1, float *s2,
												const float *input, float *output,
												size_t numSamples, size_t L){
	switch(L){
		case 1: BMMultiLevelSVF_staticMonoL(c, s1, s2, input, output, numSamples, 1); break;
		case 2: BMMultiLevelSVF_staticMonoL(c, s1, s2, input, output, numSamples, 2); break;
		case 3: BMMultiLevelSVF_staticMonoL(c, s1, s2, input, output, numSamples, 3); break;
		case 4: BMMultiLevelSVF_staticMonoL(c, s1, s2, input, output, numSamples, 4); break;
		case 5: BMMultiLevelSVF_staticMonoL(c, s1, s2, input, output, numSamples, 5); break;
		case 6: BMMultiLevelSVF_staticMonoL(c, s1, s2, input, output, numSamples, 6); break;
		case 7: BMMultiLevelSVF_staticMonoL(c, s1, s2, input, output, numSamples, 7); break;
		case 8: BMMultiLevelSVF_staticMonoL(c, s1, s2, input, output, numSamples, 8); break;
		default: break; // 0 levels: nothing to do
	}
}


static inline void BMMultiLevelSVF_dispatchStereo(const BMSVFCoefs *c,
												  float *s1L, float *s2L, float *s1R, float *s2R,
												  const float *inL, const float *inR,
												  float *outL, float *outR,
												  size_t numSamples, size_t L){
	switch(L){
		case 1: BMMultiLevelSVF_staticStereoL(c, s1L, s2L, s1R, s2R, inL, inR, outL, outR, numSamples, 1); break;
		case 2: BMMultiLevelSVF_staticStereoL(c, s1L, s2L, s1R, s2R, inL, inR, outL, outR, numSamples, 2); break;
		case 3: BMMultiLevelSVF_staticStereoL(c, s1L, s2L, s1R, s2R, inL, inR, outL, outR, numSamples, 3); break;
		case 4: BMMultiLevelSVF_staticStereoL(c, s1L, s2L, s1R, s2R, inL, inR, outL, outR, numSamples, 4); break;
		case 5: BMMultiLevelSVF_staticStereoL(c, s1L, s2L, s1R, s2R, inL, inR, outL, outR, numSamples, 5); break;
		case 6: BMMultiLevelSVF_staticStereoL(c, s1L, s2L, s1R, s2R, inL, inR, outL, outR, numSamples, 6); break;
		case 7: BMMultiLevelSVF_staticStereoL(c, s1L, s2L, s1R, s2R, inL, inR, outL, outR, numSamples, 7); break;
		case 8: BMMultiLevelSVF_staticStereoL(c, s1L, s2L, s1R, s2R, inL, inR, outL, outR, numSamples, 8); break;
		default: break;
	}
}


// gather the coefficients of levels [first, first + n) into c
static inline void BMMultiLevelSVF_gatherCoefs(const BMMultiLevelSVF *This, BMSVFCoefs *c,
											   size_t first, size_t n){
	for(size_t l=0; l<n; l++){
		size_t s = first + l;
		c[l].g0 = This->g0[s];
		c[l].g1 = This->g1[s];
		c[l].g2 = This->g2[s];
		c[l].m0 = This->m0[s];
		c[l].m1 = This->m1[s];
		c[l].m2 = This->m2[s];
		c[l].k  = This->k[s];
	}
}


static void BMMultiLevelSVF_processStaticMono(BMMultiLevelSVF *This,
											  const float *input, float *output,
											  size_t numSamples){
	for(size_t l=0; l<This->numLevels; l++)
		BMMultiLevelSVF_stageTwoParameterUpdate(This, l);
	size_t L = This->numActiveLevels;
	
	// all levels in one pass; more than BM_SVF_MAX_FUSED_LEVELS levels are
	// processed in groups of that size, the later groups in place
	BMSVFCoefs c[BM_SVF_MAX_FUSED_LEVELS];
	for(size_t first=0; first<L; first+=BM_SVF_MAX_FUSED_LEVELS){
		size_t n = BM_MIN(L - first, (size_t)BM_SVF_MAX_FUSED_LEVELS);
		BMMultiLevelSVF_gatherCoefs(This, c, first, n);
		BMMultiLevelSVF_dispatchMono(c, This->ic1eq + first, This->ic2eq + first,
									 input, output, numSamples, n);
		input = output;
	}
}


static void BMMultiLevelSVF_processStaticStereo(BMMultiLevelSVF *This,
												const float *inL, const float *inR,
												float *outL, float *outR,
												size_t numSamples){
	for(size_t l=0; l<This->numLevels; l++)
		BMMultiLevelSVF_stageTwoParameterUpdate(This, l);
	size_t L = This->numActiveLevels;
	
	// state layout: [level] for the left channel, [numLevels + level] for the right
	float *s1L = This->ic1eq, *s2L = This->ic2eq;
	float *s1R = This->ic1eq + This->numLevels, *s2R = This->ic2eq + This->numLevels;
	
	BMSVFCoefs c[BM_SVF_MAX_FUSED_LEVELS];
	for(size_t first=0; first<L; first+=BM_SVF_MAX_FUSED_LEVELS){
		size_t n = BM_MIN(L - first, (size_t)BM_SVF_MAX_FUSED_LEVELS);
		BMMultiLevelSVF_gatherCoefs(This, c, first, n);
		BMMultiLevelSVF_dispatchStereo(c, s1L + first, s2L + first, s1R + first, s2R + first,
									   inL, inR, outL, outR, numSamples, n);
		inL = outL; inR = outR;
	}
}


void BMMultiLevelSVF_calculateInterpolatedCoefficients(BMMultiLevelSVF *This, size_t level, size_t numSamples);


#pragma mark - split (two outputs from one filter)

// one section, one channel, two outputs: the lowpass output and highGain
// times the highpass output. Same expressions as BMMultiLevelSVF_tick.
static inline void BMMultiLevelSVF_tickSplit(float v0, const BMSVFCoefs *c,
											 float *ic1eq, float *ic2eq,
											 float highGain,
											 float *lowOut, float *highOut){
	float t0 = v0 - *ic2eq;
	float t1 = (c->g0 * t0) + (c->g1 * *ic1eq);
	float t2 = (c->g2 * t0) + (c->g0 * *ic1eq);
	float v1 = t1 + *ic1eq;
	float v2 = t2 + *ic2eq;
	float high = v0 - (c->k * v1) - v2;
	float low = v2;
	*lowOut = low;
	*highOut = highGain * high;
	*ic1eq += 2.0f * t1;
	*ic2eq += 2.0f * t2;
}


static inline void BMMultiLevelSVF_tickSplit2(simd_float2 v0, const BMSVFCoefs *c,
											  simd_float2 *ic1eq, simd_float2 *ic2eq,
											  float highGain,
											  simd_float2 *lowOut, simd_float2 *highOut){
	simd_float2 t0 = v0 - *ic2eq;
	simd_float2 t1 = (c->g0 * t0) + (c->g1 * *ic1eq);
	simd_float2 t2 = (c->g2 * t0) + (c->g0 * *ic1eq);
	simd_float2 v1 = t1 + *ic1eq;
	simd_float2 v2 = t2 + *ic2eq;
	simd_float2 high = v0 - (c->k * v1) - v2;
	simd_float2 low = v2;
	*lowOut = low;
	*highOut = highGain * high;
	*ic1eq += 2.0f * t1;
	*ic2eq += 2.0f * t2;
}


void BMMultiLevelSVF_processBufferMonoSplit(BMMultiLevelSVF *This,
											const float *input,
											float *lowOut, float *highOut,
											float highGain,
											size_t numSamples){
	if(numSamples == 0) return;

	assert(This->numChannels == 1);
	assert(This->numLevels == 1);
	
	if(This->numActiveLevels == 0){
		for(size_t i=0; i<numSamples; i++){
			float x = input[i];
			lowOut[i] = x; highOut[i] = 0.0f;
		}
		return;
	}

	if(This->needsClearStateVariables)
		BMMultiLevelSVF_clearStateVariables(This);
	
	if(This->shouldUpdateParam)
		BMMultiLevelSVF_updateSVFParam(This);
	
	if(This->filterSweep){
		// per-sample coefficient interpolation, as in processBufferAtLevel
		assert(numSamples <= BM_BUFFER_CHUNK_SIZE);
		BMMultiLevelSVF_calculateInterpolatedCoefficients(This, 0, numSamples);
		float ic1 = This->ic1eq[0], ic2 = This->ic2eq[0];
		for(size_t i=0; i<numSamples; i++){
			BMSVFCoefs c = {This->g0_interp[i], This->g1_interp[i], This->g2_interp[i],
							0.0f, 0.0f, 0.0f, This->k_interp[i]};
			BMMultiLevelSVF_tickSplit(input[i], &c, &ic1, &ic2, highGain, &lowOut[i], &highOut[i]);
		}
		This->ic1eq[0] = ic1; This->ic2eq[0] = ic2;
		return;
	}
	
	BMMultiLevelSVF_stageTwoParameterUpdate(This, 0);
	BMSVFCoefs c;
	BMMultiLevelSVF_gatherCoefs(This, &c, 0, 1);
	float ic1 = This->ic1eq[0], ic2 = This->ic2eq[0];
	for(size_t i=0; i<numSamples; i++)
		BMMultiLevelSVF_tickSplit(input[i], &c, &ic1, &ic2, highGain, &lowOut[i], &highOut[i]);
	This->ic1eq[0] = ic1; This->ic2eq[0] = ic2;
}


void BMMultiLevelSVF_processBufferStereoSplit(BMMultiLevelSVF *This,
											  const float *inL, const float *inR,
											  float *lowL, float *lowR,
											  float *highL, float *highR,
											  float highGain,
											  size_t numSamples){
	if(numSamples == 0) return;

	assert(This->numChannels == 2);
	assert(This->numLevels == 1);
	
	if(This->numActiveLevels == 0){
		for(size_t i=0; i<numSamples; i++){
			float left = inL[i], right = inR[i];
			lowL[i] = left; lowR[i] = right;
			highL[i] = highR[i] = 0.0f;
		}
		return;
	}

	if(This->needsClearStateVariables)
		BMMultiLevelSVF_clearStateVariables(This);
	
	if(This->shouldUpdateParam)
		BMMultiLevelSVF_updateSVFParam(This);
	
	// state layout: [0] left, [numLevels] = [1] right
	simd_float2 ic1 = simd_make_float2(This->ic1eq[0], This->ic1eq[1]);
	simd_float2 ic2 = simd_make_float2(This->ic2eq[0], This->ic2eq[1]);
	simd_float2 low, high;
	
	if(This->filterSweep){
		assert(numSamples <= BM_BUFFER_CHUNK_SIZE);
		BMMultiLevelSVF_calculateInterpolatedCoefficients(This, 0, numSamples);
		for(size_t i=0; i<numSamples; i++){
			BMSVFCoefs c = {This->g0_interp[i], This->g1_interp[i], This->g2_interp[i],
							0.0f, 0.0f, 0.0f, This->k_interp[i]};
			BMMultiLevelSVF_tickSplit2(simd_make_float2(inL[i], inR[i]), &c, &ic1, &ic2, highGain, &low, &high);
			lowL[i] = low.x; lowR[i] = low.y;
			highL[i] = high.x; highR[i] = high.y;
		}
	} else {
		BMMultiLevelSVF_stageTwoParameterUpdate(This, 0);
		BMSVFCoefs c;
		BMMultiLevelSVF_gatherCoefs(This, &c, 0, 1);
		for(size_t i=0; i<numSamples; i++){
			BMMultiLevelSVF_tickSplit2(simd_make_float2(inL[i], inR[i]), &c, &ic1, &ic2, highGain, &low, &high);
			lowL[i] = low.x; lowR[i] = low.y;
			highL[i] = high.x; highR[i] = high.y;
		}
	}
	
	This->ic1eq[0] = ic1.x; This->ic1eq[1] = ic1.y;
	This->ic2eq[0] = ic2.x; This->ic2eq[1] = ic2.y;
}

#pragma mark - process
void BMMultiLevelSVF_processBufferMono(BMMultiLevelSVF *This,
                                       const float* input,
                                       float* output,
                                       size_t numSamples){
	if(numSamples == 0) return;

    assert(This->numChannels == 1);
	
	if(This->numActiveLevels == 0){
		if(input != output) memmove(output, input, numSamples * sizeof(*output));
		return;
	}

	if(This->needsClearStateVariables)
		BMMultiLevelSVF_clearStateVariables(This);
	
	if(This->shouldUpdateParam)
		BMMultiLevelSVF_updateSVFParam(This);
	
	// static coefficients: all levels in one fused pass
	if(!This->filterSweep){
		BMMultiLevelSVF_processStaticMono(This, input, output, numSamples);
		return;
	}
    
	// filter sweep: level by level with per-sample coefficient interpolation
    for(int level = 0;level<This->numActiveLevels;level++){
        if(level==0){
            //Process into output
            BMMultiLevelSVF_processBufferAtLevel(This, level, 0, input, output, numSamples);
        }else
            BMMultiLevelSVF_processBufferAtLevel(This, level, 0, output, output, numSamples);
    }
}




void BMMultiLevelSVF_processBufferStereo(BMMultiLevelSVF *This,
                                         const float* inL, const float* inR,
                                         float* outL, float* outR, size_t numSamples){
	if(numSamples == 0) return;

    assert(This->numChannels == 2);
	
	if(This->numActiveLevels == 0){
		if(inL != outL) memmove(outL, inL, numSamples * sizeof(*outL));
		if(inR != outR) memmove(outR, inR, numSamples * sizeof(*outR));
		return;
	}

	if(This->needsClearStateVariables)
		BMMultiLevelSVF_clearStateVariables(This);
    
	// update the parameters
	if(This->shouldUpdateParam)
		BMMultiLevelSVF_updateSVFParam(This);
	
	// static coefficients: all levels and both channels in one fused pass
	if(!This->filterSweep){
		BMMultiLevelSVF_processStaticStereo(This, inL, inR, outL, outR, numSamples);
		return;
	}
    
	// Both channels use the same ramp. Advancing it separately would make
	// the right channel jump directly to the target coefficients.
	assert(numSamples <= BM_BUFFER_CHUNK_SIZE);
	for(size_t level=0; level<This->numActiveLevels; level++){
		BMMultiLevelSVF_calculateInterpolatedCoefficients(This, level, numSamples);
		size_t right = This->numLevels + level;
		simd_float2 ic1 = simd_make_float2(This->ic1eq[level], This->ic1eq[right]);
		simd_float2 ic2 = simd_make_float2(This->ic2eq[level], This->ic2eq[right]);
		for(size_t i=0; i<numSamples; i++){
			BMSVFCoefs c = {This->g0_interp[i], This->g1_interp[i], This->g2_interp[i],
				This->m0_interp[i], This->m1_interp[i], This->m2_interp[i], This->k_interp[i]};
			simd_float2 y = BMMultiLevelSVF_tick2(simd_make_float2(inL[i], inR[i]), &c, &ic1, &ic2);
			outL[i] = y.x; outR[i] = y.y;
		}
		This->ic1eq[level] = ic1.x; This->ic1eq[right] = ic1.y;
		This->ic2eq[level] = ic2.x; This->ic2eq[right] = ic2.y;
		inL = outL; inR = outR;
	}
}


/*
 * Double-precision stereo process (see the header). Same arithmetic as
 * BMMultiLevelSVF_tick, in double, both channels at once, all levels in
 * series per sample. Only the static-coefficient path exists in double: the
 * filter sweep (per-sample coefficient interpolation) is not supported here.
 */
void BMMultiLevelSVF_processBufferStereoD(BMMultiLevelSVF *This,
										  const double* inL, const double* inR,
										  double* outL, double* outR, size_t numSamples){
	if(numSamples == 0) return;

	assert(This->numChannels == 2);
	assert(!This->filterSweep);
	
	if(This->numActiveLevels == 0){
		if(inL != outL) memmove(outL, inL, numSamples * sizeof(*outL));
		if(inR != outR) memmove(outR, inR, numSamples * sizeof(*outR));
		return;
	}

	if(This->needsClearStateVariables)
		BMMultiLevelSVF_clearStateVariables(This);
	
	if(This->shouldUpdateParam)
		BMMultiLevelSVF_updateSVFParam(This);
	
	size_t L = This->numLevels;
	for(size_t l=0; l<L; l++)
		BMMultiLevelSVF_stageTwoParameterUpdate(This, l);
	
	// state layout: [level] for the left channel, [numLevels + level] for the right
	for(size_t l=0; l<This->numActiveLevels; l++){
		simd_double2 g0 = simd_make_double2(This->g0[l], This->g0[l]);
		simd_double2 g1 = simd_make_double2(This->g1[l], This->g1[l]);
		simd_double2 g2 = simd_make_double2(This->g2[l], This->g2[l]);
		simd_double2 m0 = simd_make_double2(This->m0[l], This->m0[l]);
		simd_double2 m1 = simd_make_double2(This->m1[l], This->m1[l]);
		simd_double2 m2 = simd_make_double2(This->m2[l], This->m2[l]);
		simd_double2 k  = simd_make_double2(This->k[l],  This->k[l]);
		simd_double2 ic1 = simd_make_double2(This->ic1eqD[l], This->ic1eqD[L + l]);
		simd_double2 ic2 = simd_make_double2(This->ic2eqD[l], This->ic2eqD[L + l]);
		const double *iL = (l == 0) ? inL : outL;
		const double *iR = (l == 0) ? inR : outR;
		for(size_t i=0; i<numSamples; i++){
			simd_double2 v0 = simd_make_double2(iL[i], iR[i]);
			simd_double2 t0 = v0 - ic2;
			simd_double2 t1 = (g0 * t0) + (g1 * ic1);
			simd_double2 t2 = (g2 * t0) + (g0 * ic1);
			simd_double2 v1 = t1 + ic1;
			simd_double2 v2 = t2 + ic2;
			simd_double2 high = v0 - (k * v1) - v2;
			simd_double2 out = (m0 * high) + (m1 * v1) + (m2 * v2);
			ic1 += 2.0 * t1;
			ic2 += 2.0 * t2;
			outL[i] = out.x; outR[i] = out.y;
		}
		This->ic1eqD[l] = ic1.x; This->ic1eqD[L + l] = ic1.y;
		This->ic2eqD[l] = ic2.x; This->ic2eqD[L + l] = ic2.y;
	}
}


#pragma mark - per-sample gain

// one section, one channel, returning the three outputs instead of the mix.
// Same expressions and order as BMMultiLevelSVF_tick.
static inline void BMMultiLevelSVF_tickHBL(float v0, const BMSVFCoefs *c,
										   float *ic1eq, float *ic2eq,
										   float *high, float *band, float *low){
	float t0 = v0 - *ic2eq;
	float t1 = (c->g0 * t0) + (c->g1 * *ic1eq);
	float t2 = (c->g2 * t0) + (c->g0 * *ic1eq);
	float v1 = t1 + *ic1eq;
	float v2 = t2 + *ic2eq;
	*high = v0 - (c->k * v1) - v2;
	*band = v1;
	*low = v2;
	*ic1eq += 2.0f * t1;
	*ic2eq += 2.0f * t2;
}

/*
 * Per-sample gain: level by level, each level over the whole buffer with
 * its state in registers. A level without a gain array runs the ordinary
 * tick (same output as the static path); a level with one computes the
 * three outputs and mixes them as base + G[i] gain, i.e. the level's mix
 * coefficients become m = base + G[i] gain for that sample while g and k
 * stay put.
 */
void BMMultiLevelSVF_processBufferMonoGain(BMMultiLevelSVF *This,
										   const float *input, float *output,
										   const float *const *gain,
										   size_t numSamples){
	if(numSamples == 0) return;

	assert(This->numChannels == 1);
	assert(!This->filterSweep);
	
	if(This->numActiveLevels == 0){
		if(input != output) memmove(output, input, numSamples * sizeof(*output));
		return;
	}

	if(This->needsClearStateVariables)
		BMMultiLevelSVF_clearStateVariables(This);
	if(This->shouldUpdateParam)
		BMMultiLevelSVF_updateSVFParam(This);
	
	for(size_t l=0; l<This->numActiveLevels; l++){
		BMMultiLevelSVF_stageTwoParameterUpdate(This, l);
		BMSVFCoefs c;
		BMMultiLevelSVF_gatherCoefs(This, &c, l, 1);
		const float *G = gain ? gain[l] : NULL;
		float ic1 = This->ic1eq[l], ic2 = This->ic2eq[l];
		const float *in = (l == 0) ? input : output;
		
		if(!G){
			for(size_t i=0; i<numSamples; i++)
				output[i] = BMMultiLevelSVF_tick(in[i], &c, &ic1, &ic2);
		} else {
			const BMSVFGainMix gm = This->gainMix[l];
			for(size_t i=0; i<numSamples; i++){
				float high, band, low;
				BMMultiLevelSVF_tickHBL(in[i], &c, &ic1, &ic2, &high, &band, &low);
				float yb = (gm.base[0] * high) + (gm.base[1] * band) + (gm.base[2] * low);
				float yg = (gm.gain[0] * high) + (gm.gain[1] * band) + (gm.gain[2] * low);
				output[i] = yb + G[i] * yg;
			}
		}
		This->ic1eq[l] = ic1; This->ic2eq[l] = ic2;
	}
}



void BMMultiLevelSVF_stageTwoParameterUpdate(BMMultiLevelSVF *This, size_t level){
	// parameter updates stage 2
	This->g0[level] = This->g0_target[level];
	This->g1[level] = This->g1_target[level];
	This->g2[level] = This->g2_target[level];
	This->m0[level] = This->m0_target[level];
	This->m1[level] = This->m1_target[level];
	This->m2[level] = This->m2_target[level];
	This->k[level]  = This->k_target[level];
}


void BMMultiLevelSVF_calculateInterpolatedCoefficients(BMMultiLevelSVF *This,
													   size_t level,
													   size_t numSamples){
	// compute interpolated coefficients.
	//
	// with each sample of audio input, the filter coefficients change by
	// how much?
	float g0inc = (This->g0_target[level] - This->g0[level]) / numSamples;
	float g1inc = (This->g1_target[level] - This->g1[level]) / numSamples;
	float g2inc = (This->g2_target[level] - This->g2[level]) / numSamples;
	float m0inc = (This->m0_target[level] - This->m0[level]) / numSamples;
	float m1inc = (This->m1_target[level] - This->m1[level]) / numSamples;
	float m2inc = (This->m2_target[level] - This->m2[level]) / numSamples;
	float kinc  = (This->k_target[level]  - This->k[level])  / numSamples;
	
	// These ramps will stop one sample short of reaching the target value.
	// The targets will be reached on the first sample of the next call to
	// this function because the calling function will call the coefficient
	// update function, which will copy from g_target to g and m_target to m;
	vDSP_vramp(&This->g0[level], &g0inc, This->g0_interp, 1, numSamples);
	vDSP_vramp(&This->g1[level], &g1inc, This->g1_interp, 1, numSamples);
	vDSP_vramp(&This->g2[level], &g2inc, This->g2_interp, 1, numSamples);
	vDSP_vramp(&This->m0[level], &m0inc, This->m0_interp, 1, numSamples);
	vDSP_vramp(&This->m1[level], &m1inc, This->m1_interp, 1, numSamples);
	vDSP_vramp(&This->m2[level], &m2inc, This->m2_interp, 1, numSamples);
	vDSP_vramp(&This->k[level],  &kinc,  This->k_interp,  1, numSamples);
	
	BMMultiLevelSVF_stageTwoParameterUpdate(This, level);
}







inline void BMMultiLevelSVF_processBufferAtLevel(BMMultiLevelSVF *This,
												 size_t level, size_t channel,
                                                 const float* input,
                                                 float* output,size_t numSamples){
	
	// get pointers to simplify notation for state variables
	size_t icLvl = This->numLevels*channel + level;
	float *ic1eq = &This->ic1eq[icLvl];
	float *ic2eq = &This->ic2eq[icLvl];
	
	if(This->filterSweep){
		// for filter sweep, we do not allow long buffers because linear
		// interpolation of filter coefficients is inaccurate. To process longer
		// buffers in a filter sweep, you need to break the audio up into
		// smaller sized buffers and call the filter parameter update function
		// after each one. 
		assert (numSamples <= BM_BUFFER_CHUNK_SIZE);
		
		BMMultiLevelSVF_calculateInterpolatedCoefficients(This, level, numSamples);
		
		// get pointers to simplify notation
		float *g0 = This->g0_interp;
		float *g1 = This->g1_interp;
		float *g2 = This->g2_interp;
		float *m0 = This->m0_interp;
		float *m1 = This->m1_interp;
		float *m2 = This->m2_interp;
		float *k = This->k_interp;
		
		// process the filter
		for(size_t i=0; i<numSamples; i++){
			// This code is based on the Tick 2 function in this file: https://cytomic.com/files/dsp/SvfLinearTrapezoidalSin.pdf
			float v0 = input[i];
			float t0 = v0 - *ic2eq;
			float t1 = (g0[i] * t0) + (g1[i] * *ic1eq);
			float t2 = (g2[i] * t0) + (g0[i] * *ic1eq);
			
			float v1 = t1 + *ic1eq;
			float v2 = t2 + *ic2eq;
			float high = v0 - (k[i] * v1) - v2;
			float band = v1;
			float low = v2;
			output[i] = (m0[i] * high) + (m1[i] * band) + (m2[i] * low);
			*ic1eq += 2.0f * t1;
			*ic2eq += 2.0f * t2;
		}
	} else {
		BMMultiLevelSVF_stageTwoParameterUpdate(This, level);
		
		float g0 = This->g0[level];
		float g1 = This->g1[level];
		float g2 = This->g2[level];
		float m0 = This->m0[level];
		float m1 = This->m1[level];
		float m2 = This->m2[level];
		float k  = This->k[level];
		
		for(size_t i=0; i<numSamples; i++){
			// This code is based on the Tick 2 function in this file: https://cytomic.com/files/dsp/SvfLinearTrapezoidalSin.pdf
			float v0 = input[i];
			float t0 = v0 - *ic2eq;
			float t1 = (g0 * t0) + (g1 * *ic1eq);
			float t2 = (g2 * t0) + (g0 * *ic1eq);
			float v1 = t1 + *ic1eq;
			float v2 = t2 + *ic2eq;
			float high = v0 - (k * v1) - v2;
			float band = v1;
			float low = v2;
			output[i] = (m0 * high) + (m1 * band) + (m2 * low);
			*ic1eq += 2.0f * t1;
			*ic2eq += 2.0f * t2;
		}
	}
}



inline void BMMultiLevelSVF_updateSVFParam(BMMultiLevelSVF *This){
	This->shouldUpdateParam = false;
	
	// update everything NOW, don't fade smoothly
	if(This->updateImmediately){
		This->updateImmediately = false;
		for(int i=0;i<This->numLevels;i++){
			// stage 1
			This->g0_target[i] = This->g0_pending[i];
			This->g1_target[i] = This->g1_pending[i];
			This->g2_target[i] = This->g2_pending[i];
			This->m0_target[i] = This->m0_pending[i];
			This->m1_target[i] = This->m1_pending[i];
			This->m2_target[i] = This->m2_pending[i];
			This->k_target[i]  = This->k_pending[i];
			This->gainMix[i]   = This->gainMix_pending[i];
			
			// stage 2
			This->g0[i] = This->g0_target[i];
			This->g1[i] = This->g1_target[i];
			This->g2[i] = This->g2_target[i];
			This->m0[i] = This->m0_target[i];
			This->m1[i] = This->m1_target[i];
			This->m2[i] = This->m2_target[i];
			This->k[i]  = This->k_target[i];
		}
	}
	// update with a smooth fade over the next audio buffer
	else {
		for(int i=0;i<This->numLevels;i++){
			// we do updates in two stages. This eliminates conflicts that would
			// occur if the coefficient updates are executed on the UI thread
			// while processing is going on the audio thread
			
			// stage 1: this queues values to be updated
			// the lock prevents the UI thread from modifying the pending
			// coefficient values while we are copying them to the targets
			if (BMLock_trylock(&This->lock)){
				// if we got the lock, copy the updated values.
				This->g0_target[i] = This->g0_pending[i];
				This->g1_target[i] = This->g1_pending[i];
				This->g2_target[i] = This->g2_pending[i];
				This->m0_target[i] = This->m0_pending[i];
				This->m1_target[i] = This->m1_pending[i];
				This->m2_target[i] = This->m2_pending[i];
				This->k_target[i]  = This->k_pending[i];
				This->gainMix[i]   = This->gainMix_pending[i];
				BMLock_unlock(&This->lock);
			}
			// if we didn't get the lock, schedule another update. Hopefully
			// we will get the lock next time.
			else {
				This->shouldUpdateParam = true;
			}
			
			// stage 2: This update should be done on the audio thread by
			// calling BMMultiLevelSVF_stageTwoParameterUpdate()
		}
	}
}




#pragma mark - Filters

/*
 * Every setter writes its output mix through one of these two, so that the
 * split used by the per-sample gain process functions (m = base + G gain)
 * is always in step with m. The plain mix makes G an output gain: base = 0,
 * gain = m. The split mix is for filters whose gain parameter enters the
 * mix linearly (the fixed-pole first-order shelves): m = base + G0 gain for
 * the static gain G0 given to the setter. Callers hold the lock.
 */
static inline void BMMultiLevelSVF_setMixPending(BMMultiLevelSVF *This, size_t level, double m0, double m1, double m2){
	This->m0_pending[level] = (float)m0;
	This->m1_pending[level] = (float)m1;
	This->m2_pending[level] = (float)m2;
	BMSVFGainMix *gm = &This->gainMix_pending[level];
	gm->base[0] = gm->base[1] = gm->base[2] = 0.0f;
	gm->gain[0] = (float)m0; gm->gain[1] = (float)m1; gm->gain[2] = (float)m2;
}

static inline void BMMultiLevelSVF_setMixPendingSplit(BMMultiLevelSVF *This, size_t level,
													  const double base[3], const double gain[3], double G0){
	This->m0_pending[level] = (float)(base[0] + G0 * gain[0]);
	This->m1_pending[level] = (float)(base[1] + G0 * gain[1]);
	This->m2_pending[level] = (float)(base[2] + G0 * gain[2]);
	BMSVFGainMix *gm = &This->gainMix_pending[level];
	for(int j=0; j<3; j++){ gm->base[j] = (float)base[j]; gm->gain[j] = (float)gain[j]; }
}

void BMMultiLevelSVF_setCoefficientsHelper(BMMultiLevelSVF *This, double fc, double Q, size_t level){
	// This is from the function CalcCoeff2 in https://cytomic.com/files/dsp/SvfLinearTrapezoidalSin.pdf
	double w = fc / This->sampleRate;
    double k = 1.0 / Q;
	double s1 = sin(M_PI * w);
	double s2 = sin(2.0 * M_PI * w);
	double nrm = 1.0 / (2.0 + k * s2);
	double g0 = s2 * nrm;
	double g1 = ((-2.0 * s1 * s1) - (k * s2)) * nrm;
	double g2 = (2.0 * s1 * s1) * nrm;
	
	This->g0_pending[level] = (float)g0;
	This->g1_pending[level] = (float)g1;
	This->g2_pending[level] = (float)g2;
	This->k_pending[level] = (float)k;
}





void BMMultiLevelSVF_setLowpass12dB(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, 1./sqrtf(2.), level);
}




//void BMMultiLevelSVF_setLowpass18dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd){
//	// This filter requires 2 levels. Let's make sure we have exactly 2:
//	assert((int)levelEnd - (int)levelStart == 1);
//	// And make sure the last one doesn't go off the end
//	assert(levelEnd < This->numLevels);
//	
//	// The third-order Butterworth polynomial is (s + 1)(s^2 + s + 1).
//	// Therefore a third-order Butterworth filter can be factored into
//	// a first-order filter followed by a second-order filter with Q=1.
//	//
//	// Reasoning:
//	// 1. (s + 1) is the first-order Butterworth polynomial.
//	// 2. The transfer function of an analog lowpass filter prototype
//	//    with quality factor Q is 1 / (s^2 + s/Q + 1)
//	// 3. (s^2 + s + 1) corresponds to a lowpass filter with Q = 1.
//	
//	// Set the first level to be the 1st order Butterworth lowpass
//	BMMultiLevelSVF_setLowPass6db(This, fc, levelStart);
//	
//	// Set the second level to be the 2nd order lowpass with Q = 1
//	double Q = 1.0;
//	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q, levelStart + 1);
//}





void BMMultiLevelSVF_setLowpass12dBwithQ(BMMultiLevelSVF *This, double fc, double Q, size_t level){
    assert(level < This->numLevels);
    
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, Q, level);
	BMMultiLevelSVF_setMixPending(This, level, 0.0, 0.0, 1.0);
	BMLock_unlock(&This->lock);
    
    This->shouldUpdateParam = true;
}




void BMMultiLevelSVF_setLowpass24dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd){
	// This filter requires 2 levels. Let's make sure we have exactly 2:
	assert((int)levelEnd - (int)levelStart == 1);
	// And make sure the last one doesn't go off the end
	assert(levelEnd < This->numLevels);
	
	// Q values from Mathematica:
	//   1/Sqrt(2 - Sqrt(2))
	//   1/Sqrt(2 + Sqrt(2))
	float Q1 = 1.0 / sqrt(2.0 - M_SQRT2);
	float Q2 = 1.0 / sqrt(2.0 + M_SQRT2);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q1, levelStart);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q2, levelEnd);
}




void BMMultiLevelSVF_setLowpass36dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd){
	// This filter requires 3 levels. Let's make sure we have exactly 3:
	assert((int)levelEnd - (int)levelStart == 2);
	// And make sure the last one doesn't go off the end
	assert(levelEnd < This->numLevels);
	
	// Q values from Mathematica:
	//   1/Sqrt(2 - Sqrt(3))
	//   1/Sqrt(2)
	//   1/Sqrt(2 + Sqrt(3))
	float Q1 = 1.0 / sqrt(2.0 - sqrt(3));
	float Q2 = M_SQRT1_2;
	float Q3 = 1.0 / sqrt(2.0 + sqrt(3));
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q1, levelStart);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q2, levelStart + 1);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q3, levelEnd);
}





void BMMultiLevelSVF_setLowpass48dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd){
	// This filter requires 4 levels. Let's make sure we have exactly 4:
	assert((int)levelEnd - (int)levelStart == 3);
	// And make sure the last one doesn't go off the end
	assert(levelEnd < This->numLevels);

	// Q values from Mathematica:
	//  1/Sqrt(2 - Sqrt(2 + Sqrt(2)))
	//	1/Sqrt(2 - Sqrt(2 - Sqrt(2)))
	//  1/Sqrt(2 + Sqrt(2 - Sqrt(2)))
	//	1/Sqrt(2 + Sqrt(2 + Sqrt(2)))
	float Q1 = 1.0 / sqrt(2.0 - sqrt(2.0 + M_SQRT2));
	float Q2 = 1.0 / sqrt(2.0 - sqrt(2.0 - M_SQRT2));
	float Q3 = 1.0 / sqrt(2.0 + sqrt(2.0 - M_SQRT2));
	float Q4 = 1.0 / sqrt(2.0 + sqrt(2.0 + M_SQRT2));
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q1, levelStart);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q2, levelStart + 1);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q3, levelStart + 2);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q4, levelEnd);
}





void BMMultiLevelSVF_setLowpass60dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd){
	// This filter requires 5 levels. Let's make sure we have exactly 5:
	assert((int)levelEnd - (int)levelStart == 4);
	// And make sure the last one doesn't go off the end
	assert(levelEnd < This->numLevels);
	
	// Q values from Mathematica:
	//  1/Sqrt(2 - Sqrt(2 + GoldenRatio))
	//	1/Sqrt(2 - Sqrt(3 - GoldenRatio))
	//	1/Sqrt(2)
	//	1/Sqrt(2 + Sqrt(3 - GoldenRatio))
	//	1/Sqrt(2 + Sqrt(2 + GoldenRatio))
	float Q1 = 1.0 / sqrt(2.0 - sqrt(2.0 + SVF_GOLDEN_RATIO));
	float Q2 = 1.0 / sqrt(2.0 - sqrt(3.0 - SVF_GOLDEN_RATIO));
	float Q3 = M_SQRT1_2;
	float Q4 = 1.0 / sqrt(2.0 + sqrt(3.0 - SVF_GOLDEN_RATIO));
	float Q5 = 1.0 / sqrt(2.0 + sqrt(2.0 + SVF_GOLDEN_RATIO));
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q1, levelStart);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q2, levelStart + 1);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q3, levelStart + 2);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q4, levelStart + 3);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, Q5, levelEnd);
}





void BMMultiLevelSVF_setHighpass24dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd){
	// This filter requires 2 levels. Let's make sure we have exactly 2:
	assert((int)levelEnd - (int)levelStart == 1);
	// And make sure the last one doesn't go off the end
	assert(levelEnd < This->numLevels);
	
	// Q values from Mathematica:
	//   1/Sqrt(2 - Sqrt(2))
	//   1/Sqrt(2 + Sqrt(2))
	float Q1 = 1.0 / sqrt(2.0 - M_SQRT2);
	float Q2 = 1.0 / sqrt(2.0 + M_SQRT2);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q1, levelStart);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q2, levelEnd);
	
}





void BMMultiLevelSVF_setHighpass36dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd){
	// This filter requires 3 levels. Let's make sure we have exactly 3:
	assert((int)levelEnd - (int)levelStart == 2);
	// And make sure the last one doesn't go off the end
	assert(levelEnd < This->numLevels);
	
	// Q values from Mathematica:
	//   1/Sqrt(2 - Sqrt(3))
	//   1/Sqrt(2)
	//   1/Sqrt(2 + Sqrt(3))
	float Q1 = 1.0 / sqrt(2.0 - sqrt(3));
	float Q2 = M_SQRT1_2;
	float Q3 = 1.0 / sqrt(2.0 + sqrt(3));
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q1, levelStart);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q2, levelStart + 1);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q3, levelEnd);
}





void BMMultiLevelSVF_setHighpass48dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd){
	// This filter requires 4 levels. Let's make sure we have exactly 4:
	assert((int)levelEnd - (int)levelStart == 3);
	// And make sure the last one doesn't go off the end
	assert(levelEnd < This->numLevels);
	
	// Q values from Mathematica:
	//  1/Sqrt(2 - Sqrt(2 + Sqrt(2)))
	//	1/Sqrt(2 - Sqrt(2 - Sqrt(2)))
	//  1/Sqrt(2 + Sqrt(2 - Sqrt(2)))
	//	1/Sqrt(2 + Sqrt(2 + Sqrt(2)))
	float Q1 = 1.0 / sqrt(2.0 - sqrt(2.0 + M_SQRT2));
	float Q2 = 1.0 / sqrt(2.0 - sqrt(2.0 - M_SQRT2));
	float Q3 = 1.0 / sqrt(2.0 + sqrt(2.0 - M_SQRT2));
	float Q4 = 1.0 / sqrt(2.0 + sqrt(2.0 + M_SQRT2));
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q1, levelStart);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q2, levelStart + 1);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q3, levelStart + 2);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q4, levelEnd);
}





void BMMultiLevelSVF_setHighpass60dB(BMMultiLevelSVF *This, double fc, size_t levelStart, size_t levelEnd){
	// This filter requires 5 levels. Let's make sure we have exactly 5:
	assert((int)levelEnd - (int)levelStart == 4);
	// And make sure the last one doesn't go off the end
	assert(levelEnd < This->numLevels);
	
	// Q values from Mathematica:
	//  1/Sqrt(2 - Sqrt(2 + GoldenRatio))
	//	1/Sqrt(2 - Sqrt(3 - GoldenRatio))
	//	1/Sqrt(2)
	//	1/Sqrt(2 + Sqrt(3 - GoldenRatio))
	//	1/Sqrt(2 + Sqrt(2 + GoldenRatio))
	float Q1 = 1.0 / sqrt(2.0 - sqrt(2.0 + SVF_GOLDEN_RATIO));
	float Q2 = 1.0 / sqrt(2.0 - sqrt(3.0 - SVF_GOLDEN_RATIO));
	float Q3 = M_SQRT1_2;
	float Q4 = 1.0 / sqrt(2.0 + sqrt(3.0 - SVF_GOLDEN_RATIO));
	float Q5 = 1.0 / sqrt(2.0 + sqrt(2.0 + SVF_GOLDEN_RATIO));
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q1, levelStart);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q2, levelStart + 1);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q3, levelStart + 2);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q4, levelStart + 3);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, Q5, levelEnd);
}




void BMMultiLevelSVF_setBandpass(BMMultiLevelSVF *This, double fc, double Q, size_t level){
    assert(level < This->numLevels);
    
	BMLock_lock(&This->lock);
	// band = w0 s / D peaks at 1/k = Q at fc, so scale by k for 0 dB at fc.
	// (Previously 2k, which gave a +6 dB peak.)
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, Q, level);
	BMMultiLevelSVF_setMixPending(This, level, 0.0, This->k_pending[level], 0.0);
	BMLock_unlock(&This->lock);
	
    This->shouldUpdateParam = true;
}

void BMMultiLevelSVF_setHighpass12dB(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, 1./sqrtf(2.), level);
}

void BMMultiLevelSVF_setHighpass12dBwithQ(BMMultiLevelSVF *This, double fc, double Q, size_t level){
    assert(level < This->numLevels);
    
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, Q, level);
	BMMultiLevelSVF_setMixPending(This, level, 1.0, 0.0, 0.0);
	BMLock_unlock(&This->lock);
	
    This->shouldUpdateParam = true;
}



void BMMultiLevelSVF_setLowpass24dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level1, size_t level2){
	assert(level1 != level2);
	// q / (1/sqrt 2) scales the resonant section so that q = 1/sqrt 2 is Butterworth
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, BM_SVF_BUTTERWORTH4_Q1, level1);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, q * M_SQRT2 * BM_SVF_BUTTERWORTH4_Q2, level2);
}



void BMMultiLevelSVF_setHighpass24dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level1, size_t level2){
	assert(level1 != level2);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, BM_SVF_BUTTERWORTH4_Q1, level1);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, q * M_SQRT2 * BM_SVF_BUTTERWORTH4_Q2, level2);
}



void BMMultiLevelSVF_setNumActiveLevels(BMMultiLevelSVF *This, size_t numActiveLevels){
	assert(numActiveLevels <= This->numLevels);
	// levels coming into use start from rest: nothing has been updating their state
	for(size_t l = This->numActiveLevels; l < numActiveLevels; l++)
		for(size_t ch = 0; ch < This->numChannels; ch++){
			size_t i = ch * This->numLevels + l;
			This->ic1eq[i] = This->ic2eq[i] = 0.0f;
			This->ic1eqD[i] = This->ic2eqD[i] = 0.0;
		}
	This->numActiveLevels = numActiveLevels;
}



/*
 * First-order filters on a second-order section.
 *
 * With w0 = the (prewarped) cutoff, the three SVF outputs have the
 * transfer functions
 *
 *    high = s^2 / D,   band = w0 s / D,   low = w0^2 / D,
 *    D = s^2 + k w0 s + w0^2.
 *
 * Choosing Q = 1/2 (k = 2) makes the section critically damped,
 * D = (s + w0)^2. Then
 *
 *    high + band = s (s + w0) / (s + w0)^2 = s / (s + w0)      (6 dB highpass)
 *    band + low  = w0 (s + w0) / (s + w0)^2 = w0 / (s + w0)    (6 dB lowpass)
 *
 * so one of the two poles is cancelled exactly by a zero and what remains
 * is the bilinear transform of the one-pole filter. The SVF computes its
 * coefficients from sin(2 pi fc/fs) and sin(pi fc/fs) rather than
 * tan(pi fc/fs), but the resulting transfer function is the same, so these
 * are exactly the responses of BMMultiLevelBiquad_setLowPass6db and
 * BMMultiLevelBiquad_setHighPass6db (verified numerically at 30 Hz to
 * 20 kHz, fs = 48 kHz, to within 0.001 dB).
 */
void BMMultiLevelSVF_setLowpass6dB(BMMultiLevelSVF *This, double fc, size_t level){
	assert(level < This->numLevels);
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, 0.5, level);
	BMMultiLevelSVF_setMixPending(This, level, 0.0, 1.0, 1.0);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}



void BMMultiLevelSVF_setHighpass6dB(BMMultiLevelSVF *This, double fc, size_t level){
	assert(level < This->numLevels);
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, 0.5, level);
	BMMultiLevelSVF_setMixPending(This, level, 1.0, 1.0, 0.0);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}

/*
 * The same two sums with the section's damping left free: Q = 1/sqrt 2 is
 * the section Q of 1/2 above, the one-pole filter exactly. Away from it the
 * zero no longer cancels a pole, so
 *
 *    band + low = w0 (s + w0) / D,   D = s^2 + (sqrt 2 / Q) w0 s + w0^2
 *
 * keeps the 6 dB/octave slope far from fc (two poles, one zero) and gains
 * a resonance at fc. The gain at fc is |1 + j| / (sqrt 2 / Q) = Q, the rule
 * the 12 and 24 dB types obey; the response peaks (just below fc for the
 * lowpass) once Q > sqrt(2/3) = 0.8165.
 */
void BMMultiLevelSVF_setLowpass6dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level){
	assert(level < This->numLevels);
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, q * M_SQRT1_2, level);
	BMMultiLevelSVF_setMixPending(This, level, 0.0, 1.0, 1.0);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}



void BMMultiLevelSVF_setHighpass6dBwithQ(BMMultiLevelSVF *This, double fc, double q, size_t level){
	assert(level < This->numLevels);
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, q * M_SQRT1_2, level);
	BMMultiLevelSVF_setMixPending(This, level, 1.0, 1.0, 0.0);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}

void BMMultiLevelSVF_setNotch(BMMultiLevelSVF *This, double fc, double Q, size_t level){
    assert(level < This->numLevels);
    
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, Q, level);
	BMMultiLevelSVF_setMixPending(This, level, 1.0, 0.0, 1.0);
	BMLock_unlock(&This->lock);
    
    This->shouldUpdateParam = true;
}


void BMMultiLevelSVF_setAllpass(BMMultiLevelSVF *This, double fc, double Q, size_t level){
    assert(level < This->numLevels);
    
	BMLock_lock(&This->lock);
	// high - k band + low = (s^2 - k w0 s + w0^2) / D: unit magnitude, phase
	// -180 degrees at fc. (Previously +k, which is high + k band + low = 1,
	// i.e. no filter at all.)
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, Q, level);
	BMMultiLevelSVF_setMixPending(This, level, 1.0, -This->k_pending[level], 1.0);
	BMLock_unlock(&This->lock);
    
    This->shouldUpdateParam = true;
}



void BMMultiLevelSVF_setBell(BMMultiLevelSVF *This, double fc, double gainDb, double Q, size_t level){
    assert(level < This->numLevels);
	
	// Symmetric bell, as in Simper's paper (and the RBJ cookbook): the damping
	// is scaled by the gain, k = 1/(Q*A) with A = sqrt(linear gain), and the
	// bandpass is mixed in with A^2*k. A cut is then the exact inverse of a
	// boost with the same Q. (Previously k was 1/Q regardless of gain, which
	// made cuts and boosts of the same Q have different widths.)
	double A = sqrt(BM_DB_TO_GAIN(gainDb));
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, Q * A, level);
	BMMultiLevelSVF_setMixPending(This, level, 1.0, A * A * This->k_pending[level], 1.0);
	BMLock_unlock(&This->lock);
    
    This->shouldUpdateParam = true;
}


void BMMultiLevelSVF_setBellWithSkirt(BMMultiLevelSVF *This, double fc, double bellGainDb, double skirtGainDb, double Q, size_t level){
	assert(level < This->numLevels);
	
	// The bell is defined relative to the skirt: a symmetric bell of gain
	// (bell - skirt) dB (see setBell), then the whole response is scaled by
	// the skirt gain. At fc the gain is the bell gain; far away it is the
	// skirt gain.
	double B = BM_DB_TO_GAIN(skirtGainDb);
	double A = sqrt(BM_DB_TO_GAIN((bellGainDb - skirtGainDb)));
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, Q * A, level);
	BMMultiLevelSVF_setMixPending(This, level, B, B * A * A * This->k_pending[level], B);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}


void BMMultiLevelSVF_setLowShelf(BMMultiLevelSVF *This, double fc, double gainDb, size_t level){
    BMMultiLevelSVF_setLowShelfS(This, fc, gainDb, 1.0, level);
}

void BMMultiLevelSVF_setLowShelfS(BMMultiLevelSVF *This, double fc, double gainDb, double S, size_t level){
    assert(level < This->numLevels);
    
	double A = sqrt(BM_DB_TO_GAIN(gainDb));
	
	double Q = S / sqrt(2.0);
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, Q, level);
	BMMultiLevelSVF_setMixPending(This, level, 1.0, A * This->k_pending[level], A * A);
	BMLock_unlock(&This->lock);
    
    This->shouldUpdateParam = true;
}

void BMMultiLevelSVF_setHighShelf(BMMultiLevelSVF *This, double fc, double gainDb, size_t level){
    BMMultiLevelSVF_setHighShelfS(This, fc, gainDb, 1., level);
}

void BMMultiLevelSVF_setHighShelfS(BMMultiLevelSVF *This, double fc, double gainDb, double S, size_t level){
	assert(level < This->numLevels);
	
	double A = sqrt(BM_DB_TO_GAIN(gainDb));
	
	double Q = S / sqrt(2.0);
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, Q, level);
	BMMultiLevelSVF_setMixPending(This, level, A * A, A * This->k_pending[level], 1.0);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}

/*
 * Unity gain: high + k band + low = (s^2 + k w0 s + w0^2) / D = 1, so with
 * m0 = 1, m1 = k, m2 = 1 the output equals the input for any fc and Q.
 */
void BMMultiLevelSVF_setBypass(BMMultiLevelSVF *This, size_t level){
	assert(level < This->numLevels);
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, 1000.0, M_SQRT1_2, level);
	BMMultiLevelSVF_setMixPending(This, level, 1.0, This->k_pending[level], 1.0);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}




/*
 * Flat gain: the unity mix scaled, G (high + k band + low) = G.
 */
void BMMultiLevelSVF_setGain(BMMultiLevelSVF *This, double gainDb, size_t level){
	assert(level < This->numLevels);
	
	double G = BM_DB_TO_GAIN(gainDb);
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, 1000.0, M_SQRT1_2, level);
	BMMultiLevelSVF_setMixPending(This, level, G, G * This->k_pending[level], G);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}




/*
 * Biquad-compatible bells.
 *
 * BMMultiLevelBiquad's bells use Rusty Allred's formulae with the bandwidth
 * BW = BMMultiLevelBiquad_QToBW(Q, fc) and alpha = tan(pi BW / fs). Matching
 * their width on the SVF bell (which calls setCoefficientsHelper with Q * A)
 * requires
 *
 *   Q_svf = sin(2 pi fc / fs) / (2 A tan(pi BW / fs)),  A = 10^(|gain dB| / 40)
 *
 * verified numerically against the biquad responses (max 0.004 dB
 * difference for bells between 85 Hz and 6 kHz, boosts and cuts).
 */
double BMMultiLevelSVF_QFromBiquadQAtSampleRate(double fc, double biquadQ, double relativeGainDb, double sampleRate){
	double fs = sampleRate;
	double bw = BMMultiLevelBiquad_QToBWAtSampleRate((float)biquadQ, (float)fc, (float)fs);
	double A = pow(10.0, fabs(relativeGainDb) / 40.0);
	return sin(2.0 * M_PI * fc / fs) / (2.0 * A * tan(M_PI * bw / fs));
}

static double BMMultiLevelSVF_QFromBiquadQ(BMMultiLevelSVF *This, double fc, double biquadQ, double relativeGainDb){
	return BMMultiLevelSVF_QFromBiquadQAtSampleRate(fc, biquadQ, relativeGainDb, This->sampleRate);
}



void BMMultiLevelSVF_setBellBiquadQ(BMMultiLevelSVF *This, double fc, double gainDb, double biquadQ, size_t level){
	BMMultiLevelSVF_setBell(This, fc, gainDb, BMMultiLevelSVF_QFromBiquadQ(This, fc, biquadQ, gainDb), level);
}



void BMMultiLevelSVF_setBellWithSkirtBiquadQ(BMMultiLevelSVF *This, double fc, double bellGainDb, double skirtGainDb, double biquadQ, size_t level){
	BMMultiLevelSVF_setBellWithSkirt(This, fc, bellGainDb, skirtGainDb,
									 BMMultiLevelSVF_QFromBiquadQ(This, fc, biquadQ, bellGainDb - skirtGainDb), level);
}



/*
 * Linkwitz-Riley crossover filters.
 *
 * Second order: a first-order Butterworth squared, i.e. one section with
 * Q = 1/2, -6 dB at fc. The lowpass and highpass sum to an allpass only if
 * one of them is inverted, and as in BMMultiLevelBiquad_setLinkwitzRileyHP
 * the inversion is built into the highpass (m0 = -1).
 *
 * Fourth order: a second-order Butterworth (Q = 1/sqrt2) squared, on two
 * levels. Lowpass + highpass sums to an allpass without inversion.
 */
void BMMultiLevelSVF_setLinkwitzRileyLP(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, 0.5, level);
}



void BMMultiLevelSVF_setLinkwitzRileyHP(BMMultiLevelSVF *This, double fc, size_t level){
	assert(level < This->numLevels);
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, 0.5, level);
	BMMultiLevelSVF_setMixPending(This, level, -1.0, 0.0, 0.0);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}



void BMMultiLevelSVF_setLinkwitzRileyLP4thOrder(BMMultiLevelSVF *This, double fc, size_t firstLevel){
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, M_SQRT1_2, firstLevel);
	BMMultiLevelSVF_setLowpass12dBwithQ(This, fc, M_SQRT1_2, firstLevel + 1);
}



void BMMultiLevelSVF_setLinkwitzRileyHP4thOrder(BMMultiLevelSVF *This, double fc, size_t firstLevel){
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, M_SQRT1_2, firstLevel);
	BMMultiLevelSVF_setHighpass12dBwithQ(This, fc, M_SQRT1_2, firstLevel + 1);
}



/*
 * RBJ cookbook shelving filters.
 *
 * The cookbook shelf with gain A^2 (A = 10^(dB/40)), slope S and corner
 * frequency fc is, in the analog prototype with w = s / w0,
 *
 *   high shelf:  A (A w^2 + sqrt(A)/Q w + 1) / (w^2 + sqrt(A)/Q w + A)
 *   low shelf:   A (w^2 + sqrt(A)/Q w + A) / (A w^2 + sqrt(A)/Q w + 1)
 *
 * with 1/Q = sqrt((A + 1/A)(1/S - 1) + 2). The gain at fc is A (half the
 * shelf gain in dB). Dividing numerator and denominator by A shows that the
 * denominator is a resonator at w0 * sqrt(A) (high shelf) or w0 / sqrt(A)
 * (low shelf) with the same Q, and the numerator is
 *
 *   high shelf:  A^2 high + A k band + low
 *   low shelf:       high + A k band + A^2 low
 *
 * where high, band, low are the SVF outputs of that resonator and k = 1/Q.
 * The shifted resonator frequency is prewarped through the bilinear
 * transform so that the shelf lands on fc exactly:
 *   fcShifted = (fs/pi) atan( tan(pi fc/fs) * sqrt(A) )   (high shelf)
 *   fcShifted = (fs/pi) atan( tan(pi fc/fs) / sqrt(A) )   (low shelf)
 * These setters give the same response as the AdjustableSlope shelves in
 * BMMultiLevelBiquad (verified to within 0.001 dB), including S = 0.5,
 * which is the first-order shelf.
 */
static void BMMultiLevelSVF_setShelfRBJQ(BMMultiLevelSVF *This, double fc, double gainDb, double Q, size_t level, bool highShelf){
	assert(level < This->numLevels);
	assert(Q > 0.0);
	
	double A = pow(10.0, gainDb / 40.0);
	double gw = tan(M_PI * fc / This->sampleRate);
	double fcShifted = (This->sampleRate / M_PI) * atan(highShelf ? gw * sqrt(A) : gw / sqrt(A));
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fcShifted, Q, level);
	double Ak = A * This->k_pending[level];
	if (highShelf){
		BMMultiLevelSVF_setMixPending(This, level, A * A, Ak, 1.0);
	} else {
		BMMultiLevelSVF_setMixPending(This, level, 1.0, Ak, A * A);
	}
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}



/* The cookbook slope parameter, converted to the resonator Q above. */
static void BMMultiLevelSVF_setShelfRBJ(BMMultiLevelSVF *This, double fc, double gainDb, double slope, size_t level, bool highShelf){
	assert(slope > 0.0);
	double A = pow(10.0, gainDb / 40.0);
	double Q = 1.0 / sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
	BMMultiLevelSVF_setShelfRBJQ(This, fc, gainDb, Q, level, highShelf);
}



void BMMultiLevelSVF_setLowShelfAdjustableSlope(BMMultiLevelSVF *This, double fc, double gainDb, double slope, size_t level){
	BMMultiLevelSVF_setShelfRBJ(This, fc, gainDb, slope, level, false);
}



void BMMultiLevelSVF_setHighShelfAdjustableSlope(BMMultiLevelSVF *This, double fc, double gainDb, double slope, size_t level){
	BMMultiLevelSVF_setShelfRBJ(This, fc, gainDb, slope, level, true);
}



void BMMultiLevelSVF_setLowShelfQ(BMMultiLevelSVF *This, double fc, double gainDb, double Q, size_t level){
	BMMultiLevelSVF_setShelfRBJQ(This, fc, gainDb, Q, level, false);
}



void BMMultiLevelSVF_setHighShelfQ(BMMultiLevelSVF *This, double fc, double gainDb, double Q, size_t level){
	BMMultiLevelSVF_setShelfRBJQ(This, fc, gainDb, Q, level, true);
}



double BMMultiLevelSVF_shelfQFromSlope(double gainDb, double slope){
	double A = pow(10.0, gainDb / 40.0);
	return 1.0 / sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
}


#pragma mark - BMMultiLevelBiquad's setters (...AsBiquad)

/*
 * Each of these is the BMMultiLevelBiquad setter of the same name: its
 * BMMultiLevelBiquad_design* function (the biquad's own coefficient
 * formula) converted exactly to this section (see the header's guide to
 * the setter families).
 */
static inline void BMMultiLevelSVF_setBiquadDesign(BMMultiLevelSVF *This, size_t level, BMBiquadSectionCoefs c){
	BMMultiLevelSVF_setFromBiquadCoefficients(This, c.b0, c.b1, c.b2, c.a1, c.a2, level);
}

void BMMultiLevelSVF_setBypassAsBiquad(BMMultiLevelSVF *This, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designBypass());
}
void BMMultiLevelSVF_setCoefficientZAsBiquad(BMMultiLevelSVF *This, size_t level, const double *coeff){
	BMMultiLevelSVF_setFromBiquadCoefficients(This, coeff[0], coeff[1], coeff[2], coeff[3], coeff[4], level);
}
void BMMultiLevelSVF_setBellAsBiquad(BMMultiLevelSVF *This, float fc, float bandwidth, float gain_db, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designBell(fc, bandwidth, gain_db, This->sampleRate));
}
void BMMultiLevelSVF_setBellQAsBiquad(BMMultiLevelSVF *This, float fc, float Q, float gain_db, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designBellQ(fc, Q, gain_db, This->sampleRate));
}
void BMMultiLevelSVF_setBellWithSkirtAsBiquad(BMMultiLevelSVF *This, float fc, float Q, float bellGainDb, float skirtGainDb, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designBellWithSkirt(fc, Q, bellGainDb, skirtGainDb, This->sampleRate));
}
void BMMultiLevelSVF_setHighShelfAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designHighShelf(fc, gain_db, This->sampleRate));
}
void BMMultiLevelSVF_setLowShelfAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designLowShelf(fc, gain_db, This->sampleRate));
}
void BMMultiLevelSVF_setHighShelfAdjustableSlopeAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, float slope, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designHighShelfAdjustableSlope(fc, gain_db, slope, This->sampleRate));
}
void BMMultiLevelSVF_setLowShelfAdjustableSlopeAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, float slope, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designLowShelfAdjustableSlope(fc, gain_db, slope, This->sampleRate));
}
void BMMultiLevelSVF_setHighShelfFirstOrderAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designHighShelfFirstOrder(fc, gain_db, This->sampleRate));
}
void BMMultiLevelSVF_setLowShelfFirstOrderAsBiquad(BMMultiLevelSVF *This, float fc, float gain_db, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designLowShelfFirstOrder(fc, gain_db, This->sampleRate));
}
void BMMultiLevelSVF_setLowPass12dbAsBiquad(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designLowPass12db(fc, This->sampleRate));
}
void BMMultiLevelSVF_setLowPassQ12dbAsBiquad(BMMultiLevelSVF *This, double fc, double q, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designLowPassQ12db(fc, q, This->sampleRate));
}
void BMMultiLevelSVF_setHighPass12dbAsBiquad(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designHighPass12db(fc, This->sampleRate));
}
void BMMultiLevelSVF_setHighPass12dbNegAsBiquad(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designHighPass12dbNeg(fc, This->sampleRate));
}
void BMMultiLevelSVF_setHighPassQ12dbAsBiquad(BMMultiLevelSVF *This, double fc, double q, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designHighPassQ12db(fc, q, This->sampleRate));
}
void BMMultiLevelSVF_setHighOrderBWLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel, size_t numLevels){
	size_t N = numLevels * 2;
	for(size_t i=0; i<numLevels; i++)
		BMMultiLevelSVF_setBiquadDesign(This, i+firstLevel, BMMultiLevelBiquad_designBWLPSection(fc, N, i+1, This->sampleRate));
}
void BMMultiLevelSVF_setLegendreLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel, size_t numLevels){
	size_t N = numLevels * 2;
	for(size_t i=0; i<numLevels; i++)
		BMMultiLevelSVF_setBiquadDesign(This, i+firstLevel, BMMultiLevelBiquad_designLegendreLPSection(fc, N, i+1, This->sampleRate));
}
void BMMultiLevelSVF_setCriticallyDampedLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel, size_t numLevels){
	for(size_t i=0; i<numLevels; i++)
		BMMultiLevelSVF_setBiquadDesign(This, i+firstLevel, BMMultiLevelBiquad_designCriticallyDampedLPSection(fc, This->sampleRate));
}
void BMMultiLevelSVF_setBesselLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel, size_t numLevels){
	size_t N = numLevels * 2;
	for(size_t i=0; i<numLevels; i++)
		BMMultiLevelSVF_setBiquadDesign(This, i+firstLevel, BMMultiLevelBiquad_designBesselLPSection(fc, N, i+1, This->sampleRate));
}
void BMMultiLevelSVF_setLowPass6dbAsBiquad(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designLowPass6db(fc, This->sampleRate));
}
void BMMultiLevelSVF_setHighPass6dbAsBiquad(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designHighPass6db(fc, This->sampleRate));
}
void BMMultiLevelSVF_setHighPassLowPassAsBiquad(BMMultiLevelSVF *This, double highPassFc, double lowPassFc, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designHighPassLowPass(highPassFc, lowPassFc, This->sampleRate));
}
void BMMultiLevelSVF_setLinkwitzRileyLPAsBiquad(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designLinkwitzRileyLP(fc, This->sampleRate));
}
void BMMultiLevelSVF_setLinkwitzRileyHPAsBiquad(BMMultiLevelSVF *This, double fc, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designLinkwitzRileyHP(fc, This->sampleRate));
}
void BMMultiLevelSVF_setLinkwitzRileyLP4thOrderAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel){
	BMMultiLevelSVF_setLowPass12dbAsBiquad(This, fc, firstLevel);
	BMMultiLevelSVF_setLowPass12dbAsBiquad(This, fc, firstLevel+1);
}
void BMMultiLevelSVF_setLinkwitzRileyHP4thOrderAsBiquad(BMMultiLevelSVF *This, double fc, size_t firstLevel){
	BMMultiLevelSVF_setHighPass12dbAsBiquad(This, fc, firstLevel);
	BMMultiLevelSVF_setHighPass12dbAsBiquad(This, fc, firstLevel+1);
}
void BMMultiLevelSVF_setAllpass2ndOrderAsBiquad(BMMultiLevelSVF *This, double c1, double c2, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designAllpass2ndOrder(c1, c2));
}
void BMMultiLevelSVF_setAllpass1stOrderAsBiquad(BMMultiLevelSVF *This, double c, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designAllpass1stOrder(c));
}
void BMMultiLevelSVF_setCriticallyDampedPhaseCompensatorAsBiquad(BMMultiLevelSVF *This, double lowpassFC, size_t level){
	BMMultiLevelSVF_setBiquadDesign(This, level, BMMultiLevelBiquad_designCriticallyDampedPhaseCompensator(lowpassFC, This->sampleRate));
}


#pragma mark - First order shelves

/*
 * First-order shelves in a second-order section.
 *
 * In the section's transfer function (s normalised to the prewarped fc)
 *
 *     H(s) = (m0 s^2 + m1 s + m2) / (s^2 + k s + 1)
 *
 * a first-order shelf needs one pole and one zero that cancel. Two real
 * poles whose product is 1 sit at -p and -1/p, with k = p + 1/p. With G the
 * linear shelf gain, G = 10^(dB/20), the numerators are
 *
 *   high shelf:  m = (G, G + 1, 1)      (G s + 1)(s + 1)
 *   low shelf:   m = (1, G + 1, G)      (s + G)(s + 1)
 *
 * Fixed pole, k = 2 (poles at -1, -1): the (s + 1) factors cancel and
 *
 *   high shelf:  H = (G s + 1) / (s + 1)     pole at fc, zero at fc / G
 *   low shelf:   H = (s + G) / (s + 1)       pole at fc, zero at G fc
 *
 * for boost and cut alike. g0, g1, g2 and k do not depend on the gain, so a
 * gain change only moves the output mix, linearly in G. The state variables
 * are those of a static resonator, so the gain can be modulated at any rate
 * with no recomputation of the recursion and no transient beyond the mix
 * change itself; the sweep mode's linear interpolation of the coefficients
 * over a buffer is exact for a linear ramp of G.
 *
 * Symmetric (same response as BMMultiLevelBiquad_setHighShelfFirstOrder and
 * setLowShelfFirstOrder): boost as above; cut with poles at -G and -1/G,
 * k = G + 1/G, so that
 *
 *   high shelf cut:  H = G (s + 1) / (s + G)      zero at fc, pole at G fc
 *   low shelf cut:   H = G (s + 1) / (G s + 1)    zero at fc, pole at fc / G
 *
 * which is the inverse of the boost by 1/G at the same fc. Here k, and with
 * it g0, g1, g2, change with the gain on the cut side, so modulating the
 * gain costs a coefficient recomputation per update.
 *
 * In float32 the cancelling pole-zero pair does not cancel exactly, but the
 * pair is critically damped and its residual is a doublet of relative size
 * ~1e-7, verified against the biquad first-order shelves to 1e-4 of the
 * impulse response peak and 1e-3 dB in magnitude.
 */
static void BMMultiLevelSVF_setShelfFirstOrder(BMMultiLevelSVF *This, double fc, double gainDb, size_t level, bool highShelf, bool fixedPole){
	assert(level < This->numLevels);
	
	double G = BM_DB_TO_GAIN(gainDb);
	double k = (fixedPole || G >= 1.0) ? 2.0 : G + 1.0 / G;
	
	BMLock_lock(&This->lock);
	BMMultiLevelSVF_setCoefficientsHelper(This, fc, 1.0 / k, level);
	if(fixedPole){
		// the gain enters the mix linearly: m = base + G gain
		const double baseH[3] = {0.0, 1.0, 1.0}, gainH[3] = {1.0, 1.0, 0.0};
		const double baseL[3] = {1.0, 1.0, 0.0}, gainL[3] = {0.0, 1.0, 1.0};
		BMMultiLevelSVF_setMixPendingSplit(This, level, highShelf ? baseH : baseL, highShelf ? gainH : gainL, G);
	} else {
		BMMultiLevelSVF_setMixPending(This, level, highShelf ? G : 1.0, G + 1.0, highShelf ? 1.0 : G);
	}
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}



void BMMultiLevelSVF_setHighShelfFirstOrder(BMMultiLevelSVF *This, double fc, double gainDb, size_t level){
	BMMultiLevelSVF_setShelfFirstOrder(This, fc, gainDb, level, true, false);
}



void BMMultiLevelSVF_setLowShelfFirstOrder(BMMultiLevelSVF *This, double fc, double gainDb, size_t level){
	BMMultiLevelSVF_setShelfFirstOrder(This, fc, gainDb, level, false, false);
}



void BMMultiLevelSVF_setHighShelfFirstOrderFixedPole(BMMultiLevelSVF *This, double fc, double gainDb, size_t level){
	BMMultiLevelSVF_setShelfFirstOrder(This, fc, gainDb, level, true, true);
}



void BMMultiLevelSVF_setLowShelfFirstOrderFixedPole(BMMultiLevelSVF *This, double fc, double gainDb, size_t level){
	BMMultiLevelSVF_setShelfFirstOrder(This, fc, gainDb, level, false, true);
}


#pragma mark - Biquad <-> SVF conversion

/*
 * Conversion between the coefficients of this SVF and those of a direct form
 * biquad section
 *
 *     H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2),
 *
 * which is the form stored in BMMultiLevelBiquad.coefficients_d (b0, b1, b2,
 * a1, a2 with a0 normalised to 1) and used by vDSP_biquad.
 *
 * Derivation
 *
 * The tick in BMMultiLevelSVF_tick is Andrew Simper's trapezoidal SVF (Tick 2
 * in SvfLinearTrapezoidalSin.pdf; the same recurrence as the tick of
 * SvfLinearTrapOptimised2.pdf with that paper's a1 = 1 + g1, a2 = g0,
 * a3 = g2). With g = tan(pi fc / fs) and D = 1 + g (g + k) our coefficients
 * are
 *
 *     g0 = g / D,   g1 = 1/D - 1,   g2 = g^2 / D.
 *
 * (setCoefficientsHelper computes the same numbers from sines: divide its
 * numerator and denominator by 2 cos^2(pi fc / fs).) The tick is the bilinear
 * transform of the analogue SVF, whose three outputs are
 *
 *     high = s^2 / P,   band = g s / P,   low = g^2 / P,   P = s^2 + k g s + g^2
 *
 * with s = (z - 1) / (z + 1). Multiplying numerator and denominator by
 * (z + 1)^2 and collecting powers of z gives a biquad with
 *
 *     numerator   = m0 (z - 1)^2 + m1 g (z^2 - 1) + m2 g^2 (z + 1)^2
 *     denominator = (z - 1)^2 + k g (z^2 - 1) + g^2 (z + 1)^2
 *                 = D z^2 + 2 (g^2 - 1) z + (1 - k g + g^2).
 *
 * Dividing through by D and substituting g/D = g0, g^2/D = g2, 1/D = 1 + g1:
 *
 *     b0 = (m0 + m1 g + m2 g^2) / D  =  m0 (1 + g1) + m1 g0 + m2 g2
 *     b1 = 2 (m2 g^2 - m0) / D       =  2 (m2 g2 - m0 (1 + g1))
 *     b2 = (m0 - m1 g + m2 g^2) / D  =  m0 (1 + g1) - m1 g0 + m2 g2
 *     a1 = 2 (g^2 - 1) / D           =  2 (g2 - (1 + g1))
 *     a2 = (1 - k g + g^2) / D       =  (1 + g1) - k g0 + g2
 *
 * These agree with Simper's "Convert between Trapezoidal Integrated SVF and
 * DF1 biquad coefficients" (https://cytomic.com/files/dsp/Convert-TrapSVF-DF1.pdf).
 *
 * The inverse. Evaluating the normalised denominator at z = 1 and z = -1,
 *
 *     1 + a1 + a2 = 4 g^2 / D = 4 g2,        1 - a1 + a2 = 4 / D = 4 (1 + g1),
 *
 * so g2 and 1/D come straight from a1 and a2, g0 = g/D = sqrt(g2 / D), and
 * 1 - a2 = 2 k g / D = 2 k g0 gives k. The numerator at z = 1, at z = -1 and
 * its odd part give the mix:
 *
 *     m2 = (b0 + b1 + b2) / (1 + a1 + a2)     (= H(1), the DC gain)
 *     m0 = (b0 - b1 + b2) / (1 - a1 + a2)     (= H(-1), the Nyquist gain)
 *     m1 = (b0 - b2) / (2 g0)
 *
 * Every stable biquad has 1 + a1 + a2 > 0 and 1 - a1 + a2 > 0 (the stability
 * triangle |a1| < 1 + a2, |a2| < 1), so g0 is real and positive and the
 * conversion is exact, with no sign ambiguity. First-order sections
 * (b2 = a2 = 0) convert as well: they become an SVF with one pole at z = 0,
 * k = g + 1/g.
 *
 * This replaces an earlier version based on MatchDF1Coeff in
 * SvfLinearTrapezoidalSin.pdf. That one used Simper's swapped a/b naming for
 * the DF1 and took square roots and absolute values, so it lost the signs of
 * m0, m1 and m2 (wrong for the negated Linkwitz-Riley highpass, allpasses,
 * shelves with cut and first-order sections).
 */

BMSVFSectionCoefs BMMultiLevelSVF_fromBiquadCoefs(BMBiquadSectionCoefs b){
	double sumP = 1.0 + b.a1 + b.a2;   // 4 g^2 / D, must be > 0
	double sumN = 1.0 - b.a1 + b.a2;   // 4 / D, must be > 0
	assert(sumP > 0.0 && sumN > 0.0); // stable poles, none at z = 1 or z = -1
	
	BMSVFSectionCoefs c;
	double invD = 0.25 * sumN;          // 1 / (1 + g (g + k))
	c.g2 = 0.25 * sumP;                 // g^2 / D
	c.g0 = sqrt(c.g2 * invD);           // g / D
	c.g1 = invD - 1.0;
	c.k  = (1.0 - b.a2) / (2.0 * c.g0);
	c.m0 = (b.b0 - b.b1 + b.b2) / sumN;
	c.m2 = (b.b0 + b.b1 + b.b2) / sumP;
	c.m1 = (b.b0 - b.b2) / (2.0 * c.g0);
	return c;
}



BMBiquadSectionCoefs BMMultiLevelSVF_toBiquadCoefs(BMSVFSectionCoefs c){
	double invD = 1.0 + c.g1;           // 1 / (1 + g (g + k))
	BMBiquadSectionCoefs b;
	b.b0 = c.m0 * invD + c.m1 * c.g0 + c.m2 * c.g2;
	b.b1 = 2.0 * (c.m2 * c.g2 - c.m0 * invD);
	b.b2 = c.m0 * invD - c.m1 * c.g0 + c.m2 * c.g2;
	b.a1 = 2.0 * (c.g2 - invD);
	b.a2 = invD - c.k * c.g0 + c.g2;
	return b;
}



void BMMultiLevelSVF_setFromBiquadCoefficients(BMMultiLevelSVF *This,
											   double b0, double b1, double b2,
											   double a1, double a2,
											   size_t level){
	assert(level < This->numLevels);
	
	BMBiquadSectionCoefs b = {b0, b1, b2, a1, a2};
	BMSVFSectionCoefs c = BMMultiLevelSVF_fromBiquadCoefs(b);
	
	BMLock_lock(&This->lock);
	This->g0_pending[level] = (float)c.g0;
	This->g1_pending[level] = (float)c.g1;
	This->g2_pending[level] = (float)c.g2;
	This->k_pending[level]  = (float)c.k;
	BMMultiLevelSVF_setMixPending(This, level, (float)c.m0, (float)c.m1, (float)c.m2);
	BMLock_unlock(&This->lock);
	
	This->shouldUpdateParam = true;
}



/*
 * The most recently set coefficients of one level. These are the pending
 * values, i.e. what the filter is going to be after the next process call
 * picks the update up, which is what a caller who has just called a setter
 * expects to read back.
 */
static BMSVFSectionCoefs BMMultiLevelSVF_getSectionCoefs(BMMultiLevelSVF *This, size_t level){
	assert(level < This->numLevels);
	BMSVFSectionCoefs c;
	BMLock_lock(&This->lock);
	c.g0 = This->g0_pending[level];
	c.g1 = This->g1_pending[level];
	c.g2 = This->g2_pending[level];
	c.k  = This->k_pending[level];
	c.m0 = This->m0_pending[level];
	c.m1 = This->m1_pending[level];
	c.m2 = This->m2_pending[level];
	BMLock_unlock(&This->lock);
	return c;
}



void BMMultiLevelSVF_getBiquadCoefficients(BMMultiLevelSVF *This, size_t level, double *coefficients){
	BMBiquadSectionCoefs b = BMMultiLevelSVF_toBiquadCoefs(BMMultiLevelSVF_getSectionCoefs(This, level));
	coefficients[0] = b.b0;
	coefficients[1] = b.b1;
	coefficients[2] = b.b2;
	coefficients[3] = b.a1;
	coefficients[4] = b.a2;
}



void BMMultiLevelSVF_copyFromBiquad(BMMultiLevelSVF *This, const BMMultiLevelBiquad *biquad){
	assert(biquad->numLevels == This->numLevels);
	
	for(size_t level = 0; level < This->numLevels; level++){
		// both channels of the biquad always hold the same coefficients, so
		// read channel 0
		const double *b = biquad->coefficients_d + level * biquad->numChannels * 5;
		BMMultiLevelSVF_setFromBiquadCoefficients(This, b[0], b[1], b[2], b[3], b[4], level);
	}
}



void BMMultiLevelSVF_copyToBiquad(BMMultiLevelSVF *This, BMMultiLevelBiquad *biquad){
	assert(biquad->numLevels == This->numLevels);
	assert(biquad->numChannels <= 4);
	
	for(size_t level = 0; level < This->numLevels; level++){
		double coefficients[4 * 5];
		BMMultiLevelSVF_getBiquadCoefficients(This, level, coefficients);
		// setCoefficientZ wants one set of five per channel
		for(size_t ch = 1; ch < biquad->numChannels; ch++)
			memcpy(coefficients + ch * 5, coefficients, 5 * sizeof(double));
		BMMultiLevelBiquad_setCoefficientZ(biquad, level, coefficients);
	}
}



#pragma mark - Transfer function, group delay and phase

/*
 * These convert each level to biquad coefficients and use the same per-section
 * formulae as BMMultiLevelBiquad (BMBiquadSection_*), so the SVF and the biquad
 * plot identically for the same filter.
 */

static DSPDoubleComplex BMMultiLevelSVF_tfEval(BMMultiLevelSVF *This, DSPDoubleComplex z){
	DSPDoubleComplex out = DSPDoubleComplex_init(1.0, 0.0);
	for(size_t level = 0; level < This->numActiveLevels; level++){
		double c[5];
		BMMultiLevelSVF_getBiquadCoefficients(This, level, c);
		out = DSPDoubleComplex_cmul(out, BMBiquadSection_tfEval(c[0], c[1], c[2], c[3], c[4], z));
	}
	return out;
}



void BMMultiLevelSVF_tfMagVector(BMMultiLevelSVF *This, const float *frequency, float *magnitude, size_t length){
	for(size_t i = 0; i < length; i++){
		DSPDoubleComplex z = DSPDoubleComplex_z(frequency[i], This->sampleRate);
		magnitude[i] = DSPDoubleComplex_abs(BMMultiLevelSVF_tfEval(This, z));
	}
}



void BMMultiLevelSVF_tfMagVectorAtLevel(BMMultiLevelSVF *This, const float *frequency, float *magnitude, size_t length, size_t level){
	double c[5];
	BMMultiLevelSVF_getBiquadCoefficients(This, level, c);
	for(size_t i = 0; i < length; i++){
		DSPDoubleComplex z = DSPDoubleComplex_z(frequency[i], This->sampleRate);
		magnitude[i] = DSPDoubleComplex_abs(BMBiquadSection_tfEval(c[0], c[1], c[2], c[3], c[4], z));
	}
}



double BMMultiLevelSVF_groupDelay(BMMultiLevelSVF *This, double freq){
	double w = 2.0 * M_PI * freq / This->sampleRate;
	double delay = 0.0;
	for(size_t level = 0; level < This->numActiveLevels; level++){
		double c[5];
		BMMultiLevelSVF_getBiquadCoefficients(This, level, c);
		delay += BMBiquadSection_groupDelay(c[0], c[1], c[2], c[3], c[4], w);
	}
	return delay;
}



double BMMultiLevelSVF_phaseResponse(BMMultiLevelSVF *This, double freq){
	double w = 2.0 * M_PI * freq / This->sampleRate;
	double phase = 0.0;
	for(size_t level = 0; level < This->numActiveLevels; level++){
		double c[5];
		BMMultiLevelSVF_getBiquadCoefficients(This, level, c);
		phase += BMBiquadSection_phaseResponse(c[0], c[1], c[2], c[3], c[4], w);
	}
	return BMBiquadSection_wrapPhase(phase);
}



void BMMultiLevelSVF_impulseResponse(BMMultiLevelSVF *This,size_t frameCount){
    float* irBuffer = malloc(sizeof(float)*frameCount);
    float* irBufferR = malloc(sizeof(float)*frameCount);
    float* outBuffer = malloc(sizeof(float)*frameCount);
    float* outBufferR = malloc(sizeof(float)*frameCount);
    for(int i=0;i<frameCount;i++){
        if(i==0){
            irBuffer[i] = 1;
            irBufferR[i] = 1;
        }else{
            irBuffer[i] = 0;
            irBufferR[i] = 0;
        }
    }
    
    BMMultiLevelSVF_processBufferStereo(This, irBuffer, irBufferR, outBuffer, outBufferR, frameCount);
    
    printf("\[");
    for(size_t i=0; i<(frameCount-1); i++)
        printf("%f\n", outBufferR[i]);
    printf("%f],",outBufferR[frameCount-1]);
    
}

