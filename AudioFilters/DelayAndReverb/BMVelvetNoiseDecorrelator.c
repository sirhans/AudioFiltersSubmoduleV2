//
//  BMVelvetNoiseDecorrelator.c
//  Saturator
//
//  Created by TienNM on 12/4/17
//  Rewritten by Hans on 9 October 2019
//  Anyone may use this file without restrictions
//

#include "BMVelvetNoiseDecorrelator.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../AudioFilter.h"
#include "BMVelvetNoiseInternal.h"
#include "BMReverb.h"
#include "../MathUtilities/BMVectorOps.h"
#include "BMReverb.h"
#include "BMSorting.h"

// the scale that brings dry and wet to a total energy of 1 (1 when the mode is off)
static float BMVelvetNoiseDecorrelator_unitEnergyScale(const BMVelvetNoiseDecorrelator *This, float wetGain){
	if(!This->unitTotalEnergy) return 1.0f;
	return 1.0f / sqrtf(This->dryGain*This->dryGain + wetGain*wetGain);
}


#define BM_VND_WET_MIX 0.40f

void BMVelvetNoiseDecorrelator_genRandGains(BMVelvetNoiseDecorrelator *This);

static void BMVelvetNoiseDecorrelator_initInternal(BMVelvetNoiseDecorrelator *This,
												float maxDelaySeconds,
												size_t numTaps,
												float rt60DecayTimeSeconds,
												bool hasDryTap,
												float sampleRate,
												bool evenTapDensity,
                                                const BMRandom *randomizerState);

void BMVelvetNoiseDecorrelator_initFullSettings(BMVelvetNoiseDecorrelator *This,
												float maxDelaySeconds,
												size_t numTaps,
												float rt60DecayTimeSeconds,
												bool hasDryTap,
												float sampleRate,
												bool evenTapDensity);

void BMVelvetNoiseDecorrelator_initMultiChannelInput(BMVelvetNoiseDecorrelator *This,
                                                float maxDelaySeconds,
                                                size_t numTaps,
                                                float rt60DecayTimeSeconds,
                                                bool hasDryTap,
                                                size_t numInput,
                                                float sampleRate,
                                                bool evenTapDensity);
/*!
 *BMVelvetNoiseDecorrelator_init
 */
void BMVelvetNoiseDecorrelator_init(BMVelvetNoiseDecorrelator *This,
                                    float maxDelaySeconds,
                                    size_t numTaps,
                                    float rt60DecayTimeSeconds,
									bool hasDryTap,
									float sampleRate){
	BMVelvetNoiseDecorrelator_initFullSettings(This, maxDelaySeconds, numTaps, rt60DecayTimeSeconds, hasDryTap, sampleRate, false);
}




/*!
 *BMVelvetNoiseDecorrelator_initWithEvenTapDensity
 */
void BMVelvetNoiseDecorrelator_initWithEvenTapDensity(BMVelvetNoiseDecorrelator *This,
                                    float maxDelaySeconds,
                                    size_t numTaps,
                                    float rt60DecayTimeSeconds,
									bool hasDryTap,
									float sampleRate){
	BMVelvetNoiseDecorrelator_initFullSettings(This, maxDelaySeconds, numTaps, rt60DecayTimeSeconds, hasDryTap, sampleRate, true);
}


/*!
 *BMVelvetNoiseDecorrelator_initWithDelayRange
 */
void BMVelvetNoiseDecorrelator_initWithDelayRange(BMVelvetNoiseDecorrelator *This,
												  float minDelaySeconds,
												  float maxDelaySeconds,
												  size_t numTaps,
												  float rt60DecayTimeSeconds,
												  bool hasDryTap,
												  float sampleRate){
	assert(minDelaySeconds >= 0.0f && minDelaySeconds < maxDelaySeconds);
	BMVelvetNoiseDecorrelator_initFullSettings(This, maxDelaySeconds, numTaps, rt60DecayTimeSeconds, hasDryTap, sampleRate, true);
	This->minDelayTimeS = minDelaySeconds;
	BMVelvetNoiseDecorrelator_randomiseAll(This);
}




/*!
 *BMVelvetNoiseDecorrelator_initFullSettings
 */
static void BMVelvetNoiseDecorrelator_initInternal(BMVelvetNoiseDecorrelator *This,
												float maxDelaySeconds,
												size_t numTaps,
												float rt60DecayTimeSeconds,
												bool hasDryTap,
												float sampleRate,
												bool evenTapDensity,
                                                const BMRandom *randomizerState){
	This->sampleRate = sampleRate;
	This->hasDryTap	= hasDryTap;
	This->wetMix = BM_VND_WET_MIX;
	This->rt60 = rt60DecayTimeSeconds;
	This->maxDelayTimeS = maxDelaySeconds;
	This->numWetTaps = numTaps;
	This->useExplicitGains = false;
	This->normaliseWetTaps = false;
	This->unitTotalEnergy = false;
	This->dryGain = 1.0f;
	This->wetTapGain = 1.0f;
	This->evenTapDensity = evenTapDensity;
    This->resetNumTaps = false;
    This->resetRT60DecayTime = false;
    This->fadeInSamples = 0;
	if (hasDryTap) This->numWetTaps--;
	// even density: by default the first wet tap comes one grid cell in
	This->minDelayTimeS = hasDryTap ? maxDelaySeconds / (float)This->numWetTaps : 0.0f;
	
	// allocate memory for calculating delay setups
	This->delayLengthsL = calloc(numTaps, sizeof(size_t));
	This->delayLengthsR = calloc(numTaps, sizeof(size_t));
	This->gainsL = calloc(numTaps, sizeof(float));
	This->gainsR = calloc(numTaps, sizeof(float));
    This->tempBuffer = calloc(BM_BUFFER_CHUNK_SIZE, sizeof(float));
	This->numInput = 0;
    
    //Off Switch
    BMSmoothSwitch_init(&This->offSwitchL, sampleRate);
    BMSmoothSwitch_init(&This->offSwitchR, sampleRate);
    
    BMSmoothSwitch_initWithRate(&This->offSwitchL, sampleRate,10.0f);
    BMSmoothSwitch_initWithRate(&This->offSwitchR, sampleRate,10.0f);
    
	// init the multi-tap delay in bypass mode
	size_t maxDelayLenth = ceil(maxDelaySeconds*sampleRate);
	BMMultiTapDelay_initBypass(&This->multiTapDelay,
							   true,
							   maxDelayLenth,
							   numTaps);
	
	// seed the random number generator, then set up the delay for processing
	if(randomizerState) This->rng = *randomizerState;
	else BMVelvetNoise_initRandomizer(&This->rng);
	BMVelvetNoiseDecorrelator_randomiseAll(This);
}




void BMVelvetNoiseDecorrelator_initFullSettings(BMVelvetNoiseDecorrelator *This,
                                                float maxDelaySeconds,
                                                size_t numTaps,
                                                float rt60DecayTimeSeconds,
                                                bool hasDryTap,
                                                float sampleRate,
                                                bool evenTapDensity){
    BMVelvetNoiseDecorrelator_initInternal(This, maxDelaySeconds, numTaps,
        rt60DecayTimeSeconds, hasDryTap, sampleRate, evenTapDensity, NULL);
}

void BMVelvetNoiseDecorrelator_initWithRandomizerState(BMVelvetNoiseDecorrelator *This,
                                                float minDelaySeconds,
                                                float maxDelaySeconds,
                                                size_t numTaps,
                                                float rt60DecayTimeSeconds,
                                                bool hasDryTap,
                                                bool evenTapDensity,
                                                float sampleRate,
                                                const BMRandom *randomizerState){
    assert(randomizerState != NULL);
    assert(minDelaySeconds == -1.0f ||
           (evenTapDensity && minDelaySeconds >= 0.0f && minDelaySeconds < maxDelaySeconds));
    BMVelvetNoiseDecorrelator_initInternal(This, maxDelaySeconds, numTaps,
        rt60DecayTimeSeconds, hasDryTap, sampleRate, evenTapDensity, randomizerState);
    if(minDelaySeconds >= 0.0f){
        // Match initWithDelayRange's sequence of random draws, including its
        // initial default-range setup, so existing explicitly seeded taps survive.
        This->minDelayTimeS = minDelaySeconds;
        BMVelvetNoiseDecorrelator_randomiseAll(This);
    }
}

void BMVelvetNoiseDecorrelator_setSeed(BMVelvetNoiseDecorrelator *This, uint32_t seed){
	BMRandom_init(&This->rng, seed);
	BMVelvetNoiseDecorrelator_randomiseAll(This);
}


/*!
 *BMVelvetNoiseDecorrelator_genRandGains
 *
 * @abstract generates random gains and normalises them and updates the delay
 */
#define VND_StartFadeInDB -60
void BMVelvetNoiseDecorrelator_genRandGains(BMVelvetNoiseDecorrelator *This){
	// prepare to skip an array index if there is a dry tap at the beginning
	size_t shift = This->hasDryTap ? 1 : 0;
	
	// use the velvet noise algorithm to set random delay tap signs
	BMVelvetNoise_setTapSignsInternal(&This->rng, This->gainsL+shift, This->numWetTaps);
	BMVelvetNoise_setTapSignsInternal(&This->rng, This->gainsR+shift, This->numWetTaps);
	
	
	// apply an exponential decay envelope to the gains
    float startGain = 0;//BM_DB_TO_GAIN(VND_StartFadeInDB);
	for(size_t i=shift; i<This->numWetTaps+shift; i++){
		// left channel
		float delayTimeInSeconds = This->delayLengthsL[i] / This->sampleRate;
		float rt60Gain = BMReverbDelayGainFromRT60(This->rt60, delayTimeInSeconds);
        float fadeIn = 1;
        if(This->fadeInSamples>0){
            float fade0To1 = MIN(This->delayLengthsL[i], This->fadeInSamples)/This->fadeInSamples;
            fadeIn = fade0To1*(1.0f-startGain) + startGain;
        }
		This->gainsL[i] *= rt60Gain * fadeIn;
		
		// right channel
		delayTimeInSeconds = This->delayLengthsR[i] / This->sampleRate;
		rt60Gain = BMReverbDelayGainFromRT60(This->rt60, delayTimeInSeconds);
        if(This->fadeInSamples>0){
            float fade0To1 = MIN(This->delayLengthsR[i], This->fadeInSamples)/This->fadeInSamples;
            fadeIn = fade0To1*(1.0f-startGain) + startGain;
        }
		This->gainsR[i] *= rt60Gain * fadeIn;
	}
    
    //Last tap gain
    This->lastTapGainL = This->gainsL[This->numWetTaps+shift-1];
    This->lastTapGainR = This->gainsR[This->numWetTaps+shift-1];
	
	// explicit gains: dry tap as given, wet taps scaled as given, no normalisation
	if(This->useExplicitGains){
		This->gainsL[0] = This->dryGain;
		This->gainsR[0] = This->dryGain;
		// the wet taps' level apart from their shape: unit energy per channel
		// first, so that wetTapGain is the gain of the whole burst
		if(This->normaliseWetTaps){
			BMVectorNormalise(This->gainsL+1, This->numWetTaps);
			BMVectorNormalise(This->gainsR+1, This->numWetTaps);
		}
		// unit total energy: dry and wet keep their ratio and are scaled together
		// so that dry^2 + wet^2 = 1 (the wet taps have unit energy here)
		float totalScale = BMVelvetNoiseDecorrelator_unitEnergyScale(This, This->wetTapGain);
		float wetScale = This->wetTapGain * totalScale;
		This->gainsL[0] *= totalScale;
		This->gainsR[0] *= totalScale;
		vDSP_vsmul(This->gainsL+1, 1, &wetScale, This->gainsL+1, 1, This->numWetTaps);
		vDSP_vsmul(This->gainsR+1, 1, &wetScale, This->gainsR+1, 1, This->numWetTaps);
		BMMultiTapDelay_setGains(&This->multiTapDelay, This->gainsL, This->gainsR);
	}
	
	// else if there is is a dry tap,
	// set the balance between all of the wet taps against the single dry tap.
	else if(This->hasDryTap)
		BMVelvetNoiseDecorrelator_setWetMix(This, This->wetMix);
	
	// else, if there is no dry tap, normalise so the wet gain is 1.0
	else {
		BMVectorNormalise(This->gainsL, This->numWetTaps);
		BMVectorNormalise(This->gainsR, This->numWetTaps);
		BMMultiTapDelay_setGains(&This->multiTapDelay, This->gainsL, This->gainsR);
	}
}






/*!
*BMVelvetNoiseDecorrelator_genRandTapTimes
*
* @abstract generates random delay tap times and updates the delay
*/
void BMVelvetNoiseDecorrelator_genRandTapTimes(BMVelvetNoiseDecorrelator *This){
	// prepare to skip an array index if there is a dry tap at the beginning
	size_t shift = This->hasDryTap ? 1 : 0;
	
	// set the dry tap delay time to zero
	if(This->hasDryTap){
		This->delayLengthsL[0] = 0;
		This->delayLengthsR[0] = 0;
	}
	
	// even tap density uses the velvet noise algorithm. The frequency response may be less even but it won't leave large gaps in the time domain
	if(This->evenTapDensity){
		// the first wet tap comes after minDelayTimeS (one grid cell by
		// default, or whatever initWithDelayRange was given)
		float minDelayTimeS = This->minDelayTimeS;
		
		// set the randomised tap indices
		BMVelvetNoise_setTapIndicesInternal(&This->rng,
									minDelayTimeS * 1000.0f,
									This->maxDelayTimeS * 1000.0f,
									This->delayLengthsL + shift,
									This->sampleRate, This->numWetTaps);
		BMVelvetNoise_setTapIndicesInternal(&This->rng,
									minDelayTimeS * 1000.0f,
									This->maxDelayTimeS * 1000.0f,
									This->delayLengthsR + shift,
									This->sampleRate, This->numWetTaps);
	}
	// uneven tap density may have more natural frequency response
	else {
		// set the min delay index depending on whether there is a dry tap
		size_t min;
		if(This->hasDryTap)
			min = ceil((This->maxDelayTimeS * This->sampleRate) / (float)This->numWetTaps);
		else
			min = 0;
		
		// set max delay index
		size_t max = ceil(This->maxDelayTimeS * This->sampleRate);
		
		// set the random delay times from this instance's own seeded stream
		// (BMReverbRandomsInRange uses the process-wide rand(), which would
		// make the pattern depend on what else has drawn random numbers)
		for(size_t i = 0; i < This->numWetTaps; i++){
			This->delayLengthsL[shift + i] = BMRandom_inRange(&This->rng, min, max);
			This->delayLengthsR[shift + i] = BMRandom_inRange(&This->rng, min, max);
		}
	}
	
	BMMultiTapDelay_setDelayTimes(&This->multiTapDelay, This->delayLengthsL, This->delayLengthsR);
}





void BMVelvetNoiseDecorrelator_randomiseAll(BMVelvetNoiseDecorrelator *This){
	// randomise times
	BMVelvetNoiseDecorrelator_genRandTapTimes(This);
	
	// randomise gains
	BMVelvetNoiseDecorrelator_genRandGains(This);
}




/*!
 *BMVelvetNoiseDecorrelator_setWetMix
 *
 * @abstract sets the wet/dry mix and issues the command to update the multitap delay
 */
void BMVelvetNoiseDecorrelator_setWetMix(BMVelvetNoiseDecorrelator *This, float wetMix01){
	// we need a dry tap to set the mix; otherwise it's fixed at 100% wet
	// beacuse the wet/dry mix setting is meaningless
	assert(This->hasDryTap);
	
	// keep the wet mix in bounds
	assert(0.0f <= wetMix01 & wetMix01 <= 1.0f);
	
	This->wetMix = wetMix01;
	This->useExplicitGains = false;
	This->normaliseWetTaps = false;
	This->unitTotalEnergy = false;

	// what is the minimum gain we can set for the dry tap? It would not make
	// sense for the dry tap to have less gain than the wet taps, so the minimum
	// is not simply zero.
	float minDryGain = sqrtf(1.0f / (This->numWetTaps + 1.0f));
	
	// find the corrected dry mix, which is in [minDryGain,1] instead of [0,1]
	float dryGainUncorrected = sqrt(1.0f - wetMix01*wetMix01);
	float dryGainCorrected = minDryGain + dryGainUncorrected * (1.0f - minDryGain);
	
	// find the corrected wet gain
	float wetGainCorrected = sqrt(1.0f - dryGainCorrected*dryGainCorrected);
	
	// set the dry bypass tap gains
	This->gainsL[0] = dryGainCorrected;
	This->gainsR[0] = dryGainCorrected;
	
	// normalize so the norm of the vector of all wet taps is 1.0
	BMVectorNormalise(This->gainsL+1, This->numWetTaps);
	BMVectorNormalise(This->gainsR+1, This->numWetTaps);
	
	// scale the wet taps to get the wet gain as desired
	vDSP_vsmul(This->gainsL+1, 1, &wetGainCorrected, This->gainsL+1, 1, This->numWetTaps);
	vDSP_vsmul(This->gainsR+1, 1, &wetGainCorrected, This->gainsR+1, 1, This->numWetTaps);

	// set the gains to the multitap delay
	BMMultiTapDelay_setGains(&This->multiTapDelay, This->gainsL, This->gainsR);
}


void BMVelvetNoiseDecorrelator_setDryAndWetTapGains(BMVelvetNoiseDecorrelator *This, float dryGain, float wetTapGain){
	assert(This->hasDryTap);
	This->useExplicitGains = true;
	This->normaliseWetTaps = false;
	This->unitTotalEnergy = false;
	This->dryGain = dryGain;
	This->wetTapGain = wetTapGain;
	BMVelvetNoiseDecorrelator_genRandGains(This);
}


void BMVelvetNoiseDecorrelator_setDryGainAndWetEnergy(BMVelvetNoiseDecorrelator *This, float dryGain, float wetGain){
	assert(This->hasDryTap);
	This->useExplicitGains = true;
	This->normaliseWetTaps = true;
	This->unitTotalEnergy = false;
	This->dryGain = dryGain;
	This->wetTapGain = wetGain;
	BMVelvetNoiseDecorrelator_genRandGains(This);
}


void BMVelvetNoiseDecorrelator_setDryAndWetAtUnitEnergy(BMVelvetNoiseDecorrelator *This, float dryGain, float wetGain){
	assert(This->hasDryTap && dryGain*dryGain + wetGain*wetGain > 0.0f);
	This->useExplicitGains = true;
	This->normaliseWetTaps = true;
	This->unitTotalEnergy = true;
	This->dryGain = dryGain;
	This->wetTapGain = wetGain;
	BMVelvetNoiseDecorrelator_genRandGains(This);
}


size_t BMVelvetNoiseDecorrelator_getNumTaps(const BMVelvetNoiseDecorrelator *This){
	return This->numWetTaps + (This->hasDryTap ? 1 : 0);
}


void BMVelvetNoiseDecorrelator_getTapTimes(const BMVelvetNoiseDecorrelator *This, float *timesL, float *timesR){
	size_t numTaps = BMVelvetNoiseDecorrelator_getNumTaps(This);
	for(size_t i=0; i<numTaps; i++){
		timesL[i] = (float)This->delayLengthsL[i] / This->sampleRate;
		timesR[i] = (float)This->delayLengthsR[i] / This->sampleRate;
	}
}


void BMVelvetNoiseDecorrelator_getTapGains(const BMVelvetNoiseDecorrelator *This, float *gainsL, float *gainsR){
	size_t numTaps = BMVelvetNoiseDecorrelator_getNumTaps(This);
	memcpy(gainsL, This->gainsL, sizeof(float)*numTaps);
	memcpy(gainsR, This->gainsR, sizeof(float)*numTaps);
}


void BMVelvetNoiseDecorrelator_setWetTapGain(BMVelvetNoiseDecorrelator *This, float wetTapGain){
	assert(This->hasDryTap && This->useExplicitGains);
	assert(wetTapGain > 0.0f && This->wetTapGain > 0.0f);
	
	// rescale the wet taps from the old gain to the new one; signs and decay are kept
	// at unit total energy the dry tap moves too: the two are rescaled together
	float oldScale = BMVelvetNoiseDecorrelator_unitEnergyScale(This, This->wetTapGain);
	float newScale = BMVelvetNoiseDecorrelator_unitEnergyScale(This, wetTapGain);
	float ratio = (wetTapGain * newScale) / (This->wetTapGain * oldScale);
	This->gainsL[0] = This->gainsR[0] = This->dryGain * newScale;
	vDSP_vsmul(This->gainsL+1, 1, &ratio, This->gainsL+1, 1, This->numWetTaps);
	vDSP_vsmul(This->gainsR+1, 1, &ratio, This->gainsR+1, 1, This->numWetTaps);
	This->wetTapGain = wetTapGain;
	This->lastTapGainL = This->gainsL[This->numWetTaps];
	This->lastTapGainR = This->gainsR[This->numWetTaps];
	BMMultiTapDelay_setGains(&This->multiTapDelay, This->gainsL, This->gainsR);
}


void BMVelvetNoiseDecorrelator_setNumTaps(BMVelvetNoiseDecorrelator *This, size_t numTaps){
    //Start off switch
    BMSmoothSwitch_setState(&This->offSwitchL, false);
    BMSmoothSwitch_setState(&This->offSwitchR, false);
    
    This->numWetTaps = numTaps;
    This->resetNumTaps = true;
    
}

void BMVelvetNoiseDecorrelator_resetNumTaps(BMVelvetNoiseDecorrelator *This){
    if(BMSmoothSwitch_getState(&This->offSwitchL)==BMSwitchOff){
        if(This->resetNumTaps){
            This->resetNumTaps = false;
            
            //Free it first
            BMMultiTapDelay_free(&This->multiTapDelay);
            // init the multi-tap delay in bypass mode
            size_t maxDelayLenth = ceil(This->maxDelayTimeS*This->sampleRate);
            BMMultiTapDelay_initBypass(&This->multiTapDelay,
                                       true,
                                       maxDelayLenth,
                                       This->numWetTaps);
            // setup the delay for processing
            BMVelvetNoiseDecorrelator_randomiseAll(This);
            
            //Enable off switch
            BMSmoothSwitch_setState(&This->offSwitchL, true);
            BMSmoothSwitch_setState(&This->offSwitchR, true);
        }
    }
}

void BMVelvetNoiseDecorrelator_setRT60DecayTime(BMVelvetNoiseDecorrelator *This, float rt60DT){
    This->rt60 = rt60DT;
    This->resetRT60DecayTime = true;
}

void BMVelvetNoiseDecorrelator_resetRT60DecayTime(BMVelvetNoiseDecorrelator *This){
    if(This->resetRT60DecayTime){
        This->resetRT60DecayTime = false;
        BMVelvetNoiseDecorrelator_genRandGains(This);
    }
}

void BMVelvetNoiseDecorrelator_setFadeIn(BMVelvetNoiseDecorrelator *This,float fadeInS){
    //Start off switch
    BMSmoothSwitch_setState(&This->offSwitchL, false);
    BMSmoothSwitch_setState(&This->offSwitchR, false);
    This->fadeInSamples = fadeInS * This->sampleRate;
    This->resetFadeIn = true;
}

void BMVelvetNoiseDecorrelator_resetFadeIn(BMVelvetNoiseDecorrelator *This){
    if(BMSmoothSwitch_getState(&This->offSwitchL)==BMSwitchOff){
        if(This->resetFadeIn){
            This->resetFadeIn = false;
            BMVelvetNoiseDecorrelator_genRandGains(This);
            
            BMSmoothSwitch_setState(&This->offSwitchL, true);
            BMSmoothSwitch_setState(&This->offSwitchR, true);
        }
    }
}

/*!
 *BMVelvetNoiseDecorrelator_free
 */
void BMVelvetNoiseDecorrelator_free(BMVelvetNoiseDecorrelator *This){
	BMMultiTapDelay_free(&This->multiTapDelay);
	
	free(This->delayLengthsL);
	This->delayLengthsL = NULL;
	free(This->delayLengthsR);
	This->delayLengthsR = NULL;
	free(This->gainsL);
	This->gainsL = NULL;
	free(This->gainsR);
	This->gainsR = NULL;
    free(This->tempBuffer);
    This->tempBuffer = NULL;
}


/*!
 *BMVelvetNoiseDecorrelator_processBufferStereo
 */
void BMVelvetNoiseDecorrelator_processBufferStereo(BMVelvetNoiseDecorrelator *This,
                                                   float* inputL,
                                                   float* inputR,
                                                   float* outputL,
                                                   float* outputR,
                                                   size_t length){
    //Reset numtap if needed
    BMVelvetNoiseDecorrelator_resetNumTaps(This);
    BMVelvetNoiseDecorrelator_resetRT60DecayTime(This);
    BMVelvetNoiseDecorrelator_resetFadeIn(This);
	// all processing is done by the multi-tap delay class
    BMMultiTapDelay_processBufferStereo(&This->multiTapDelay,
										inputL, inputR,
										outputL, outputR,
										length);
    //Off switch
    if(BMSmoothSwitch_getState(&This->offSwitchL)!=BMSwitchOn){
        BMSmoothSwitch_processBufferMono(&This->offSwitchL, outputL, outputL, length);
        BMSmoothSwitch_processBufferMono(&This->offSwitchR, outputR, outputR, length);
    }
}

void BMVelvetNoiseDecorrelator_processBufferStereoWithFinalOutput(BMVelvetNoiseDecorrelator *This,
                                                   float* inputL,
                                                   float* inputR,
                                                   float* outputL,
                                                   float* outputR,
                                                   float* finalOutputL,
                                                   float* finalOutputR,
                                                   size_t length){
    //Reset numtap if needed
    BMVelvetNoiseDecorrelator_resetNumTaps(This);
    BMVelvetNoiseDecorrelator_resetRT60DecayTime(This);
    BMVelvetNoiseDecorrelator_resetFadeIn(This);
    // all processing is done by the multi-tap delay class
    BMMultiTapDelay_processStereoWithFinalOutput(&This->multiTapDelay,
                                                 inputL, inputR,
                                                 outputL, outputR, finalOutputL, finalOutputR, length);
    //Apply lasttap gain
    vDSP_vsmul(finalOutputL, 1, &This->lastTapGainL, finalOutputL, 1, length);
    vDSP_vsmul(finalOutputR, 1, &This->lastTapGainR, finalOutputR, 1, length);
    
    //Off switch
    if(BMSmoothSwitch_getState(&This->offSwitchL)!=BMSwitchOn){
        BMSmoothSwitch_processBufferMono(&This->offSwitchL, finalOutputL, finalOutputL, length);
        BMSmoothSwitch_processBufferMono(&This->offSwitchR, finalOutputR, finalOutputR, length);
    }
}





void BMVelvetNoiseDecorrelator_processBufferMonoToStereo(BMVelvetNoiseDecorrelator *This,
														 float* inputL,
														 float* outputL, float* outputR,
														 size_t length){
    //Reset numtap if needed
    BMVelvetNoiseDecorrelator_resetNumTaps(This);
    BMVelvetNoiseDecorrelator_resetRT60DecayTime(This);
    BMVelvetNoiseDecorrelator_resetFadeIn(This);
	// all processing is done by the multi-tap delay class
	BMMultiTapDelay_processBufferStereo(&This->multiTapDelay,
										inputL, inputL,
										outputL, outputR,
										length);
    
    //Off switch
    if(BMSmoothSwitch_getState(&This->offSwitchL)!=BMSwitchOn){
        BMSmoothSwitch_processBufferMono(&This->offSwitchL, outputL, outputL, length);
        BMSmoothSwitch_processBufferMono(&This->offSwitchR, outputR, outputR, length);
    }
}
