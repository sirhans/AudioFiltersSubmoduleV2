//
//  BMConvolver.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 18/9/26.
//  Anyone may use this file without restrictions of any kind.
//

#include "BMConvolver.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>




#pragma mark - direct stage

static void BMConvolverDirectStage_init(BMConvolverDirectStage *This, size_t irCapacity, size_t blockSize){
	This->irCapacity = irCapacity;
	This->irLength = 0;
	This->blockSize = blockSize;
	This->ir = calloc(irCapacity, sizeof(float));
	This->history = calloc(irCapacity - 1 + blockSize, sizeof(float));
}

static void BMConvolverDirectStage_free(BMConvolverDirectStage *This){
	free(This->ir);			This->ir = NULL;
	free(This->history);	This->history = NULL;
}

static void BMConvolverDirectStage_clearBuffers(BMConvolverDirectStage *This){
	memset(This->history, 0, sizeof(float) * (This->irCapacity - 1 + This->blockSize));
}

static void BMConvolverDirectStage_setImpulseResponse(BMConvolverDirectStage *This,
													  const float *ir, size_t irLength){
	assert(irLength > 0 && irLength <= This->irCapacity);
	memcpy(This->ir, ir, sizeof(float) * irLength);
	This->irLength = irLength;
}

/*!
 *BMConvolverDirectStage_processBlock
 *
 * @abstract Exactly blockSize samples in and out, no delay.
 */
static void BMConvolverDirectStage_processBlock(BMConvolverDirectStage *This,
												const float *input, float *output){
	size_t past = This->irCapacity - 1;
	size_t L = This->irLength;

	// append the new block after the stored past samples
	memcpy(This->history + past, input, sizeof(float) * This->blockSize);

	// vDSP_conv with the kernel read backwards from its last element is a
	// convolution; it reads L - 1 samples before each output sample
	vDSP_conv(This->history + past - (L - 1), 1,
			  This->ir + (L - 1), -1,
			  output, 1,
			  This->blockSize, L);

	// slide the window on by one block
	memmove(This->history, This->history + This->blockSize, sizeof(float) * past);
}




#pragma mark - BMConvolver

/*!
 *BMConvolver_init
 */
void BMConvolver_init(BMConvolver *This, size_t maxIRLength, size_t bufferSize){
	assert(maxIRLength > 0);
	assert(bufferSize > 0);

	// the largest power of two that divides the buffer size: every call of
	// bufferSize samples is then a whole number of blocks
	size_t blockSize = bufferSize & (~bufferSize + 1);

	This->hostBufferSize = bufferSize;
	This->blockSize = blockSize;
	This->maxIRLength = maxIRLength;
	This->irLength = 0;
	This->numActiveStages = 0;
	This->bufferPosition = 0;
	This->accumulating = false;
	This->hasDirectStage = blockSize < BM_CONVOLVER_MIN_FFT_PARTITION;

	// lay out the stages. Stage k has partition size P_k = blockSize * ratio^k
	// and covers [P_k, P_{k+1}) of the impulse response, except stage 0 which
	// starts at 0 and the last stage which runs to maxIRLength.
	size_t start = 0;
	size_t partitionSize = blockSize;
	This->numStages = 0;
	while(start < maxIRLength){
		size_t k = This->numStages;
		assert(k < BM_CONVOLVER_MAX_STAGES);

		size_t nextPartitionSize = partitionSize * BM_CONVOLVER_STAGE_RATIO;
		size_t end = nextPartitionSize;
		bool isLast = (nextPartitionSize > BM_CONVOLVER_MAX_PARTITION_SIZE)
					|| (k + 1 == BM_CONVOLVER_MAX_STAGES)
					|| (end >= maxIRLength);
		if(isLast) end = maxIRLength;

		This->stageStart[k] = start;
		This->stageEnd[k] = end;
		if(k == 0 && This->hasDirectStage)
			BMConvolverDirectStage_init(&This->direct, end - start, partitionSize);
		else
			BMPartitionedConvolver_init(&This->stages[k], end - start, partitionSize);
		This->numStages++;

		start = end;
		partitionSize = nextPartitionSize;
	}

	This->inputBlock = calloc(blockSize, sizeof(float));
	This->outputBlock = calloc(blockSize, sizeof(float));
	This->stageOutput = calloc(blockSize, sizeof(float));
}




/*!
 *BMConvolver_free
 */
void BMConvolver_free(BMConvolver *This){
	for(size_t k = 0; k < This->numStages; k++){
		if(k == 0 && This->hasDirectStage) BMConvolverDirectStage_free(&This->direct);
		else BMPartitionedConvolver_free(&This->stages[k]);
	}
	This->numStages = 0;

	free(This->inputBlock);		This->inputBlock = NULL;
	free(This->outputBlock);	This->outputBlock = NULL;
	free(This->stageOutput);	This->stageOutput = NULL;
}




static void BMConvolver_clearStage(BMConvolver *This, size_t k){
	if(k == 0 && This->hasDirectStage) BMConvolverDirectStage_clearBuffers(&This->direct);
	else BMPartitionedConvolver_clearBuffers(&This->stages[k]);
}


/*!
 *BMConvolver_clearBuffers
 */
void BMConvolver_clearBuffers(BMConvolver *This){
	for(size_t k = 0; k < This->numStages; k++) BMConvolver_clearStage(This, k);
	memset(This->inputBlock, 0, sizeof(float) * This->blockSize);
	memset(This->outputBlock, 0, sizeof(float) * This->blockSize);
	This->bufferPosition = 0;
}




/*!
 *BMConvolver_setImpulseResponse
 */
void BMConvolver_setImpulseResponse(BMConvolver *This,
									const float *impulseResponse,
									size_t irLength){
	assert(irLength > 0 && irLength <= This->maxIRLength);

	size_t previouslyActive = This->numActiveStages;
	This->irLength = irLength;
	This->numActiveStages = 0;

	for(size_t k = 0; k < This->numStages; k++){
		size_t start = This->stageStart[k];
		if(start >= irLength) break;

		size_t end = This->stageEnd[k];
		if(end > irLength) end = irLength;

		// a stage that was idle has stale input history
		if(k >= previouslyActive) BMConvolver_clearStage(This, k);

		if(k == 0 && This->hasDirectStage)
			BMConvolverDirectStage_setImpulseResponse(&This->direct, impulseResponse, end);
		else
			BMPartitionedConvolver_setImpulseResponse(&This->stages[k],
													  impulseResponse + start,
													  end - start);
		This->numActiveStages++;
	}
}




/*!
 *BMConvolver_processBlock
 *
 * @abstract Convolve exactly one block. Stage 0 is called at a block boundary
 * with a whole block, so it returns its result immediately. Each later stage
 * accumulates blocks until it has a partition, and returns its result
 * partitionSize samples late, which is exactly the offset of its section of
 * the impulse response.
 */
static void BMConvolver_processBlock(BMConvolver *This, const float *input, float *output){
	size_t blockSize = This->blockSize;

	if(This->numActiveStages == 0){
		vDSP_vclr(output, 1, blockSize);
		return;
	}

	if(This->hasDirectStage)
		BMConvolverDirectStage_processBlock(&This->direct, input, output);
	else
		BMPartitionedConvolver_processBuffer(&This->stages[0], input, output, blockSize);

	for(size_t k = 1; k < This->numActiveStages; k++){
		BMPartitionedConvolver_processBuffer(&This->stages[k], input, This->stageOutput, blockSize);
		vDSP_vadd(output, 1, This->stageOutput, 1, output, 1, blockSize);
	}
}




/*!
 *BMConvolver_processBuffer
 */
void BMConvolver_processBuffer(BMConvolver *This,
							   const float *input,
							   float *output,
							   size_t numSamples){
	size_t blockSize = This->blockSize;

	while(numSamples > 0){
		size_t pos = This->bufferPosition;

		// aligned: process a whole block now with no added latency. The input
		// is copied first because the later stages need it after stage 0 has
		// written the output, which may be the same memory.
		if(!This->accumulating && pos == 0 && numSamples >= blockSize){
			memcpy(This->inputBlock, input, sizeof(float) * blockSize);
			BMConvolver_processBlock(This, This->inputBlock, output);
			input += blockSize;
			output += blockSize;
			numSamples -= blockSize;
			continue;
		}

		// unaligned: accumulate a block and return the previous block's
		// output. Once this has happened the aligned path stays off so the
		// latency is stable.
		This->accumulating = true;
		size_t n = blockSize - pos;
		if(n > numSamples) n = numSamples;

		memcpy(This->inputBlock + pos, input, sizeof(float) * n);
		memcpy(output, This->outputBlock + pos, sizeof(float) * n);

		This->bufferPosition = pos + n;
		input += n;
		output += n;
		numSamples -= n;

		if(This->bufferPosition == blockSize){
			BMConvolver_processBlock(This, This->inputBlock, This->outputBlock);
			This->bufferPosition = 0;
		}
	}
}




size_t BMConvolver_latency(const BMConvolver *This, size_t hostBufferSize){
	if(!This->accumulating && hostBufferSize > 0 && hostBufferSize % This->blockSize == 0) return 0;
	return This->blockSize;
}


size_t BMConvolver_maxIRLength(const BMConvolver *This){
	return This->maxIRLength;
}
