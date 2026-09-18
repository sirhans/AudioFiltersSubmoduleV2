//
//  BMPartitionedConvolver.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 18/9/26.
//  Anyone may use this file without restrictions of any kind.
//

#include "BMPartitionedConvolver.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


static bool BMPC_isPowerOfTwo(size_t x){
	return x != 0 && (x & (x - 1)) == 0;
}

static size_t BMPC_log2(size_t x){
	size_t n = 0;
	while(x > 1){ x >>= 1; n++; }
	return n;
}

static inline void BMPC_splitComplexAt(const float *re, const float *im,
									   size_t offset, DSPSplitComplex *out){
	out->realp = (float*)re + offset;
	out->imagp = (float*)im + offset;
}




/*!
 *BMPartitionedConvolver_init
 */
void BMPartitionedConvolver_init(BMPartitionedConvolver *This,
								 size_t maxIRLength,
								 size_t blockSize){
	assert(BMPC_isPowerOfTwo(blockSize) && blockSize >= 8);
	assert(maxIRLength > 0);

	This->blockSize = blockSize;
	This->fftSize = 2 * blockSize;
	This->log2FFTSize = BMPC_log2(This->fftSize);
	This->numBins = blockSize;
	This->maxPartitions = (maxIRLength + blockSize - 1) / blockSize;
	This->numPartitions = 0;
	This->irLength = 0;

	This->fftSetup = vDSP_create_fftsetup(This->log2FFTSize, kFFTRadix2);

	size_t spectraLength = This->maxPartitions * This->numBins;
	This->irSpectraRe = calloc(spectraLength, sizeof(float));
	This->irSpectraIm = calloc(spectraLength, sizeof(float));
	This->inputSpectraRe = calloc(spectraLength, sizeof(float));
	This->inputSpectraIm = calloc(spectraLength, sizeof(float));

	This->accumulator.realp = calloc(This->numBins, sizeof(float));
	This->accumulator.imagp = calloc(This->numBins, sizeof(float));
	This->inputTimeBuffer = calloc(This->fftSize, sizeof(float));
	This->outputTimeBuffer = calloc(This->fftSize, sizeof(float));
	This->partitionBuffer = calloc(This->fftSize, sizeof(float));

	This->ringIndex = 0;
	This->bufferPosition = 0;
	This->accumulating = false;
}




/*!
 *BMPartitionedConvolver_free
 */
void BMPartitionedConvolver_free(BMPartitionedConvolver *This){
	vDSP_destroy_fftsetup(This->fftSetup);
	This->fftSetup = NULL;

	free(This->irSpectraRe);		This->irSpectraRe = NULL;
	free(This->irSpectraIm);		This->irSpectraIm = NULL;
	free(This->inputSpectraRe);		This->inputSpectraRe = NULL;
	free(This->inputSpectraIm);		This->inputSpectraIm = NULL;
	free(This->accumulator.realp);	This->accumulator.realp = NULL;
	free(This->accumulator.imagp);	This->accumulator.imagp = NULL;
	free(This->inputTimeBuffer);	This->inputTimeBuffer = NULL;
	free(This->outputTimeBuffer);	This->outputTimeBuffer = NULL;
	free(This->partitionBuffer);	This->partitionBuffer = NULL;
}




/*!
 *BMPartitionedConvolver_clearBuffers
 */
void BMPartitionedConvolver_clearBuffers(BMPartitionedConvolver *This){
	size_t spectraLength = This->maxPartitions * This->numBins;
	memset(This->inputSpectraRe, 0, sizeof(float) * spectraLength);
	memset(This->inputSpectraIm, 0, sizeof(float) * spectraLength);
	memset(This->inputTimeBuffer, 0, sizeof(float) * This->fftSize);
	memset(This->outputTimeBuffer, 0, sizeof(float) * This->fftSize);
	This->ringIndex = 0;
	This->bufferPosition = 0;
}




/*!
 *BMPartitionedConvolver_setImpulseResponse
 */
void BMPartitionedConvolver_setImpulseResponse(BMPartitionedConvolver *This,
											   const float *impulseResponse,
											   size_t irLength){
	assert(irLength > 0);
	assert(irLength <= This->maxPartitions * This->blockSize);

	size_t numPartitions = (irLength + This->blockSize - 1) / This->blockSize;

	// the ring of input spectra is indexed modulo numPartitions, so a change
	// in the partition count invalidates the stored history
	if(numPartitions != This->numPartitions){
		This->numPartitions = numPartitions;
		BMPartitionedConvolver_clearBuffers(This);
	}
	This->irLength = irLength;

	// vDSP's forward real FFT is scaled by 2 and its inverse by fftSize, so
	// the product of two forward transforms comes back 4 * fftSize too large.
	// Fold the correction into the stored IR spectra.
	float scale = 1.0f / (4.0f * (float)This->fftSize);

	for(size_t p = 0; p < numPartitions; p++){
		// copy one partition into the first half of a zeroed buffer
		size_t start = p * This->blockSize;
		size_t length = irLength - start;
		if(length > This->blockSize) length = This->blockSize;
		memset(This->partitionBuffer, 0, sizeof(float) * This->fftSize);
		memcpy(This->partitionBuffer, impulseResponse + start, sizeof(float) * length);

		// transform it in place in the IR spectra array
		DSPSplitComplex spectrum;
		BMPC_splitComplexAt(This->irSpectraRe, This->irSpectraIm, p * This->numBins, &spectrum);
		vDSP_ctoz((DSPComplex*)This->partitionBuffer, 2, &spectrum, 1, This->numBins);
		vDSP_fft_zrip(This->fftSetup, &spectrum, 1, This->log2FFTSize, kFFTDirection_Forward);
		vDSP_vsmul(spectrum.realp, 1, &scale, spectrum.realp, 1, This->numBins);
		vDSP_vsmul(spectrum.imagp, 1, &scale, spectrum.imagp, 1, This->numBins);
	}
}




/*!
 *BMPartitionedConvolver_processBlock
 *
 * @abstract Called once per blockSize input samples. Expects inputTimeBuffer
 * to hold [previous block | current block]; leaves the blockSize output
 * samples for the current block in the second half of outputTimeBuffer.
 */
static void BMPartitionedConvolver_processBlock(BMPartitionedConvolver *This){
	size_t numBins = This->numBins;

	// transform the current 2*blockSize input window straight into its ring slot
	DSPSplitComplex inputSpectrum;
	BMPC_splitComplexAt(This->inputSpectraRe, This->inputSpectraIm,
						This->ringIndex * numBins, &inputSpectrum);
	vDSP_ctoz((DSPComplex*)This->inputTimeBuffer, 2, &inputSpectrum, 1, numBins);
	vDSP_fft_zrip(This->fftSetup, &inputSpectrum, 1, This->log2FFTSize, kFFTDirection_Forward);

	// output spectrum = sum over partitions p of (input spectrum p blocks ago) * (IR partition p)
	DSPSplitComplex *acc = &This->accumulator;
	vDSP_vclr(acc->realp, 1, numBins);
	vDSP_vclr(acc->imagp, 1, numBins);
	float dc = 0.0f, nyquist = 0.0f;
	size_t slot = This->ringIndex;
	for(size_t p = 0; p < This->numPartitions; p++){
		DSPSplitComplex x, h;
		BMPC_splitComplexAt(This->inputSpectraRe, This->inputSpectraIm, slot * numBins, &x);
		BMPC_splitComplexAt(This->irSpectraRe, This->irSpectraIm, p * numBins, &h);

		vDSP_zvma(&x, 1, &h, 1, acc, 1, acc, 1, numBins);

		// the packed real FFT keeps the Nyquist term in imagp[0], so the
		// complex multiply above is wrong for bin 0. DC and Nyquist are both
		// real: accumulate them separately and write them in afterwards.
		dc += x.realp[0] * h.realp[0];
		nyquist += x.imagp[0] * h.imagp[0];

		slot = (slot == 0) ? This->numPartitions - 1 : slot - 1;
	}
	acc->realp[0] = dc;
	acc->imagp[0] = nyquist;

	// back to the time domain; the first half is circular wrap-around and is discarded
	vDSP_fft_zrip(This->fftSetup, acc, 1, This->log2FFTSize, kFFTDirection_Inverse);
	vDSP_ztoc(acc, 1, (DSPComplex*)This->outputTimeBuffer, 2, numBins);

	// the current block becomes the previous block of the next window
	memcpy(This->inputTimeBuffer, This->inputTimeBuffer + This->blockSize,
		   sizeof(float) * This->blockSize);

	This->ringIndex = (This->ringIndex + 1) % This->numPartitions;
}




/*!
 *BMPartitionedConvolver_processBuffer
 */
void BMPartitionedConvolver_processBuffer(BMPartitionedConvolver *This,
										  const float *input,
										  float *output,
										  size_t numSamples){
	// no impulse response yet: output silence
	if(This->numPartitions == 0){
		vDSP_vclr(output, 1, numSamples);
		return;
	}

	size_t blockSize = This->blockSize;
	float *inputBlock = This->inputTimeBuffer + blockSize;
	const float *outputBlock = This->outputTimeBuffer + blockSize;

	while(numSamples > 0){
		size_t pos = This->bufferPosition;

		// aligned: a whole block is available at a block boundary, so it can
		// be computed and returned now with no added latency
		if(!This->accumulating && pos == 0 && numSamples >= blockSize){
			memcpy(inputBlock, input, sizeof(float) * blockSize);
			BMPartitionedConvolver_processBlock(This);
			memcpy(output, outputBlock, sizeof(float) * blockSize);
			input += blockSize;
			output += blockSize;
			numSamples -= blockSize;
			continue;
		}

		// unaligned: accumulate the input and return the previous block's
		// output, which delays the signal by blockSize samples. Once this
		// has happened the aligned path stays off so the latency is stable.
		This->accumulating = true;
		size_t n = blockSize - pos;
		if(n > numSamples) n = numSamples;

		// take the input first so in-place processing works
		memcpy(inputBlock + pos, input, sizeof(float) * n);
		memcpy(output, outputBlock + pos, sizeof(float) * n);

		This->bufferPosition = pos + n;
		input += n;
		output += n;
		numSamples -= n;

		if(This->bufferPosition == blockSize){
			BMPartitionedConvolver_processBlock(This);
			This->bufferPosition = 0;
		}
	}
}




size_t BMPartitionedConvolver_latency(const BMPartitionedConvolver *This, size_t hostBufferSize){
	if(!This->accumulating && hostBufferSize > 0 && hostBufferSize % This->blockSize == 0) return 0;
	return This->blockSize;
}


size_t BMPartitionedConvolver_maxIRLength(const BMPartitionedConvolver *This){
	return This->maxPartitions * This->blockSize;
}
