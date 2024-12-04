//
//  BMLFOPan2.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 5/8/24.
//

#include <assert.h>
#include <Accelerate/Accelerate.h>
#include "BMLFOPan2.h"
#include "BMUnitConversion.h"



void BMLFOPan2_processStereoQuadratureDb(BMLFOPan2 *This,
										 float *inL, float *inR,
										 float *outL, float *outR,
										 size_t numSamples){
	// make sure this BMLFOPan2 was initialized with the correct type for this process function
	assert(This->type == PANTYPE_QUADRATURE_DB);
	
	// chunked processing
	size_t samplesRemaining = numSamples;
	size_t samplesProcessed = 0;
	
	while(samplesRemaining > 0){
		size_t samplesProcessing = BM_MIN(samplesRemaining, BM_BUFFER_CHUNK_SIZE);
		
		// generate gain control signal for mixing to L and R channels
		BMLFO_processQuadrature(&This->lfo, This->mixControlSignalL, This->mixControlSignalR, samplesProcessing);
		
		// convert the gain control signals from decibels to linear gain
		BMConv_dBToGainV(This->mixControlSignalL, This->mixControlSignalL, samplesProcessing);
		BMConv_dBToGainV(This->mixControlSignalR, This->mixControlSignalR, samplesProcessing);
		
		// mix output to L channel
		vDSP_vmul(inL + samplesProcessed, 1,
				  This->mixControlSignalL, 1,
				  outL + samplesProcessed, 1,
				  samplesProcessing);
		
		// mix output to R channel
		vDSP_vmul(inR + samplesProcessed, 1,
				  This->mixControlSignalR, 1,
				  outR + samplesProcessed, 1,
				  samplesProcessing);
		
		samplesProcessed += samplesProcessing;
		samplesRemaining -= samplesProcessing;
	}
}




void BMLFOPan2_processStereoOutOfPhaseCutDbTriangle(BMLFOPan2 *This,
													float *inL, float *inR,
													float *outL, float *outR,
													size_t numSamples){
	// make sure this BMLFOPan2 was initialized with the correct type for this process function
	assert(This->type == PANTYPE_TRIANGLE_CUT_ONLY_OUT_OF_PHASE_DB);
	
	// chunked processing
	size_t samplesRemaining = numSamples;
	size_t samplesProcessed = 0;
	
	while(samplesRemaining > 0){
		size_t samplesProcessing = BM_MIN(samplesRemaining, BM_BUFFER_CHUNK_SIZE);
		
		// generate gain control signal for mixing the left channel
		BMTriangleLFO_process(&This->TriangleLFO, This->mixControlSignalL, samplesProcessing);
		
		// the gain control signal for the right channel is the opposite of the left
		vDSP_vneg(This->mixControlSignalL, 1, This->mixControlSignalR, 1, samplesProcessing);
		
		// we want the gain cut but not boosted so we zero out all positive values
		//
		// since we don't have a vDSP function for that, we start by zeroing out the negative values
		float zero = 0.0;
		vDSP_vthr(This->mixControlSignalL, 1, &zero, This->mixControlSignalL, 1, samplesProcessing);
		vDSP_vthr(This->mixControlSignalR, 1, &zero, This->mixControlSignalR, 1, samplesProcessing);
		// and then we negate the result to have only negative values
		vDSP_vneg(This->mixControlSignalL, 1, This->mixControlSignalL, 1, samplesProcessing);
		vDSP_vneg(This->mixControlSignalR, 1, This->mixControlSignalR, 1, samplesProcessing);
		
		// convert the gain control signals from decibels to linear gain
		BMConv_dBToGainV(This->mixControlSignalL, This->mixControlSignalL, samplesProcessing);
		BMConv_dBToGainV(This->mixControlSignalR, This->mixControlSignalR, samplesProcessing);
		
		// mix output to L channel
		vDSP_vmul(inL + samplesProcessed, 1,
				  This->mixControlSignalL, 1,
				  outL + samplesProcessed, 1,
				  samplesProcessing);
		
		// mix output to R channel
		vDSP_vmul(inR + samplesProcessed, 1,
				  This->mixControlSignalR, 1,
				  outR + samplesProcessed, 1,
				  samplesProcessing);
		
		samplesProcessed += samplesProcessing;
		samplesRemaining -= samplesProcessing;
	}
}




/*!
 *BMLFOPan2_processStereo
 *
 * Input is mixed to mono before processing.
 */
void BMLFOPan2_processStereoMixdown(BMLFOPan2 *This,
								  float *inL, float *inR,
								  float *outL, float *outR,
								  size_t numSamples){
	// make sure this BMLFOPan2 was initialized with the correct type for this process function
	assert(This->type == PANTYPE_MIXDOWNLINEAR);
	
	// chunked processing
	size_t samplesRemaining = numSamples;
	size_t samplesProcessed = 0;
	
	while(samplesRemaining > 0){
		size_t samplesProcessing = BM_MIN(samplesRemaining, BM_BUFFER_CHUNK_SIZE);
		
		// mix input to mono and buffer
		float gain = M_SQRT1_2; // reduce the gain a bit when mixing because hard pan of the mixed singal will have higher peak volume
		vDSP_vasm(inL + samplesProcessed, 1, inR + samplesProcessed, 1,
				  &gain,
				  This->buffer, 1,
				  samplesProcessing);
		
		// generate gain control signal for mixing to L channel
		BMLFO_process(&This->lfo, This->mixControlSignalL, samplesProcessing);
		
		// generate gain control signal for mixing to R channel by doing 1.0 - L
		float negOne = -1.0f;
		vDSP_vsadd(This->mixControlSignalL, 1, &negOne, This->mixControlSignalR, 1, samplesProcessing);
		vDSP_vneg(This->mixControlSignalR, 1, This->mixControlSignalR, 1, samplesProcessing);
		
		// mix output to R channel
		vDSP_vmul(This->buffer, 1,
				  This->mixControlSignalR, 1,
				  outR + samplesProcessed, 1,
				  samplesProcessing);
		
		// mix output to L channel
		vDSP_vmul(This->buffer, 1,
				  This->mixControlSignalL, 1,
				  outL + samplesProcessed, 1,
				  samplesProcessing);
		
		samplesProcessed += samplesProcessing;
		samplesRemaining -= samplesProcessing;
	}
}




void BMLFOPan2_init(BMLFOPan2 *This, float LFOFreqHz, float depth, float sampleRate){
	assert(depth >= 0.0f && depth <= 1.0f);

	float min = 0.5f - (0.5f * depth);
	float max = 0.5f + (0.5f * depth);
	BMTriangleLFO_init(&This->TriangleLFO, LFOFreqHz, min, max, sampleRate);
	BMLFO_init(&This->lfo, LFOFreqHz, min, max, sampleRate);
	
	This->mixControlSignalL = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->mixControlSignalR = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->buffer = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	
	// set the type of panning for this init function
	This->type = PANTYPE_MIXDOWNLINEAR;
}




void BMLFOPan2_initQuadratureDb(BMLFOPan2 *This, float LFOFreqHz, float depthDb, float sampleRate){
	// since the pan effect is gain-cut only, the depth in decibels must not be positive.
	assert(depthDb <= 0.0f);

	float min = depthDb;
	float max = 0.0;
	BMTriangleLFO_init(&This->TriangleLFO, LFOFreqHz, min, max, sampleRate);
	BMLFO_init(&This->lfo, LFOFreqHz, min, max, sampleRate);
	
	This->mixControlSignalL = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->mixControlSignalR = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->buffer = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	
	// set the type of panning
	This->type = PANTYPE_QUADRATURE_DB;
}




void BMLFOPan2_initOutOfPhaseCutDbTriangle(BMLFOPan2 *This, float LFOFreqHz, float depthDb, float sampleRate){
	// since the pan effect is gain-cut only, the depth in decibels must not be positive.
	assert(depthDb <= 0.0f);

	float min = depthDb;
	float max = -depthDb;
	BMTriangleLFO_init(&This->TriangleLFO, LFOFreqHz, min, max, sampleRate);
	BMLFO_init(&This->lfo, LFOFreqHz, min, max, sampleRate);
	
	This->mixControlSignalL = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->mixControlSignalR = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	This->buffer = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
	
	// set the type of panning
	This->type = PANTYPE_TRIANGLE_CUT_ONLY_OUT_OF_PHASE_DB;
}




void BMLFOPan2_setDepthSmoothlyDb(BMLFOPan2 *This, float depthDb){
	// this function should only be called if This struct was initialized to
	// operate with the depth measured in decibels
	assert(This->type == PANTYPE_QUADRATURE_DB || This->type == PANTYPE_TRIANGLE_CUT_ONLY_OUT_OF_PHASE_DB);
	
	// since this is a cut-only pan effect, the depth must not be a positive number
	assert(depthDb <= 0.0f);
	
	float min = depthDb;
	float max = 0.0;
	if (This->type == PANTYPE_TRIANGLE_CUT_ONLY_OUT_OF_PHASE_DB) max = -depthDb;
	BMLFO_setMinMaxSmoothly(&This->lfo, min, max);
	BMTriangleLFO_setMinMaxSmoothly(&This->TriangleLFO, min, max);
}




/*!
 *BMLFOPan2_free
 */
void BMLFOPan2_free(BMLFOPan2 *This){
	BMLFO_free(&This->lfo);
	BMTriangleLFO_free(&This->TriangleLFO);
	
	free(This->mixControlSignalL);
	free(This->mixControlSignalR);
	free(This->buffer);
	This->mixControlSignalL = NULL;
	This->mixControlSignalR = NULL;
	This->buffer = NULL;
}
