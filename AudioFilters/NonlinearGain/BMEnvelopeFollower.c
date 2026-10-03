//
//  BMEnvelopeFollower.c
//  AudioFiltersXCodeProject
//
//  Created by hans anderson on 2/26/19.
//
//  Anyone may use this file without restrictions.
//

#include <math.h>
#include <assert.h>
#include "../AudioFilter.h"
#include "BMEnvelopeFollower.h"
#include "../Constants.h"

// set all stages of smoothing filters to critically damped
#define BMENV_FILTER_Q 0.5f

// set default time for attack and release
#define BMENV_ATTACK_TIME 1.0f / 1000.0f
#define BMENV_RELEASE_TIME 1.0f / 10.0f



void BMAttackFilter_setCutoff(BMAttackFilter *This, float fc){
    assert(fc > 0.0f);
    
    This->fc = fc;
    
    // compute the input gain to the integrators
    double g = tan(M_PI * (double)fc / (double)This->sampleRate);
	
	// we need double precision for the coefficient calculation but can use
	// single precision float for the filter processing
	double a1, a2, a3;
    
    // update the first integrator state variable to ensure that the second
    // derivative remains continuous when changing the cutoff freuency
    This->ic1 *= This->g / g;
    
    // then save the new value of the gain coefficient
    This->g = (float)g;
    
    // compute the three filter coefficients
    a1 = 1.0f / (1.0f + g * (g + (double)This->k));
    a2 = a1  * This->g;
    a3 = a2  * This->g;
	
	// copy the double precision filter coefficients in this function
	// to the single precision coefficients in the filter struct
	This->a1 = (float)a1;
	This->a2 = (float)a2;
	This->a3 = (float)a3;
    
    // precompute a value that helps us set the state variable to keep the
    // gradient continuous when switching from release to attack mode
    This->gInv_2 = 1.0f / (2.0f * g);
}




void BMReleaseFilter_setCutoff(BMReleaseFilter *This, float fc){
    assert(fc > 0.0f);
    
    This->fc = fc;
    
    // compute the input gain to the integrators
    double g = tanf(M_PI * fc / This->sampleRate);
	
	// we need double precision for the coefficient calculation but can use
	// single precision float for the filter processing
	double a1, a2, a3;
    
    // update the first integrator state variable to ensure that the second
    // derivative remains continuous when changing the cutoff freuency
    This->ic1 *= This->g / g;
    
    // then save the new value of the gain coefficient
    This->g = (float)g;
    
    // compute the three filter coefficients
    a1 = 1.0f / (1.0f + g * (g + (double)This->k));
    a2 = a1  * This->g;
    a3 = a2  * This->g;
	
	// copy the double precision filter coefficients in this function
	// to the single precision coefficients in the filter struct
	This->a1 = (float)a1;
	This->a2 = (float)a2;
	This->a3 = (float)a3;
}

void BMReleaseFilter_setCutoffRange(BMReleaseFilter *This, float min,float cross,float max){
    This->fcMin = min;
    This->fcCross = cross;
    This->fcMax = max;
}

void BMReleaseFilter_setDBRange(BMReleaseFilter *This, float min,float max){
    This->minDb = min;
    This->maxDb = max;
}

void BMReleaseFilter_updateSampleRate(BMReleaseFilter *This, float sampleRate){
    This->sampleRate = sampleRate;
    BMReleaseFilter_setCutoff(This, This->fc);
}



void BMAttackFilter_updateSampleRate(BMAttackFilter *This, float sampleRate){
    This->sampleRate = sampleRate;
    BMAttackFilter_setCutoff(This, This->fc);
}



/*!
 * ARTimeToCutoffFrequency
 * @param time       time in seconds
 * @param numStages  number of stages in the filter cascade
 * @abstract The -3db point of the filters shifts to a lower frequency each time we add another filter to the cascade. This function converts from time to cutoff frequency, taking into account the number of filters in the cascade.
 */
float ARTimeToCutoffFrequency(float time, size_t numStages){
    assert (time > 0.0f);
    float correctionFactor = 0.472859f + 0.434404 * (float)numStages;
    return 1.0f / (time / correctionFactor);
}




void BMAttackFilter_init(BMAttackFilter *This, float fc, float sampleRate){
    
    This->sampleRate = sampleRate;
    
    This->k = 1.0f / BMENV_FILTER_Q;
    This->g = 0.0f;
    This->previousOutputGradient = 0.0f;
    This->previousOutputValue = 0.0f;
    This->attackMode = false;
    This->ic1 = This->ic2 = 0;
    This->localPeakValue = 0;
    This->localReleaseValue = 0;
    
    BMAttackFilter_setCutoff(This, fc);
}



void BMReleaseFilter_init(BMReleaseFilter *This, float fc, float sampleRate){
    
    This->sampleRate = sampleRate;
    
    This->k = 1.0f / BMENV_FILTER_Q;
    This->g = 0.0f;
    This->previousOutputValue = 0.0f;
    This->attackMode = false;
    This->ic1 = This->ic2 = 0;
    
    BMReleaseFilter_setCutoffRange(This, 1.0f,1.0f,1.0f);
    BMReleaseFilter_setDBRange(This, 0.0f,0.0f);
    BMReleaseFilter_setCutoff(This, fc);
    
}




void BMAttackFilter_processBuffer(BMAttackFilter *This,
                                  const float* input,
                                  float* output,
                                  size_t numSamples){
    
    // The state in locals for the loop: output is a float pointer, so the
    // compiler must assume it may point into *This and would reload every
    // field after each store. The same arithmetic, in the same order.
    float ic1 = This->ic1, ic2 = This->ic2;
    float previousOutputValue = This->previousOutputValue;
    float previousOutputGradient = This->previousOutputGradient;
    bool attackMode = This->attackMode;
    const float a1 = This->a1, a2 = This->a2, a3 = This->a3, gInv_2 = This->gInv_2;
    
    for (size_t i=0; i<numSamples; i++){
        float x = input[i];
        float y;
        
        // if we are in attack mode,
        if (x > previousOutputValue){
            
            // if this is the first sample in attack mode
            if(!attackMode){
                // set the attack mode flag
                attackMode = true;
                // and update the state variables to get continuous gradient
                ic1 = previousOutputGradient * gInv_2;
                // and continuous output value
                ic2 = previousOutputValue;
            }
            
            // process the state variable filter
            float v3 = x - ic2;
            float v1 = a1 * ic1 + a2 * v3;
            float v2 = ic2 + a2 * ic1 + a3 * v3;
            
            // update the state variables
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            
            // output
            y = v2;
        }
        
        // if we are in release mode,
        else {
            // unset the attack mode flag
            attackMode = false;
            
            // copy the input to the output
            y = x;
            
            // and save the gradient so that we have it ready in case the next
            // sample we switch into attack mode
            previousOutputGradient = x - previousOutputValue;
        }
        
        output[i] = y;
        previousOutputValue = y;
    }
    
    This->ic1 = ic1;
    This->ic2 = ic2;
    This->previousOutputValue = previousOutputValue;
    This->previousOutputGradient = previousOutputGradient;
    This->attackMode = attackMode;
}

void BMAttackFilter_processBufferLP(BMAttackFilter *This,
                                  const float* input,
                                  float* output,
                                  size_t numSamples){
    
    for (size_t i=0; i<numSamples; i++){
        float x = input[i];
        
        // process the state variable filter
        float v3 = x - This->ic2;
        float v1 = This->a1  * This->ic1 + This->a2 * v3;
        float v2 = This->ic2 + This->a2  * This->ic1 + This->a3 * v3;
        
        // update the state variables
        This->ic1 = 2.0f * v1 - This->ic1;
        This->ic2 = 2.0f * v2 - This->ic2;
        
        // output
        output[i] = v2;
    }
}

void BMAttackFilter_processBufferBelowDb(BMAttackFilter *This,float maxDb,
                                  const float* input,
                                  float* output,
                                  size_t numSamples){
    
    for (size_t i=0; i<numSamples; i++){
        float x = input[i];
        
        // if we are in attack mode,
        if (x > This->previousOutputValue&&x<maxDb){
            
            // if this is the first sample in attack mode
            if(!This->attackMode){
                // set the attack mode flag
                This->attackMode = true;
                // and update the state variables to get continuous gradient
                This->ic1 = This->previousOutputGradient  *This->gInv_2;
                // and continuous output value
                This->ic2 = This->previousOutputValue;
            }
            
            // process the state variable filter
            float v3 = x - This->ic2;
            float v1 = This->a1  *This->ic1 + This->a2 * v3;
            float v2 = This->ic2 + This->a2  *This->ic1 + This->a3 * v3;
            
            // update the state variables
            This->ic1 = 2.0f * v1 - This->ic1;
            This->ic2 = 2.0f * v2 - This->ic2;
            
            // output
            output[i] = v2;
        }
        
        // if we are in release mode,
        else {
            // unset the attack mode flag
            This->attackMode = false;
            
            // copy the input to the output
            output[i] = x;
            
            // and save the gradient so that we have it ready in case the next
            // sample we switch into attack mode
            This->previousOutputGradient = x - This->previousOutputValue;
        }
        
        This->previousOutputValue = output[i];
    }
}

void BMReleaseFilter_processBufferNegative(BMReleaseFilter *This,
                                   const float* input,
                                   float* output,
                                   size_t numSamples){
    // invert the sign of the signal
    vDSP_vneg(input, 1, output, 1, numSamples);
    BMReleaseFilter_processBuffer(This, output, output, numSamples);
    vDSP_vneg(output, 1, output, 1, numSamples);
}



void BMReleaseFilter_processBuffer(BMReleaseFilter *This,
                                   const float* input,
                                   float* output,
                                   size_t numSamples){
    
    // The state in locals for the loop (see BMAttackFilter_processBuffer).
    float ic1 = This->ic1, ic2 = This->ic2;
    float previousOutputValue = This->previousOutputValue;
    bool attackMode = This->attackMode;
    const float a1 = This->a1, a2 = This->a2, a3 = This->a3;
    
    for (size_t i=0; i<numSamples; i++){
        float x = input[i];
        float y;
        
        // if we are in attack mode,
        if (x > previousOutputValue){
            // set the attack mode flag
            attackMode = true;
            // copy the input to the output
            y = x;
        }
        
        // if we are in release mode,
        else {
            // if this is the first sample in release mode
            if(attackMode){
                // unset the attack mode flag
                attackMode = false;
                // set the gradient to zero
                ic1 = 0.0f;
                // set the second state variable to the previous output
                // to keep the output function continuous
                ic2 = previousOutputValue;
            }
            
            // process the state variable filter
            float v3 = x - ic2;
            float v1 = a1 * ic1 + a2 * v3;
            float v2 = ic2 + a2 * ic1 + a3 * v3;
            
            // update the state variables
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            
            // output
            y = v2;
        }
        
        output[i] = y;
        previousOutputValue = y;
    }
    
    This->ic1 = ic1;
    This->ic2 = ic2;
    This->previousOutputValue = previousOutputValue;
    This->attackMode = attackMode;
}



void BMReleaseFilter_processBufferDynamic(BMReleaseFilter *This,
                                   const float* input,
                                   float* output,
                                   size_t numSamples){
    for (size_t i=0; i<numSamples; i++){
        //Calculate fc - Distance is between 3 db -> fc grow from fcMax to fcMin
        float distance = fabsf(input[i]);
        float v = BM_MIN(BM_MAX((distance - This->minDb)/(This->maxDb - This->minDb),0),1.0f);
        float fc = This->fcMin + (This->fcMax-This->fcMin) * powf(v, 10.0f);
        BMReleaseFilter_setCutoff(This, fc);
        
        //Process
        float x = input[i];
        
        // if we are in attack mode,
        if (x > This->previousOutputValue){
            // set the attack mode flag
            This->attackMode = true;
            // copy the input to the output
            output[i] = x;
        }
        
        // if we are in release mode,
        else {
            // if this is the first sample in release mode
            if(This->attackMode){
                // unset the attack mode flag
                This->attackMode = false;
                // set the gradient to zero
                This->ic1 = 0.0f;
                // set the second state variable to the previous output
                // to keep the output function continuous
                This->ic2 = This->previousOutputValue;
            }
            
            // process the state variable filter
            float v3 = x - This->ic2;
            float v1 = This->a1  *This->ic1 + This->a2 * v3;
            float v2 = This->ic2 + This->a2  *This->ic1 + This->a3 * v3;
            
            // update the state variables
            This->ic1 = 2.0f * v1 - This->ic1;
            This->ic2 = 2.0f * v2 - This->ic2;
            
            // output
            output[i] = v2;
        }
        
        This->previousOutputValue = output[i];
        
        
    }
}

void BMReleaseFilter_processBufferDynamic1(BMReleaseFilter *This,
                                   const float* input,
                                   float* output,
                                   size_t numSamples){
    for (size_t i=0; i<numSamples; i++){
        //Calculate fc - Distance is between 3 db -> fc grow from fcMax to fcMin
        float distance = fabsf(input[i]);
        if(distance<0.001f){
            This->samplesFromZero = 0;
        }else
            This->samplesFromZero++;
        float v = BM_MIN(BM_MAX((This->samplesFromZero/This->maxDb),0),1.0f);
        float fc = This->fcMin + (This->fcMax-This->fcMin) * powf(v, 10.0f);
        BMReleaseFilter_setCutoff(This, fc);
        
        //Process
        float x = input[i];
        
        // if we are in attack mode,
        if (x > This->previousOutputValue){
            // set the attack mode flag
            This->attackMode = true;
            // copy the input to the output
            output[i] = x;
        }
        
        // if we are in release mode,
        else {
            // if this is the first sample in release mode
            if(This->attackMode){
                // unset the attack mode flag
                This->attackMode = false;
                // set the gradient to zero
                This->ic1 = 0.0f;
                // set the second state variable to the previous output
                // to keep the output function continuous
                This->ic2 = This->previousOutputValue;
            }
            
            // process the state variable filter
            float v3 = x - This->ic2;
            float v1 = This->a1  *This->ic1 + This->a2 * v3;
            float v2 = This->ic2 + This->a2  *This->ic1 + This->a3 * v3;
            
            // update the state variables
            This->ic1 = 2.0f * v1 - This->ic1;
            This->ic2 = 2.0f * v2 - This->ic2;
            
            // output
            output[i] = v2;
        }
        
        This->previousOutputValue = output[i];
        
        
    }
}


void BMEnvelopeFollower_init(BMEnvelopeFollower *This, float sampleRate){
    BMEnvelopeFollower_initWithCustomNumStages(This, BMENV_NUM_STAGES, BMENV_NUM_STAGES, sampleRate);
}





void BMEnvelopeFollower_free(BMEnvelopeFollower *This){
    free(This->attackFilters);
    free(This->releaseFilters);
    This->attackFilters = NULL;
    This->releaseFilters = NULL;
}



/*!
 *BMEnvelopeFollower_initWithCustomNumStages
 */
void BMEnvelopeFollower_initWithCustomNumStages(BMEnvelopeFollower *This,
                                                size_t numReleaseStages,
                                                size_t numAttackStages,
                                                float sampleRate){
    // there must be at least one release filter
    assert(numReleaseStages > 0);
    assert(numAttackStages >= 0);
    
    // don't process the attack filters if there aren't any
    if (numAttackStages == 0) This->processAttack = false;
    
    // remember how many stages we have
    This->numAttackStages = numAttackStages;
    This->numReleaseStages = numReleaseStages;
    
    // set the attack and release filter cutoff frequencies, adjusted for the
    // number of stages
    float releaseFc = ARTimeToCutoffFrequency(BMENV_RELEASE_TIME, This->numReleaseStages);
    float attackFc = 1.0f;
    if(numAttackStages > 0)
        attackFc = ARTimeToCutoffFrequency(BMENV_ATTACK_TIME, This->numAttackStages);
    
    // allocate memory for the filter structs
    This->attackFilters = malloc(sizeof(BMAttackFilter)*This->numAttackStages);
    This->releaseFilters = malloc(sizeof(BMReleaseFilter)*This->numReleaseStages);
    
    // initialize all the filters
    for(size_t i=0; i<This->numAttackStages; i++)
        BMAttackFilter_init(&This->attackFilters[i], attackFc, sampleRate);
    for(size_t i=0; i<This->numReleaseStages; i++)
        BMReleaseFilter_init(&This->releaseFilters[i], releaseFc, sampleRate);
}





void BMEnvelopeFollower_setAttackTime(BMEnvelopeFollower *This, float attackTimeSeconds){
    assert(attackTimeSeconds >= 0.0f);
    
    // if the attack time is zero, don't process the attack at all
    if(attackTimeSeconds == 0.0f || This->numAttackStages == 0){
        This->processAttack = false;
    }
    
    // for non-zero attack time, set the attack filter frequency
    else {
        float attackFc = ARTimeToCutoffFrequency(attackTimeSeconds,This->numAttackStages);
        This->processAttack = true;
        for(size_t i=0; i<This->numAttackStages; i++)
            BMAttackFilter_setCutoff(&This->attackFilters[i],attackFc);
    }
}




void BMEnvelopeFollower_setReleaseTime(BMEnvelopeFollower *This, float releaseTimeSeconds){
    
    float releaseFc = ARTimeToCutoffFrequency(releaseTimeSeconds,This->numReleaseStages);
    
    for(size_t i=0; i<This->numReleaseStages; i++)
        BMReleaseFilter_setCutoff(&This->releaseFilters[i],releaseFc);
}





// All the stages in one pass, each sample through the release stages then the
// attack stages, every stage's state in locals: the same arithmetic as the
// stages one buffer pass at a time (BMReleaseFilter_processBuffer, then
// BMAttackFilter_processBuffer), so the same result. One pass a stage waits
// on that stage's recursion every sample; in one pass the stages of
// neighbouring samples overlap.
#define BMENV_FUSED_MAX_STAGES 4

static void BMEnvelopeFollower_processBufferFused(BMEnvelopeFollower *This, const float* input, float* output, size_t numSamples){
    const size_t nr = This->numReleaseStages, na = This->numAttackStages;
    float rIc1[BMENV_FUSED_MAX_STAGES], rIc2[BMENV_FUSED_MAX_STAGES], rPrev[BMENV_FUSED_MAX_STAGES];
    float rA1[BMENV_FUSED_MAX_STAGES], rA2[BMENV_FUSED_MAX_STAGES], rA3[BMENV_FUSED_MAX_STAGES];
    bool rAttack[BMENV_FUSED_MAX_STAGES];
    float aIc1[BMENV_FUSED_MAX_STAGES], aIc2[BMENV_FUSED_MAX_STAGES], aPrev[BMENV_FUSED_MAX_STAGES], aGrad[BMENV_FUSED_MAX_STAGES];
    float aA1[BMENV_FUSED_MAX_STAGES], aA2[BMENV_FUSED_MAX_STAGES], aA3[BMENV_FUSED_MAX_STAGES], aGInv2[BMENV_FUSED_MAX_STAGES];
    bool aAttack[BMENV_FUSED_MAX_STAGES];
    for(size_t j=0; j<nr; j++){
        const BMReleaseFilter *f = &This->releaseFilters[j];
        rIc1[j] = f->ic1; rIc2[j] = f->ic2; rPrev[j] = f->previousOutputValue; rAttack[j] = f->attackMode;
        rA1[j] = f->a1; rA2[j] = f->a2; rA3[j] = f->a3;
    }
    for(size_t j=0; j<na; j++){
        const BMAttackFilter *f = &This->attackFilters[j];
        aIc1[j] = f->ic1; aIc2[j] = f->ic2; aPrev[j] = f->previousOutputValue; aGrad[j] = f->previousOutputGradient;
        aAttack[j] = f->attackMode; aA1[j] = f->a1; aA2[j] = f->a2; aA3[j] = f->a3; aGInv2[j] = f->gInv_2;
    }
    
    for(size_t i=0; i<numSamples; i++){
        float x = input[i];
        for(size_t j=0; j<nr; j++){
            float y;
            if (x > rPrev[j]){
                rAttack[j] = true;
                y = x;
            } else {
                if (rAttack[j]){
                    rAttack[j] = false;
                    rIc1[j] = 0.0f;
                    rIc2[j] = rPrev[j];
                }
                float v3 = x - rIc2[j];
                float v1 = rA1[j] * rIc1[j] + rA2[j] * v3;
                float v2 = rIc2[j] + rA2[j] * rIc1[j] + rA3[j] * v3;
                rIc1[j] = 2.0f * v1 - rIc1[j];
                rIc2[j] = 2.0f * v2 - rIc2[j];
                y = v2;
            }
            rPrev[j] = y;
            x = y;
        }
        for(size_t j=0; j<na; j++){
            float y;
            if (x > aPrev[j]){
                if(!aAttack[j]){
                    aAttack[j] = true;
                    aIc1[j] = aGrad[j] * aGInv2[j];
                    aIc2[j] = aPrev[j];
                }
                float v3 = x - aIc2[j];
                float v1 = aA1[j] * aIc1[j] + aA2[j] * v3;
                float v2 = aIc2[j] + aA2[j] * aIc1[j] + aA3[j] * v3;
                aIc1[j] = 2.0f * v1 - aIc1[j];
                aIc2[j] = 2.0f * v2 - aIc2[j];
                y = v2;
            } else {
                aAttack[j] = false;
                y = x;
                aGrad[j] = x - aPrev[j];
            }
            aPrev[j] = y;
            x = y;
        }
        output[i] = x;
    }
    
    for(size_t j=0; j<nr; j++){
        BMReleaseFilter *f = &This->releaseFilters[j];
        f->ic1 = rIc1[j]; f->ic2 = rIc2[j]; f->previousOutputValue = rPrev[j]; f->attackMode = rAttack[j];
    }
    for(size_t j=0; j<na; j++){
        BMAttackFilter *f = &This->attackFilters[j];
        f->ic1 = aIc1[j]; f->ic2 = aIc2[j]; f->previousOutputValue = aPrev[j];
        f->previousOutputGradient = aGrad[j]; f->attackMode = aAttack[j];
    }
}

void BMEnvelopeFollower_processBuffer(BMEnvelopeFollower *This, const float* input, float* output, size_t numSamples){
    
    if(This->numReleaseStages <= BMENV_FUSED_MAX_STAGES && This->numAttackStages <= BMENV_FUSED_MAX_STAGES){
        BMEnvelopeFollower_processBufferFused(This, input, output, numSamples);
        return;
    }
    
    // process all the release filters in series
    BMReleaseFilter_processBuffer(&This->releaseFilters[0], input, output, numSamples);
    for(size_t i=1; i<This->numReleaseStages; i++)
        BMReleaseFilter_processBuffer(&This->releaseFilters[i], output, output, numSamples);

    if(This->numAttackStages > 0){
        // process all the attack filters in series
        for(size_t i=0; i<This->numAttackStages; i++)
            BMAttackFilter_processBuffer(&This->attackFilters[i], output, output, numSamples);
    }
}


// Process attack first, the release. This has the effect of ignoring short spikes
void BMEnvelopeFollower_processBufferRMS(BMEnvelopeFollower *This, const float* input, float* output, size_t numSamples){
	
	// process all the attack filters in series
	BMAttackFilter_processBufferLP(&This->attackFilters[0], input, output, numSamples);
	for(size_t i=1; i<This->numAttackStages; i++)
		BMAttackFilter_processBufferLP(&This->attackFilters[i], output, output, numSamples);

	if(This->numReleaseStages > 0){
		// process all the release filters in series
		for(size_t i=0; i<This->numReleaseStages; i++)
			BMReleaseFilter_processBuffer(&This->releaseFilters[i], output, output, numSamples);
	}
}


#pragma mark - FirstOrderAttackFilter
/*!
*BMFOAttackFilter_init
*/
void BMFOAttackFilter_init(BMFOAttackFilter* This,float fc, float sampleRate){
    This->z1_f = 0.0f;
    This->az_f = 0.0f;
    This->sampleRate = sampleRate;
    This->previousOutputValue = 0;
    This->attackMode = true;
    
    BMFOAttackFilter_setCutoff(This, fc);
}
void BMFOAttackFilter_setCutoff(BMFOAttackFilter* This, float fc){
    This->fc = fc;
    double gamma = tan(M_PI * fc / This->sampleRate);
    double one_over_denominator = 1.0 / (gamma + 1.0);
    
    This->b0_f = gamma * one_over_denominator;
    This->b1_f = This->b0_f;
    
    This->a1_f = (gamma - 1.0) * one_over_denominator;
}

void BMFOAttackFilter_processBuffer(BMFOAttackFilter* This,
                                    const float* input,
                                    float* output,
                                    size_t numSamples){
    
        for(size_t i=0; i<numSamples; i++){
            if(input[i]>This->z1_f){
                //Attack mode
                if(!This->attackMode){
                    This->attackMode = true;
                    //Connect sample from not attackmode to  attack mode
                    This->az_f = This->z1_f;
                }
                output[i] = input[i] * This->b0_f + This->az_f * This->b1_f - This->z1_f * This->a1_f;
            }else{
                //Not attack mode
                if(This->attackMode){
                    This->attackMode = false;
                }
                //Copy the output
                output[i] = input[i];
            }
            This->az_f = input[i];
            This->z1_f = output[i];
        }
}

void BMFOAttackFilter_processBufferBelowDb(BMFOAttackFilter* This,
                                    const float* input,
                                    float* output,float db,
                                    size_t numSamples){
    
        for(size_t i=0; i<numSamples; i++){
            if(input[i]>This->z1_f&&
               input[i]<db){
                //Attack mode
                if(!This->attackMode){
                    This->attackMode = true;
                    //Connect sample from not attackmode to  attack mode
                    This->az_f = This->z1_f;
                }
                output[i] = input[i] * This->b0_f + This->az_f * This->b1_f - This->z1_f * This->a1_f;
            }else{
                //Not attack mode
                if(This->attackMode){
                    This->attackMode = false;
                }
                //Copy the output
                output[i] = input[i];
            }
            This->az_f = input[i];
            This->z1_f = output[i];
        }
}

#pragma mark - FirstOrderReleaseFilter
void BMFOReleaseFilter_init(BMFOReleaseFilter* This,float fc, float sampleRate){
    This->z1_f = 0.0f;
    This->az_f = 0.0f;
    This->sampleRate = sampleRate;
    
    BMFOReleaseFilter_setCutoff(This, fc);
}
void BMFOReleaseFilter_setCutoff(BMFOReleaseFilter* This, float fc){
    double gamma = tan(M_PI * fc / This->sampleRate);
    double one_over_denominator = 1.0 / (gamma + 1.0);

    This->b0_f = gamma * one_over_denominator;
    This->b1_f = This->b0_f;

    This->a1_f = (gamma - 1.0) * one_over_denominator;
}

void BMFOReleaseFilter_processBuffer(BMFOReleaseFilter* This,
                                    const float* input,
                                    float* output,
                                    size_t numSamples){
    for(size_t i=0; i<numSamples; i++){
        if(input[i]<This->z1_f){
            //Release mode
            if(This->attackMode){
                This->attackMode = false;
                //Connect sample from not attackmode to  attack mode
                
            }
            output[i] = input[i] * This->b0_f + This->az_f * This->b1_f - This->z1_f * This->a1_f;
        }else{
            //Not release mode
            if(!This->attackMode){
                This->attackMode = true;
            }
            //Copy the output
            output[i] = input[i];
        }
        This->az_f = input[i];
        This->z1_f = output[i];
    }
}

void BMFOReleaseFilter_processBufferBelowDb(BMFOReleaseFilter* This,
                                    const float* input,
                                    float* output,float db,
                                    size_t numSamples){
    for(size_t i=0; i<numSamples; i++){
        if(input[i]<This->z1_f&&
           input[i]<db){
            //Release mode
            if(This->attackMode){
                This->attackMode = false;
                //Connect sample from not attackmode to  attack mode
                
            }
            output[i] = input[i] * This->b0_f + This->az_f * This->b1_f - This->z1_f * This->a1_f;
        }else{
            //Not release mode
            if(!This->attackMode){
                This->attackMode = true;
            }
            //Copy the output
            output[i] = input[i];
        }
        This->az_f = input[i];
        This->z1_f = output[i];
    }
}

void BMFOReleaseFilter_processBufferStandard(BMFOReleaseFilter* This,
                                    const float* input,
                                    float* output,
                                    size_t numSamples){
    for(size_t i=0; i<numSamples; i++){
        
        if(input[i]<This->z1_f){
            //Release mode
            if(This->attackMode){
                This->attackMode = false;
                //Connect sample from not attackmode to  attack mode
                
            }
            output[i] = input[i] * This->b0_f + This->az_f * This->b1_f - This->z1_f * This->a1_f;
        }else{
            //Not release mode
            if(!This->attackMode){
                This->attackMode = true;
            }
            //Copy the output
            output[i] = input[i];
        }
        This->az_f = input[i];
        This->z1_f = output[i];
    }
}

#pragma mark - Multi Release Filter
void BMMultiReleaseFilter_init(BMMultiReleaseFilter *This, float fc,int numLayers, float sampleRate){
    This->numLayers = numLayers;
    This->filters = malloc(sizeof(BMReleaseFilter)*numLayers);
    This->foFilters = malloc(sizeof(BMFOReleaseFilter)*numLayers);
    for(int i=0;i<numLayers;i++){
        BMReleaseFilter_init(&This->filters[i], fc, sampleRate);
        BMFOReleaseFilter_init(&This->foFilters[i], fc, sampleRate);
    }
}

void BMMultiReleaseFilter_destroy(BMMultiReleaseFilter *This){
    free(This->filters);
    This->filters = nil;
    free(This->foFilters);
    This->foFilters = nil;
}

void BMMultiReleaseFilter_setCutoff(BMMultiReleaseFilter *This, float fc){
    for(int i=0;i<This->numLayers;i++){
        BMReleaseFilter_setCutoff(&This->filters[i], fc);
        BMFOReleaseFilter_setCutoff(&This->foFilters[i], fc);
    }
}

void BMMultiReleaseFilter_setCutoffRange(BMMultiReleaseFilter *This, float min,float cross,float max){
    for(int i=0;i<This->numLayers;i++){
        BMReleaseFilter_setCutoffRange(&This->filters[i], min,cross, max);
    }
}
void BMMultiReleaseFilter_setDBRange(BMMultiReleaseFilter *This, float min,float max){
    for(int i=0;i<This->numLayers;i++){
        BMReleaseFilter_setDBRange(&This->filters[i], min, max);
    }
}

void BMMultiReleaseFilter_processBufferFO(BMMultiReleaseFilter *This,
                                   const float* input,
                                   float* output,
                                   size_t numSamples){
    memcpy(output, input, sizeof(float)*numSamples);
    for(int i=0;i<This->numLayers;i++){
        BMFOReleaseFilter_processBuffer(&This->foFilters[i], output, output, numSamples);
    }
}

void BMMultiReleaseFilter_processBufferFOBelowDb(BMMultiReleaseFilter *This,
                                   const float* input,
                                   float* output,float db,
                                   size_t numSamples){
    memcpy(output, input, sizeof(float)*numSamples);
    for(int i=0;i<This->numLayers;i++){
        BMFOReleaseFilter_processBufferBelowDb(&This->foFilters[i], output, output,db, numSamples);
    }
}

void BMMultiReleaseFilter_processBufferStandard(BMMultiReleaseFilter *This,
                                   const float* input,
                                   float* standard,
                                   size_t numSamples){
    memcpy(standard, input, sizeof(float)*numSamples);
    for(int i=0;i<This->numLayers;i++){
        BMFOReleaseFilter_processBuffer(&This->foFilters[i], standard, standard, numSamples);
    }
}

void BMMultiReleaseFilter_processBuffer(BMMultiReleaseFilter *This,
                                   const float* input,
                                   float* output,
                                   size_t numSamples){
    memcpy(output, input, sizeof(float)*numSamples);
    for(int i=0;i<This->numLayers;i++){
        BMReleaseFilter_processBuffer(&This->filters[i], output, output, numSamples);
    }
}



void BMMultiReleaseFilter_processBufferDynamic(BMMultiReleaseFilter *This,
                                   const float* input,
                                   float* output,
                                   size_t numSamples){
//    memcpy(output, input, sizeof(float)*numSamples);
    for(int i=0;i<This->numLayers;i++){
        BMReleaseFilter_processBufferDynamic(&This->filters[i], input, output, numSamples);
    }
}

void BMMultiReleaseFilter_processBufferDynamic1(BMMultiReleaseFilter *This,
                                   const float* input,
                                   float* output,
                                   size_t numSamples){
//    memcpy(output, input, sizeof(float)*numSamples);
    for(int i=0;i<This->numLayers;i++){
        BMReleaseFilter_processBufferDynamic1(&This->filters[i], input, output, numSamples);
    }
}

#pragma mark - Multi Attack Filter
void BMMultiAttackFilter_init(BMMultiAttackFilter *This, float fc,int numLayers, float sampleRate){
    This->numLayers = numLayers;
    This->filters = malloc(sizeof(BMAttackFilter)*numLayers);
    This->foFilters = malloc(sizeof(BMFOAttackFilter)*numLayers);
    for(int i=0;i<numLayers;i++){
        BMAttackFilter_init(&This->filters[i], fc, sampleRate);
        BMFOAttackFilter_init(&This->foFilters[i], fc, sampleRate);
    }
}

void BMMultiAttackFilter_destroy(BMMultiAttackFilter *This){
    free(This->filters);
    This->filters = nil;
    free(This->foFilters);
    This->foFilters = nil;
}

void BMMultiAttackFilter_setCutoff(BMMultiAttackFilter *This, float fc){
    for(int i=0;i<This->numLayers;i++){
        BMAttackFilter_setCutoff(&This->filters[i], fc);
        BMFOAttackFilter_setCutoff(&This->foFilters[i], fc);
    }
}

void BMMultiAttackFilter_processBuffer(BMMultiAttackFilter *This,
                                  const float* input,
                                  float* output,
                                  size_t numSamples){
    memcpy(output, input, sizeof(float)*numSamples);
    for(int i=0;i<This->numLayers;i++){
        BMAttackFilter_processBuffer(&This->filters[i], output, output, numSamples);
    }
}

void BMMultiAttackFilter_processBufferFO(BMMultiAttackFilter *This,
                                  const float* input,
                                  float* output,
                                  size_t numSamples){
    memcpy(output, input, sizeof(float)*numSamples);
    for(int i=0;i<This->numLayers;i++){
        BMFOAttackFilter_processBuffer(&This->foFilters[i], output, output, numSamples);
    }
}

void BMMultiAttackFilter_processBufferFOBelowDb(BMMultiAttackFilter *This,
                                  const float* input,
                                  float* output,float db,
                                  size_t numSamples){
    memcpy(output, input, sizeof(float)*numSamples);
    for(int i=0;i<This->numLayers;i++){
        BMFOAttackFilter_processBufferBelowDb(&This->foFilters[i], output, output,db, numSamples);
    }
}
