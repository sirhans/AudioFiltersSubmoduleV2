//
//  BMPartitionedConvolver.h
//  AudioFiltersXcodeProject
//
//  Uniformly partitioned FFT convolution (overlap-save with a frequency-
//  domain delay line). Convolves audio with an impulse response of any
//  length at a fixed cost per block, independent of the buffer size the
//  caller uses.
//
//  How it works: the impulse response is cut into partitions of blockSize
//  samples, each zero-padded to 2*blockSize and transformed once at init.
//  Each block of input is transformed once, pushed into a ring of stored
//  input spectra, and the output spectrum is the sum over partitions of
//  (input spectrum p blocks ago) * (IR partition p). One inverse FFT then
//  gives blockSize output samples. Per block that is one forward FFT, one
//  inverse FFT and numPartitions complex multiply-accumulates of blockSize
//  bins, so the cost per sample is O(irLength / blockSize + log blockSize).
//
//  Latency: none when every call to processBuffer is a whole number of
//  blocks (set blockSize to the audio system's buffer size), blockSize
//  samples otherwise. A large blockSize is cheapest for offline rendering.
//  For long impulse responses at a small block size use BMConvolver, which
//  chains several of these with growing partition sizes.
//
//  Created by hans anderson on 18/9/26.
//  Anyone may use this file without restrictions of any kind.
//

#ifndef BMPartitionedConvolver_h
#define BMPartitionedConvolver_h

#include <stdio.h>
#include <stdbool.h>
#include <Accelerate/Accelerate.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BMPartitionedConvolver {
	size_t blockSize;		// samples per partition
	size_t fftSize;			// 2 * blockSize
	size_t log2FFTSize;
	size_t numBins;			// blockSize; packed real FFT has fftSize/2 complex bins
	size_t maxPartitions;	// allocated
	size_t numPartitions;	// in use for the current impulse response
	size_t irLength;

	FFTSetup fftSetup;

	// impulse response spectra, numPartitions blocks of numBins, contiguous
	float *irSpectraRe, *irSpectraIm;

	// frequency-domain delay line: a ring of the last numPartitions input
	// spectra, same layout as the IR spectra
	float *inputSpectraRe, *inputSpectraIm;
	size_t ringIndex;		// slot that receives the next input spectrum

	// working memory
	DSPSplitComplex accumulator;	// numBins
	float *inputTimeBuffer;		// fftSize: [previous block | current block]
	float *outputTimeBuffer;	// fftSize: second half is the valid output
	float *partitionBuffer;		// fftSize: for transforming the IR
	size_t bufferPosition;		// samples of the current block received so far
	bool accumulating;			// set for good by the first call that is not a whole number of blocks
} BMPartitionedConvolver;




/*!
 *BMPartitionedConvolver_init
 *
 * @param This pointer to an uninitialised struct
 * @param maxIRLength the longest impulse response that can be set later, in samples
 * @param blockSize partition length in samples; a power of two >= 8. Use the audio system buffer size for realtime processing (see processBuffer for the latency rule). Cost per sample falls as blockSize grows, so use a large one for offline rendering.
 */
void BMPartitionedConvolver_init(BMPartitionedConvolver *This,
								 size_t maxIRLength,
								 size_t blockSize);


/*!
 *BMPartitionedConvolver_free
 */
void BMPartitionedConvolver_free(BMPartitionedConvolver *This);


/*!
 *BMPartitionedConvolver_setImpulseResponse
 *
 * @abstract Transform and store an impulse response. Until this is called the convolver outputs silence. Not safe to call while another thread is processing audio. The stored input history is kept when the new impulse response has the same number of partitions as the old one, so a swap between two impulse responses of similar length does not interrupt the sound; otherwise the history is cleared.
 *
 * @param This pointer to an initialised struct
 * @param impulseResponse array of irLength samples
 * @param irLength in [1, maxIRLength]
 */
void BMPartitionedConvolver_setImpulseResponse(BMPartitionedConvolver *This,
											   const float *impulseResponse,
											   size_t irLength);


/*!
 *BMPartitionedConvolver_processBuffer
 *
 * @abstract Convolve input with the impulse response. Any numSamples is accepted. In-place processing (input == output) is allowed.
 *
 * While every call passes a whole number of blocks, each block is computed and returned in the same call and the output lines up with a direct convolution. The first call that does not switches the instance permanently to accumulating input into blocks, which delays the output by blockSize samples; that one switch repeats blockSize samples of output. So a host that always sends whole blocks gets no added latency, and a host that sometimes sends odd sizes gets a fixed latency of blockSize after its first odd call. clearBuffers does not reset this; init does.
 *
 * @param This pointer to an initialised struct
 * @param input array of numSamples
 * @param output array of numSamples
 * @param numSamples any length
 */
void BMPartitionedConvolver_processBuffer(BMPartitionedConvolver *This,
										  const float *input,
										  float *output,
										  size_t numSamples);


/*!
 *BMPartitionedConvolver_clearBuffers
 *
 * @abstract Clear the input history and any partially received block so the next output starts from silence. The impulse response is kept.
 */
void BMPartitionedConvolver_clearBuffers(BMPartitionedConvolver *This);


/*!
 *BMPartitionedConvolver_latency
 *
 * @param This pointer to an initialised struct
 * @param hostBufferSize the number of samples the caller passes to processBuffer on every call
 * @returns the delay of the output relative to direct convolution, in samples: 0 if hostBufferSize is a multiple of blockSize and no odd-sized call has been made, otherwise blockSize
 */
size_t BMPartitionedConvolver_latency(const BMPartitionedConvolver *This, size_t hostBufferSize);


/*!
 *BMPartitionedConvolver_maxIRLength
 *
 * @returns the longest impulse response this instance can hold, in samples
 */
size_t BMPartitionedConvolver_maxIRLength(const BMPartitionedConvolver *This);


#ifdef __cplusplus
}
#endif

#endif /* BMPartitionedConvolver_h */
