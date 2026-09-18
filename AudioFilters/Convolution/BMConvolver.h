//
//  BMConvolver.h
//  AudioFiltersXcodeProject
//
//  Non-uniformly partitioned FFT convolution for impulse responses of any
//  length (speaker cabinets, reverbs) at the latency of the audio buffer.
//
//  This is Gardner's scheme built from BMPartitionedConvolver stages. The
//  first stage uses partitions of P0 samples and covers the start of the
//  impulse response; each later stage uses partitions BM_CONVOLVER_STAGE_RATIO
//  times longer and covers the next section. A stage with partition size P
//  is placed so that its section starts P samples into the impulse response,
//  which is exactly the delay it incurs by accumulating P samples before its
//  first transform, so the stage outputs line up with no extra delay lines.
//  Partition sizes stop growing at BM_CONVOLVER_MAX_PARTITION_SIZE so that
//  the largest transform stays a small fraction of one audio buffer.
//
//  P0 is the largest power of two that divides the audio buffer size, so a
//  power-of-two buffer gets P0 = bufferSize, 480 gets 32, and an odd size
//  gets 1. When P0 is below BM_CONVOLVER_MIN_FFT_PARTITION the first stage
//  is a direct time-domain FIR instead of an FFT stage, so buffer sizes down
//  to 1 sample work with no added latency.
//
//  Cost per sample is roughly (BM_CONVOLVER_STAGE_RATIO * numStages +
//  tailLength / largestPartition) complex multiply-adds plus the transforms,
//  which for a multi-second reverb at a 64-sample buffer is well under one
//  percent of one core. All work is done on the calling thread; the load is
//  heavier on blocks where a large stage completes a partition, but every
//  block stays far inside its real-time budget.
//
//  Created by hans anderson on 18/9/26.
//  Anyone may use this file without restrictions of any kind.
//

#ifndef BMConvolver_h
#define BMConvolver_h

#include <stdio.h>
#include "BMPartitionedConvolver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BM_CONVOLVER_MAX_STAGES 8
#define BM_CONVOLVER_STAGE_RATIO 8
#define BM_CONVOLVER_MIN_FFT_PARTITION 8
#define BM_CONVOLVER_MAX_PARTITION_SIZE 8192


/*!
 *BMConvolverDirectStage
 *
 * @abstract Time-domain FIR for the head of the impulse response when the
 * first partition is too small for an FFT to be worthwhile.
 */
typedef struct BMConvolverDirectStage {
	float *ir;			// irCapacity
	float *history;		// irCapacity - 1 + blockSize: [past samples | current block]
	size_t irCapacity, irLength, blockSize;
} BMConvolverDirectStage;


typedef struct BMConvolver {
	size_t hostBufferSize;	// what init was given
	size_t blockSize;		// P0: the block the container works in
	size_t maxIRLength;
	size_t irLength;

	size_t numStages;		// allocated, including the direct stage if any
	size_t numActiveStages;	// stages that hold part of the current impulse response
	bool hasDirectStage;	// stage 0 is `direct` rather than `stages[0]`
	BMConvolverDirectStage direct;
	BMPartitionedConvolver stages[BM_CONVOLVER_MAX_STAGES];
	size_t stageStart[BM_CONVOLVER_MAX_STAGES];	// first IR sample each stage covers
	size_t stageEnd[BM_CONVOLVER_MAX_STAGES];	// one past the last IR sample each stage can cover

	// block accumulation for callers whose buffers are not whole blocks
	float *inputBlock, *outputBlock, *stageOutput;
	size_t bufferPosition;
	bool accumulating;		// set for good by the first call that is not a whole number of blocks
} BMConvolver;




/*!
 *BMConvolver_init
 *
 * @param This pointer to an uninitialised struct
 * @param maxIRLength the longest impulse response that can be set later, in samples
 * @param bufferSize the number of samples the audio system passes per call; any size >= 1. For offline rendering, larger is cheaper.
 */
void BMConvolver_init(BMConvolver *This, size_t maxIRLength, size_t bufferSize);


/*!
 *BMConvolver_free
 */
void BMConvolver_free(BMConvolver *This);


/*!
 *BMConvolver_setImpulseResponse
 *
 * @abstract Transform and store an impulse response. Until this is called the convolver outputs silence. Not safe to call while another thread is processing audio.
 *
 * @param This pointer to an initialised struct
 * @param impulseResponse array of irLength samples
 * @param irLength in [1, maxIRLength]
 */
void BMConvolver_setImpulseResponse(BMConvolver *This,
									const float *impulseResponse,
									size_t irLength);


/*!
 *BMConvolver_processBuffer
 *
 * @abstract Convolve input with the impulse response. Any numSamples is accepted. In-place processing (input == output) is allowed.
 *
 * While every call passes a whole number of blocks (any multiple of the bufferSize given to init qualifies), the output lines up with a direct convolution. The first call that does not switches the instance permanently to accumulating input into blocks, which delays the output by blockSize samples; that one switch repeats blockSize samples of output. clearBuffers does not reset this; init does.
 *
 * @param This pointer to an initialised struct
 * @param input array of numSamples
 * @param output array of numSamples
 * @param numSamples any length
 */
void BMConvolver_processBuffer(BMConvolver *This,
							   const float *input,
							   float *output,
							   size_t numSamples);


/*!
 *BMConvolver_clearBuffers
 *
 * @abstract Clear the input history so the next output starts from silence. The impulse response is kept.
 */
void BMConvolver_clearBuffers(BMConvolver *This);


/*!
 *BMConvolver_latency
 *
 * @param This pointer to an initialised struct
 * @param hostBufferSize the number of samples the caller passes to processBuffer on every call
 * @returns the delay of the output relative to direct convolution, in samples: 0 if hostBufferSize is a multiple of blockSize and no odd-sized call has been made, otherwise blockSize
 */
size_t BMConvolver_latency(const BMConvolver *This, size_t hostBufferSize);


/*!
 *BMConvolver_maxIRLength
 */
size_t BMConvolver_maxIRLength(const BMConvolver *This);


#ifdef __cplusplus
}
#endif

#endif /* BMConvolver_h */
