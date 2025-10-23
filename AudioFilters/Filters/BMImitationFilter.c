//
//  BMImitationFilter.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 5/12/25.
//  We release this file into the public domain with no restrictions.
//

#include "BMImitationFilter.h"
#include "Constants.h"
#include <stdlib.h>
#include "../AudioFilter.h"


/*!
 *BMImitationFilter_init
 *
 * @param This pointer to an uninitialized struct
 * @param filterOrder higher numbers yield more precise filter curves, but beware overfitting! If the unknown filter is simple then the filter order should be low. If you don't know, try starting with 16.
 * @param convergenceRate in (0,1). 0 means the filter doesn't adapt at all. 1 means it completely resets with every sample.
 */
void BMImitationFilter_init(BMImitationFilter *This,
							size_t filterOrder,
							float convergenceRate){
	This->filterOrder = filterOrder;
	This->h = malloc(sizeof(float) * filterOrder);
	memset(This->h, 0, sizeof(float) * filterOrder);
	This->mu = convergenceRate;
	TPCircularBufferInit(&This->buffer1, sizeof(float)*(BM_BUFFER_CHUNK_SIZE + (uint32_t)filterOrder));
	TPCircularBufferInit(&This->buffer2, sizeof(float)*(BM_BUFFER_CHUNK_SIZE + (uint32_t)filterOrder));
	
	// push some samples into the circular buffer so we can start using the filter immediately
	//
	// buffer 1
	uint32_t availableBytes;
	float* head = TPCircularBufferHead(&This->buffer1, &availableBytes);
	uint32_t bytesWriting = sizeof(float) * (uint32_t)(filterOrder -1);
	assert(availableBytes >= bytesWriting);
	memset(head, 0, bytesWriting);
	TPCircularBufferProduce(&This->buffer1, bytesWriting);
	//
	// buffer 2
	head = TPCircularBufferHead(&This->buffer2, &availableBytes);
	bytesWriting = sizeof(float) * (uint32_t)(filterOrder -1);
	assert(availableBytes >= bytesWriting);
	memset(head, 0, bytesWriting);
	TPCircularBufferProduce(&This->buffer2, bytesWriting);
}




// process buffer of length <= buffer_chunk_size
void BMImitationFilter_processShortBuffer(BMImitationFilter *This,
										  float *unfilteredReference,
										  float *filteredReference,
										  float *input,
										  float *output,
										  float *error,
										  size_t numSamples){
	// get write pointers
	uint32_t availableBytes;
	float* writePointer1 = TPCircularBufferHead(&This->buffer1, &availableBytes);
	float* writePointer2 = TPCircularBufferHead(&This->buffer2, &availableBytes);
	
	// confirm that there's enough space to write into the buffers
	assert(availableBytes >= numSamples * sizeof(float));
	
	// copy into the circular buffers
	memcpy(writePointer1, unfilteredReference, sizeof(float) * numSamples);
	memcpy(writePointer2, input, sizeof(float) * numSamples);
	
	// mark the buffers written
	TPCircularBufferProduce(&This->buffer1, (uint32_t)numSamples * (uint32_t)sizeof(float));
	TPCircularBufferProduce(&This->buffer2, (uint32_t)numSamples * (uint32_t)sizeof(float));
	
	
	// get read pointers for the same two buffers.
	// Note that these pointers are behind where the write pointers because
	// it's delayed by filterOrder-1 samples to allow us to read samples from
	// the past to process the FIR filter.
	float* readPointer1 = TPCircularBufferTail(&This->buffer1, &availableBytes);
	float* readPointer2 = TPCircularBufferTail(&This->buffer2, &availableBytes);
	
	// process the filter
	for(size_t i=0; i<numSamples; i++){
		// give nice names to the read pointers
		float *X = readPointer1 + i;
		float *W = readPointer2 + i;
		
		assert(!isnan(This->h[0]));
		
		// Find the vector dot product of the filter kernel and the last
		// filterOrder samples, ending at the copy of the ith sample from the
		// "unfiltered" input buffer.
		// The result, hX, is an estimate of filtered[i].
		//
		// hX = h . X
		float hX;
		vDSP_dotpr(X, 1, This->h, 1, &hX, This->filterOrder);
		
		assert(!isinf(This->h[0]));
		assert(!isinf(hX));
		
		// calculate e, the error in the estimation of filtered[i].
		float e = filteredReference[i] - hX;
		
		assert(!isinf(e));
		
		// Write the error to the error buffer. This is used for debugging.
		error[i] = e;
		
		// if the error is large, limit it to prevent the filter from becoming
		// unstable
		float maxE = 0.0005;
		if (fabsf(e) > maxE) {
			if (e < 0.0) e = -maxE;
			else e = maxE;
		}
		
		// use the error and the last filterOrder values of the input to update
		// our filter coefficients.
		//
		// h = h + (mu * e * X)
		float mu_e = This->mu * e;
		vDSP_vsma(X, 1, &mu_e, This->h, 1, This->h, 1, This->filterOrder);
		
		// filter the input and write one sample of output
		vDSP_dotpr(W, 1, This->h, 1, output + i, This->filterOrder);
		
	}
	
	// mark the buffers read
	TPCircularBufferConsume(&This->buffer1, (uint32_t)numSamples * (uint32_t)sizeof(float));
	TPCircularBufferConsume(&This->buffer2, (uint32_t)numSamples * (uint32_t)sizeof(float));
}

 


// process input arrays of any length
void BMImitationFilter_process(BMImitationFilter *This,
							   float *unfilteredReference,
							   float *filteredReference,
							   float *input,
							   float *output,
							   float *error,
							   size_t numSamples){
	size_t samplesRemaining = numSamples;
	size_t offset = 0;
	while(samplesRemaining > 0) {
		// process the signal in chunks of size <= BM_BUFFER_CHUNK_SIZE
		size_t samplesProcessing = BM_MIN(BM_BUFFER_CHUNK_SIZE, samplesRemaining);
		
		BMImitationFilter_processShortBuffer(This,
											 unfilteredReference + offset,
											 filteredReference + offset,
											 input + offset,
											 output + offset,
											 error + offset,
											 samplesProcessing);
		
		samplesRemaining -= samplesProcessing;
		offset += samplesProcessing;
	}
}






void BMImitationFilter_free(BMImitationFilter *This){
	free(This->h);
	This->h = NULL;
	TPCircularBufferCleanup(&This->buffer1);
	TPCircularBufferCleanup(&This->buffer2);
}
