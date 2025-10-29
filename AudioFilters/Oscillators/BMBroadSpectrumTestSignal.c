//
//  BMBroadSpectrumTestSignal.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 7/23/25.
//  We release this file into the public domain.
//

#include "BMBroadSpectrumTestSignal.h"
#include "BMUnitConversion.h"
#include <stdlib.h>
#include "../AudioFilter.h"
#include "Constants.h"


void BMBroadSpectrumTestSignal_init(BMBroadSpectrumTestSignal *This,
									float minFrequency,
									float maxFrequency,
									size_t numOscillators,
									bool randomPhase,
									float sampleRate){
	This->outputBuffer = malloc(sizeof(double)*BM_BUFFER_CHUNK_SIZE);
	
	double *magnitudes = malloc(sizeof(double)*numOscillators);
	double *phases = malloc(sizeof(double)*numOscillators);
	double *frequencies = malloc(sizeof(double)*numOscillators);
	
	// set all the magnitudes to keep the volume under clipping
	double mag = BM_DB_TO_GAIN(-11.0)/sqrt(numOscillators);
	vDSP_vfillD(&mag, magnitudes, 1, numOscillators);
	
	// set all the phases to 0
	vDSP_vclrD(phases, 1, numOscillators);
	
	// if the calling function wants random phase, randomise them
	if(randomPhase){
		for(size_t i=0; i<numOscillators; i++){
			uint32_t randInt = arc4random();
			
			phases[i] = (double)((long double)randInt / (long double)INT_MAX);
			
			phases[i] *= 2.0 * M_PI;
		}
	}
	
	// set the frequencies
	frequencies[0] = minFrequency;
	for(size_t i=1; i<numOscillators; i++){
		float fraction = (float)i / (float)(numOscillators-1);
		frequencies[i] = (double)BMLogInterp(minFrequency, maxFrequency, fraction);
	}
	
	BMOscillatorArray_init(&This->array, magnitudes, phases, frequencies, sampleRate, numOscillators);
	
	free(magnitudes);
	free(phases);
	free(frequencies);
}

void BMBroadSpectrumTestSignal_free(BMBroadSpectrumTestSignal *This){
	BMOscillatorArray_free(&This->array);
	free(This->outputBuffer);
	This->outputBuffer = NULL;
}

void BMBroadSpectrumTestSignal_processMono(BMBroadSpectrumTestSignal *This,
										   float *output,
										   size_t numSamples){
	size_t samplesProcessed = 0;
	while(samplesProcessed < numSamples){
		// find out how many samples we can process this time through the loop
		size_t samplesProcessing = BM_MIN(numSamples-samplesProcessed, BM_BUFFER_CHUNK_SIZE);
		
		// process into the output buffer
		for(size_t i=0; i<samplesProcessing; i++){
			BMOscillatorArray_processSample(&This->array, This->outputBuffer + i);
		}
		
		// convert the output buffer to float and write to output
		vDSP_vdpsp(This->outputBuffer, 1, output + samplesProcessed, 1, samplesProcessing);
		
		samplesProcessed += samplesProcessing;
	}
}

void BMBroadSpectrumTestSignal_processStereo(BMBroadSpectrumTestSignal *This,
										   float *outputL, float *outputR,
											 size_t numSamples){
	BMBroadSpectrumTestSignal_processMono(This, outputL, numSamples);
	memcpy(outputR, outputL, sizeof(float) * numSamples);
}
