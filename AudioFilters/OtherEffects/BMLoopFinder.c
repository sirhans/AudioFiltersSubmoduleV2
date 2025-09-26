//
//  BMLoopFinder.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 9/26/25.
//  We hereby release this code into the public domain.
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <stdbool.h>
#include "BMLoopFinder.h"
#include "BMIntegerMath.h"
#include "BMSpectrum.h"
#include "Constants.h"

/*!
 *loopQualityEval
 *
 * This function measures the noise, in decibels from a set of loop points.
 */
float measureLoopNoise(float *testBuffer,
						 float *loopStartInAudioBuffer,
						 size_t loopLength,
						 size_t waveLength,
						 float *A,
					     float *B,
					     float *C,
					     float *AWindow,
					     float *BWindow,
					     float *CWindow,
					     size_t fftSize,
					     BMSpectrum *spectrum){
	
	// the loop must be at least two wavelengths long
	assert(loopLength >= waveLength * 2);
	
	// copy the loop from the audio buffer into the test buffer 2 times
	for(size_t n=0; n < 2; n++)
		memcpy(testBuffer + (n * sizeof(float) * loopLength), loopStartInAudioBuffer, sizeof(float)*loopLength);
	
	// when we take the FFT to get the spectrum, we don't want to apply the default window because we have a special one for this.
	bool applyWindow = false;
	
	// A: get the spectrum at the beginning of the loop
	//    1. apply the window function
	vDSP_vmul(AWindow, 1, testBuffer, 1, A, 1, fftSize);
	//    2. take the abs(fft)
	BMSpectrum_processDataBasic(spectrum, A, A, applyWindow, fftSize);
	
	// B: get the spectrum at the end of the loop
	vDSP_vmul(BWindow, 1, testBuffer + loopLength - (fftSize / 2), 1, B, 1, fftSize);
	BMSpectrum_processDataBasic(spectrum, B, B, applyWindow, fftSize);
	
	// C: get the spectrum in the region centred around the loop point
	vDSP_vmul(BWindow, 1, testBuffer + loopLength - fftSize, 1, C, 1, fftSize);
	BMSpectrum_processDataBasic(spectrum, C, C, applyWindow, fftSize);
	
	// Find the distance between the spectra before and across the loop point:
	float beforeDistance;
	vDSP_distancesq(A, 1, B, 1, &beforeDistance, fftSize/2);
	beforeDistance = sqrtf(beforeDistance);
	
	// Find the difference between the spectra across and after the loop point:
	float afterDistance;
	vDSP_distancesq(C, 1, B, 1, &afterDistance, fftSize/2);
	afterDistance = sqrtf(afterDistance);
	
	// find the maximum of the two distances
	float maxDistance = BM_MAX(beforeDistance, afterDistance);
	
	// normalize so that the same quality setting works at any frequency
	float maxDistanceNormalized = maxDistance / sqrt(waveLength);
	
	// convert the distance to decibels
	float loopNoise = BM_GAIN_TO_DB(maxDistanceNormalized);
	
	return loopNoise;
}
					  




void BMLoopFinder_FreeBuffers(float *ltb,
						 	  float *A, float *B, float *C,
							  float *AW, float *BW, float *CW){
	free(ltb);
	free(A);
	free(B);
	free(C);
	free(AW);
	free(BW);
	free(CW);
}





bmLoopPoints BMFindLoop(float *audioBuffer,
						float sampleRate,
						float frequency,
						size_t audioBufferLength,
						size_t minLoopLength,
						size_t maxLoopLength,
						float loopNoiseTargetDb){
	
	// allocate memory for a temp buffer for testing loops to see how smooth they are
	float *loopTestBuffer = malloc(sizeof(float) * audioBufferLength * 2);
	
	// find the wavelength corresponding to the frequency of the tone
	size_t waveLength = (size_t)(sampleRate / frequency);
	
	// how long is the fft size needed to find the spectrum at this wavelength?
	// it should be at least twice the wavelength
	size_t fftSize = nextPowOf2((uint32_t) (2 * waveLength));
	
	// allocate memory for calculating the spectrum of the audio before the loop
	// point, across the loop point, and after the loop point
	float *A = malloc(sizeof(float) * fftSize);
	float *B = malloc(sizeof(float) * fftSize);
	float *C = malloc(sizeof(float) * fftSize);
	float *AWindow = malloc(sizeof(float) * fftSize);
	float *BWindow = malloc(sizeof(float) * fftSize);
	float *CWindow = malloc(sizeof(float) * fftSize);
	
	// prepare three windows for taking the abs fft after, across, and before the loop point
	//
	// A is after the loop point, in other words at the start of the loop
	memset(AWindow, 0, sizeof(float) * fftSize);
	float beta = 15.0;
	BMFFT_generateKaiserCoefficients(AWindow, beta, 2 * waveLength);
	//
	// B is centred around the loop point
	// we calculate how to shift the window to the centre of the FFT
	size_t shiftToCentre = (fftSize - (waveLength * 2)) / 2;
	memset(BWindow, 0, sizeof(float) * fftSize);
	memcpy(BWindow + shiftToCentre, AWindow, sizeof(float) * waveLength * 2);
	//
	// C is before the loop point, in other words at the end of the loop
	size_t shiftToEnd = fftSize - (waveLength * 2);
	memset(CWindow, 0, sizeof(float) * fftSize);
	memcpy(CWindow + shiftToEnd, AWindow, sizeof(float) * waveLength * 2);
	
	// init a struct for finding the magnitude spectrum
	BMSpectrum spectrum;
	BMSpectrum_initWithLength(&spectrum, fftSize);
	
	// the loop length must be shorter than the buffer length
	assert(maxLoopLength <= audioBufferLength);
	
	// the loop length must be longer than twice the wavelength so that we can
	// clearly measure the spectrum
	assert(minLoopLength >= waveLength * 2);
	
	// so far the best loop we've found at length = loopLength has quality of 0
	// because we haven't started searching yet.
	float lowestLoopNoiseFound = FLT_MAX;
	float lowestNoiseLoopStartIndex = 0;
	float lowestNoiseLoopLength = 0;
		
	// Starting with the longest loop,
	// for every loop of length loopLength in [minLoopLength, maxLoopLength]...
	for (size_t loopLength = maxLoopLength; loopLength >= minLoopLength; loopLength--){
		
		// Starting at the end of the buffer, find the best loop of length loopLength,
		// for every startSample in [0, numSamples - loopLength]...
		for (size_t startSample = audioBufferLength - loopLength; startSample >= 0; startSample--){
			
			// measure and record the quality of the loop beginning at startSample
			float loopNoise = measureLoopNoise(loopTestBuffer,
											   audioBuffer + startSample,
											   loopLength,
											   waveLength,
											   A, B, C,
											   AWindow, BWindow, CWindow,
											   fftSize,
											   &spectrum);
			
			// if this loop is the best one we've found so far
			if (loopNoise < lowestLoopNoiseFound) {
				
				// if the noise of this loop is below the target level, stop searching and return this result immediately
				if (loopNoise < loopNoiseTargetDb){
					BMLoopFinder_FreeBuffers(loopTestBuffer, A, B, C, AWindow, BWindow, CWindow);
					bmLoopPoints lp = {startSample, startSample + loopLength - 1, loopNoise};
					return lp;
				}
				
				// Or if this loop doesn't meet the noise target we're looking for,
				// take note of its noise level, loop length, and starting sample index in
				// case we don't find a better one later on.
				lowestLoopNoiseFound = loopNoise;
				lowestNoiseLoopStartIndex = startSample;
				lowestNoiseLoopLength = loopLength;
			}
		}
	}
	
	BMLoopFinder_FreeBuffers(loopTestBuffer, A, B, C, AWindow, BWindow, CWindow);
	
	// if we reach this point, we didn't find a loop that met our quality goal,
	// so we return the best quality loop we have
	bmLoopPoints lp = {lowestNoiseLoopStartIndex, lowestNoiseLoopStartIndex + lowestNoiseLoopLength - 1, lowestLoopNoiseFound};
	return lp;
}
