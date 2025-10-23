//
//  BMLongFade.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 7/23/25.
//  We hereby release this file into the public domain.
//

#include "BMLongFade.h"
#include "Constants.h"
#include "stdlib.h"
#include "BMUnitConversion.h"
#include "../AudioFilter.h"


void BMLongFade_init(BMLongFade *This, float startGainDB, float endGainDB, float lengthInSeconds, float sampleRate){
	This->startGainDB = startGainDB;
	This->endGainDB = endGainDB;
	This->gainDB = startGainDB;
	
	This->samplesProcessed = 0;
	This->lengthSamples = lengthInSeconds * sampleRate;
	
	This->buffer = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE);
}

/*!
 *BMLongFade_free
 */
void BMLongFade_free(BMLongFade *This){
	free(This->buffer);
	This->buffer = NULL;
}

/*!
 *BMLongFade_processStereo
 */
void BMLongFade_processStereo(BMLongFade *This,
							  float *inL, float *inR,
							  float *outL, float *outR,
							  size_t numSamples){
	size_t samplesProcessedThisFunction = 0;
	while(samplesProcessedThisFunction < numSamples){
		size_t samplesProcessing = BM_MIN(BM_BUFFER_CHUNK_SIZE, numSamples - samplesProcessedThisFunction);
		
		// at the end of this buffer, what fraction of the fade will be complete?
		float fractionComplete = (float)(This->samplesProcessed + samplesProcessing) / (float)This->lengthSamples;
		
		// don't let it be more than 100% complete in case we process more than
		// the expected amount
		if(fractionComplete > 1.0) fractionComplete = 1.0;
		
		// find the starting gain for this buffer of samples
		float startGainThisBuffer = BM_DB_TO_GAIN(This->gainDB);
		
		// find the end gain for this buffer in decibels
		float endGainDBThisBuffer = BMLerp(This->startGainDB, This->endGainDB, fractionComplete);
		
		// find the ending gain for this buffer of samples
		float endGainThisBuffer = BM_DB_TO_GAIN(endGainDBThisBuffer);
		
		// write a linear interpolated ramp between startGain and endGain into
		// the buffer.
		//
		// How much to increment the gain with each sample?
		// Note that this actually stops slightly short of endGain. We do that
		// so that we can set startGain = endGain and continue smoothly from the
		// next buffer.
		float increment = (endGainThisBuffer - startGainThisBuffer) / samplesProcessing;
		vDSP_vramp(&startGainThisBuffer, &increment, This->buffer, 1, samplesProcessing);
		
		// multiply the ramp in the buffer by both channels
		vDSP_vmul(This->buffer, 1, inL + samplesProcessedThisFunction, 1, outL + samplesProcessedThisFunction, 1, samplesProcessing);
		vDSP_vmul(This->buffer, 1, inR + samplesProcessedThisFunction, 1, outR + samplesProcessedThisFunction, 1, samplesProcessing);
		
		// update gainDB for next time
		This->gainDB = endGainDBThisBuffer;
		
		// update the samples processed for this function call
		samplesProcessedThisFunction += samplesProcessing;
		// we also need to keep track of the samples processed between function calls
		This->samplesProcessed += samplesProcessing;
	}
}
