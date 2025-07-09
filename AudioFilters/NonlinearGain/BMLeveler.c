//
//  BMLeveler.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 7/8/25.
//  Copyright © 2025 BlueMangoo. All rights reserved.
//


#include <Accelerate/Accelerate.h>
#include "BMLeveler.h"
#include "Constants.h"
#include "BMUnitConversion.h"


void BMLeveler_init(BMLeveler *This,
					float targetOutputDb,
					float ratio,
					float maxGainDb,
					float speedSeconds,
					float thresholdDb,
					float sampleRate){
	This->targetOutputDb = targetOutputDb;
	This->ratio = ratio;
	This->gradient = 1.0f - (1.0f / ratio);
	This->maxGainDb = maxGainDb;
	This->speedSeconds = speedSeconds;
	This->thresholdDb = thresholdDb;
	This->sampleRate = sampleRate;
	
	This->controlSignalBuffer = malloc(sizeof(float) * BM_BUFFER_CHUNK_SIZE);
	
	// init the envelope follower
	BMEnvelopeFollower_init(&This->envelopeFollowerL, sampleRate);
	BMEnvelopeFollower_init(&This->envelopeFollowerR, sampleRate);
	
	// set release time. On this, the Soundweb Designer help file says:
	//
	// "Adjusts the time it takes for the leveller to recover from high levels when low levels are encountered. Higher values make the recovery slower, so there are no sudden jumps in gain. Lower (faster) values allow the leveller to track and counteract rapidly decreasing levels."
	//
	// This indicates that the attack time is fixed, and speed is only for release time. We do not set attack time here, so it remains at the default value specified in the init function of BMEnvelopeFollower.
	BMEnvelopeFollower_setReleaseTime(&This->envelopeFollowerL, speedSeconds);
	BMEnvelopeFollower_setReleaseTime(&This->envelopeFollowerR, speedSeconds);
}




void BMLeveler_free(BMLeveler *This){
	free(This->controlSignalBuffer);
	This->controlSignalBuffer = NULL;
	
	BMEnvelopeFollower_free(&This->envelopeFollowerL);
	BMEnvelopeFollower_free(&This->envelopeFollowerR);
}




void BMLeveler_processHelper(BMLeveler *This,
							 float *input, float *output,
							 BMEnvelopeFollower *env,
							 size_t numSamples){
	
	size_t samplesProcessed = 0;
	size_t samplesProcessing;
	while(samplesProcessed < numSamples){
		samplesProcessing = BM_MIN(numSamples,BM_BUFFER_CHUNK_SIZE);
		
		// get a pointer to the control signal buffer to avoid writing "This->" many times.
		float* controlSignalBuffer = This->controlSignalBuffer;
		
		// rectify the input signal
		vDSP_vabs(input+samplesProcessed, 1, controlSignalBuffer, 1, samplesProcessing);
		
		// convert linear gain to decibel scale
		float one = 1.0f;
		uint32_t use20dBRule = 1;
		vDSP_vdbcon(controlSignalBuffer, 1, &one, controlSignalBuffer, 1, samplesProcessing, use20dBRule);
		
		// shift the values so that the target output volume is at zero
		float negativeTargetOutput = -This->targetOutputDb;
		vDSP_vsadd(controlSignalBuffer, 1, &negativeTargetOutput, controlSignalBuffer, 1, samplesProcessing);
		
		// replace values that were originally below the threshold with zeros
		// so that low volume signals are neither boosted nor cut.
		//
		// since in the previous line we shifted the values to place the target
		// output volume at zero, we need to calculate where the threshold
		// value has moved to.
		float shiftedThreshold = This->thresholdDb - This->targetOutputDb;
		// now we replace values below the shiftedThreshold with zeros
		vDSP_vthres(controlSignalBuffer, 1, &shiftedThreshold, controlSignalBuffer, 1, samplesProcessing);
		
		// apply the compression ratio
		vDSP_vsmul(controlSignalBuffer, 1, &This->gradient, controlSignalBuffer, 1, samplesProcessing);
		
		// replace values below -maxGainDb with -maxGainDb. This prevents the
		// leveler from boosting low volume signals more than maxGainDb decibels.
		float negativeMaxGainDb = -This->maxGainDb;
		vDSP_vthr(controlSignalBuffer, 1, &negativeMaxGainDb, controlSignalBuffer, 1, samplesProcessing);
		
		// filter to get a smooth volume change envelope
		BMEnvelopeFollower_processBuffer(env, controlSignalBuffer, controlSignalBuffer, samplesProcessing);
		
		// negate the signal to get the dB change required to apply the leveling effect
		vDSP_vneg(controlSignalBuffer, 1, controlSignalBuffer, 1, samplesProcessing);
		
		// convert to linear gain control signal
		BMConv_dBToGainV(controlSignalBuffer,controlSignalBuffer,samplesProcessing);

		// apply the gain adjustment to the audio signal
		vDSP_vmul(controlSignalBuffer, 1, input+samplesProcessed, 1, output+samplesProcessed, 1, samplesProcessing);
		
		// advance pointers to the next chunk
		samplesProcessed += samplesProcessing;
	}
}




void BMLeveler_processMono(BMLeveler *This,
						   float *input,
						   float *output,
						   size_t numSamples){
	BMLeveler_processHelper(This, input, output, &This->envelopeFollowerL, numSamples);
}




void BMLeveler_processStereo(BMLeveler *This,
							 float *inL, float *inR,
							 float *outL, float *outR,
							 size_t numSamples){
	BMLeveler_processHelper(This, inL, outL, &This->envelopeFollowerL, numSamples);
	BMLeveler_processHelper(This, inR, outR, &This->envelopeFollowerR, numSamples);
}
