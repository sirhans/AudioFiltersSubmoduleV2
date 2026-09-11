//
//  BMHysteresisLimiter.c
//  AudioFiltersXcodeProject
//
//  Created by Hans on 29/10/19.
//  Anyone may use this file without restrictions
//

#include "BMHysteresisLimiter.h"
#include "BMAsymptoticLimiter.h"
#include <math.h>
#include <assert.h>
#include "Constants.h"

#define BM_HYSTERESISLIMITER_DEFAULT_POWER_LIMIT -45.0f
#define BM_HYSTERESISLIMITER_DEFAULT_SAG 1.0f / (4000.0f)




void BMHysteresisLimiter_processMonoRectified(BMHysteresisLimiter *This,
                                      const float *inputPos, const float *inputNeg,
                                      float* outputPos, float* outputNeg,
                                      size_t numSamples){
	assert(This->AAFilter.numChannels == 2 &&
		   numSamples % 2 == 0);
	
	// create two alias pointers for code readability
	float* limitedPos = outputPos;
	float* limitedNeg = outputNeg;
	
	// apply asymptotic limit
	BMAsymptoticLimitRectified(inputPos, inputNeg,
							   limitedPos, limitedNeg,
                               This->sampleRate,
                               This->sag,
							   numSamples);
	
	// antialiasing filter
	BMMultiLevelBiquad_processBufferStereo(&This->AAFilter,
										   limitedPos, limitedNeg,
										   limitedPos, limitedNeg,
										   numSamples);
	
    for(size_t i=0; i<numSamples; i++){
		// we alternate the order in which the positive and negative signals
		// consume charge in order to avoid biasing the signal by always consuming
		// from one side before the other
		
		// positive, then negative
		//
		// positive output
		float charge = This->halfSR * (1.0f - This->c);
		float oPos = limitedPos[i] * (This->c + charge);
		// update charge
		This->c = This->c - (oPos * This->s) + charge;
		//
		// negative output
		charge = This->halfSR * (1.0f - This->c);
        float oNeg = limitedNeg[i] * (This->c + charge);
        // update charge
		This->c = This->c + (oNeg * This->s) + charge;
		// output
        outputPos[i] = oPos;
        outputNeg[i] = oNeg;
		
		
		i++;
		// Negative, then positive
		//
		// negative output
		charge = This->halfSR * (1.0f - This->c);
        oNeg = limitedNeg[i] * (This->c + charge);
        // update charge
		This->c = This->c + (oNeg * This->s) + charge;
		//
		// positive output
		charge = This->halfSR * (1.0f - This->c);
		oPos = limitedPos[i] * (This->c + charge);
		// update charge
		This->c = This->c - (oPos * This->s) + charge;
		// output
        outputPos[i] = oPos;
        outputNeg[i] = oNeg;
    }
	
	// scale to compensate for gain loss
	vDSP_vsmul(outputPos,1,&This->oneOverR,outputPos,1,numSamples);
	vDSP_vsmul(outputNeg,1,&This->oneOverR,outputNeg,1,numSamples);
}




void BMHysteresisLimiter_processMonoClassA(BMHysteresisLimiter *This,
                                      const float *inputPos,
                                      float* outputPos,
                                      size_t numSamples){
	assert(This->AAFilter.numChannels == 1 &&
		   numSamples % 2 == 0);
	
	// create an alias pointer for code readability
	float* buffer = outputPos;
	
	// apply asymptotic limit
	BMAsymptoticLimitPositive(inputPos,
							  buffer,
							  This->sampleRate,
							  This->sag,
							  numSamples);
	
	// antialiasing filter
	BMMultiLevelBiquad_processBufferMono(&This->AAFilter,
										   buffer,
										   buffer,
										   numSamples);
	
    for(size_t i=0; i<numSamples; i++){
		// find output value
		float charge = This->halfSR * (1.0f - This->c);
		float oPos = buffer[i] * (This->c + charge);
		// update charge
		This->c = This->c - (oPos * This->s) + charge;
		// output
        buffer[i] = oPos;
    }
	
	// scale to compensate for gain loss
	vDSP_vsmul(buffer,1,&This->oneOverR,outputPos,1,numSamples);
}





void BMHysteresisLimiter_processStereoRectified(BMHysteresisLimiter *This,
													  const float *inputPosL,
													  const float *inputPosR,
													  const float *inputNegL,
													  const float *inputNegR,
													  float *outputPosL, float *outputPosR,
													  float *outputNegL, float *outputNegR,
													  size_t numSamples){
	assert(This->AAFilter.numChannels == 4 &&
		   numSamples % 2 == 0);
	
	// create some alias pointers for code readability
	float* limitedPosL = outputPosL;
	float* limitedNegL = outputNegL;
	float* limitedPosR = outputPosR;
	float* limitedNegR = outputNegR;
	
	// apply asymptotic limit
	BMAsymptoticLimitRectified(inputPosL, inputNegL,
							   limitedPosL, limitedNegL,
                               This->sampleRate,
                               This->sag,
							   numSamples);
	BMAsymptoticLimitRectified(inputPosR, inputNegR,
							   limitedPosR, limitedNegR,
                               This->sampleRate,
                               This->sag,
							   numSamples);
	
	// Antialiasing filter
	BMMultiLevelBiquad_processBuffer4(&This->AAFilter,
										   limitedPosL, limitedNegL,
										   limitedPosR, limitedNegR,
										   limitedPosL, limitedNegL,
										   limitedPosR, limitedNegR,
										   numSamples);
	
	
    for(size_t i=0; i<numSamples; i++){
		// we alternate the order in which the positive and negative signals
		// consume charge in order to avoid biasing the signal by always consuming
		// from one side before the other
		
		// Positive, then negative
		//
		// positive output
		simd_float2 charge = This->halfSR * (1.0f - This->cs);
		simd_float2 iPos = simd_make_float2(limitedPosL[i], limitedPosR[i]);
		simd_float2 oPos = iPos * (This->cs + charge);
		// update charge
		This->cs = This->cs - (This->s * oPos) + charge;
		//
		// negative output
		charge = This->halfSR * (1.0f - This->cs);
		simd_float2 iNeg = simd_make_float2(limitedNegL[i], limitedNegR[i]);
		simd_float2 oNeg = iNeg * (This->cs + charge);
        // update charge
		This->cs = This->cs + (This->s * oNeg) + charge;
		// output
        outputPosL[i] = oPos.x;
		outputPosR[i] = oPos.y;
        outputNegL[i] = oNeg.x;
		outputNegR[i] = oNeg.y;
		
		
		// negative, then positive
		i++;
		// negative output
		charge = This->halfSR * (1.0f - This->cs);
		iNeg = simd_make_float2(limitedNegL[i], limitedNegR[i]);
		oNeg = iNeg * (This->cs + charge);
        // update charge
		This->cs = This->cs + (This->s * oNeg) + charge;
		//
		// positive output
		charge = This->halfSR * (1.0f - This->cs);
		iPos = simd_make_float2(limitedPosL[i], limitedPosR[i]);
		oPos = iPos * (This->cs + charge);
		// update charge
		This->cs = This->cs - (This->s * oPos) + charge;
		// output
        outputPosL[i] = oPos.x;
		outputPosR[i] = oPos.y;
        outputNegL[i] = oNeg.x;
		outputNegR[i] = oNeg.y;
    }
	
	// scale to compensate for gain loss
	vDSP_vsmul(outputPosL,1,&This->oneOverR,outputPosL,1,numSamples);
	vDSP_vsmul(outputPosR,1,&This->oneOverR,outputPosR,1,numSamples);
	vDSP_vsmul(outputNegL,1,&This->oneOverR,outputNegL,1,numSamples);
	vDSP_vsmul(outputNegR,1,&This->oneOverR,outputNegR,1,numSamples);
}





void BMHysteresisLimiter_processStereoClassA(BMHysteresisLimiter *This,
											 const float *inputPosL,
											 const float *inputPosR,
											 float *outputPosL,
											 float *outputPosR,
											 size_t numSamples){
	assert(This->AAFilter.numChannels == 2 &&
		   numSamples % 2 == 0);
	
	// create some alias pointers for code readability
	float* limitedPosL = outputPosL;
	float* limitedPosR = outputPosR;
	
	// apply asymptotic limit
	BMAsymptoticLimitPositive(inputPosL,
							  limitedPosL,
							  This->sampleRate,
							  This->sag,
							  numSamples);
	BMAsymptoticLimitPositive(inputPosR,
							  limitedPosR,
							  This->sampleRate,
							  This->sag,
							  numSamples);
	
	// Antialiasing filter
	BMMultiLevelBiquad_processBufferStereo(&This->AAFilter,
										   limitedPosL, limitedPosR,
										   limitedPosL, limitedPosR,
										   numSamples);
	
	
    for(size_t i=0; i<numSamples; i++){
		// find output value
		simd_float2 charge = This->halfSR * (1.0f - This->cs);
		simd_float2 iPos = simd_make_float2(limitedPosL[i], limitedPosR[i]);
		simd_float2 oPos = iPos * (This->cs + charge);
		// update charge
		This->cs = This->cs - (This->s * oPos) + charge;
        outputPosL[i] = oPos.x;
		outputPosR[i] = oPos.y;
    }
	
	// scale to compensate for gain loss
	vDSP_vsmul(outputPosL,1,&This->oneOverR,outputPosL,1,numSamples);
	vDSP_vsmul(outputPosR,1,&This->oneOverR,outputPosR,1,numSamples);
}





void BMHysteresisLimiter_processStereoSimple(BMHysteresisLimiter *This,
											 const float *inputL, const float *inputR,
											 float *outputL, float *outputR,
											 size_t numSamples){
	assert(This->AAFilter.numChannels == 2);
		   
	// create some aliases for code readability
	float *limitedL = outputL;
	float *limitedR = outputR;
	
	// apply asymptotic limit
	BMAsymptoticLimit(inputL, limitedL, This->sampleRate, This->sag, numSamples);
	BMAsymptoticLimit(inputR, limitedR, This->sampleRate, This->sag, numSamples);
	
	// Antialiasing filter
	BMMultiLevelBiquad_processBufferStereo(&This->AAFilter,
										   limitedL, limitedR,
										   limitedL, limitedR,
										   numSamples);
	
	for(size_t i=0; i<numSamples; i++){
		// output
		simd_float2 in = simd_make_float2(limitedL[i], limitedR[i]);
		simd_float2 out = in * This->cs;
		// update charge
		This->cs -= This->s * simd_abs(out);
		This->cs += This->sR * (1.0f - This->cs);
		// scale to compensate for gain loss
		out *= This->oneOverR;
		// output
		outputL[i] = out.x;
		outputR[i] = out.y;
	}
}



void BMHysteresisLimiter_updateSettings(BMHysteresisLimiter *This){
    This->s = 1.0f / (This->sampleRate * This->sag);
    This->oneOverR = 1.0 / This->R;
    This->sR = This->s * This->R;
    This->halfSR = This->sR * 0.5f;
}



void BMHysteresisLimiter_setPowerLimit(BMHysteresisLimiter *This, float limitDb){
    assert(limitDb <= 0.0f);
    This->R = BM_DB_TO_GAIN(limitDb);
    BMHysteresisLimiter_updateSettings(This);
}





void BMHysteresisLimiter_setSag(BMHysteresisLimiter *This, float sag){
    assert(sag > 0.0);
    This->sag = sag;
    BMHysteresisLimiter_updateSettings(This);
}





void BMHysteresisLimiter_setAAFilterFC(BMHysteresisLimiter *This, float fc){
    // safety check: don't let fc exceed 90% of the Nyquist frequency
    fc = BM_MIN(This->sampleRate * 0.5f * 0.9f, fc);
    // set the AA filters
	for(size_t i=0; i<This->AAFilter.numLevels; i++){
		// BMMultiLevelBiquad_setLowPass6db(&This->AAFilter, fc, i);
		BMMultiLevelBiquad_setLowPassQ12db(&This->AAFilter, fc, 0.5f, i);
	}
	// keep the double-precision copy of the filter in step (state is reset;
	// this is only called at configuration time)
	if(This->AAFilterD) vDSP_biquadm_DestroySetupD(This->AAFilterD);
	This->AAFilterD = vDSP_biquadm_CreateSetupD(This->AAFilter.coefficients_d, This->AAFilter.numLevels, This->AAFilter.numChannels);
	// BMMultiLevelBiquad_setLegendreLP(&This->AAFilter, fc, 0, BM_HYSTERESISLIMITER_AA_FILTER_NUMLEVELS);
}




void BMHysteresisLimiter_init(BMHysteresisLimiter *This,
							  float sampleRate,
							  size_t aaFilterNumLevels,
							  size_t aaFilterFc,
							  size_t numChannels){
	assert(numChannels == 1 || numChannels == 2 || numChannels == 4);
    
    This->sampleRate = sampleRate;
	This->AAFilterD = NULL;
	
	// init the AA filter
    assert(aaFilterFc < sampleRate * 0.5f);
	if(numChannels == 1)
		BMMultiLevelBiquad_init(&This->AAFilter, aaFilterNumLevels, sampleRate, false, false, false);
	if(numChannels == 2)
		BMMultiLevelBiquad_init(&This->AAFilter, aaFilterNumLevels, sampleRate, true, false, false);
	if(numChannels == 4)
		BMMultiLevelBiquad_init4(&This->AAFilter, aaFilterNumLevels, sampleRate, false);
	BMHysteresisLimiter_setAAFilterFC(This,aaFilterFc);

	BMHysteresisLimiter_setPowerLimit(This, BM_HYSTERESISLIMITER_DEFAULT_POWER_LIMIT);
    BMHysteresisLimiter_setSag(This, BM_HYSTERESISLIMITER_DEFAULT_SAG);
	
	This->c = 0.0f;
	This->cs = 0.0f;
	This->cd = 0.0;
	This->csd = 0.0;
}





void BMHysteresisLimiter_free(BMHysteresisLimiter *This){
	BMMultiLevelBiquad_free(&This->AAFilter);
	if(This->AAFilterD) vDSP_biquadm_DestroySetupD(This->AAFilterD);
	This->AAFilterD = NULL;
}




/************************************************
 *          Double-precision processing         *
 ************************************************/

/* out = in / (1 + |in|/(fs*sag)), for rectified (sign-known) inputs */
static void BMHysteresisLimiter_asymptoticLimitRectifiedD(const double *inputPos, const double *inputNeg,
														   double *outputPos, double *outputNeg,
														   double sampleRate, double sag, size_t numSamples){
	double s = 1.0 / (sampleRate * sag);
	for(size_t i=0; i<numSamples; i++){
		outputPos[i] = inputPos[i] / (1.0 + s * inputPos[i]);
		outputNeg[i] = inputNeg[i] / (1.0 - s * inputNeg[i]);
	}
}



void BMHysteresisLimiter_processMonoRectifiedD(BMHysteresisLimiter *This,
											   const double *inputPos, const double *inputNeg,
											   double *outputPos, double *outputNeg,
											   size_t numSamples){
	assert(This->AAFilter.numChannels == 2 && numSamples % 2 == 0);
	
	// asymptotic limit, into the output buffers
	BMHysteresisLimiter_asymptoticLimitRectifiedD(inputPos, inputNeg, outputPos, outputNeg,
												  This->sampleRate, This->sag, numSamples);
	
	// antialiasing filter, in place
	const double *in2 [2] = {outputPos, outputNeg};
	double *out2 [2] = {outputPos, outputNeg};
	vDSP_biquadmD(This->AAFilterD, in2, 1, out2, 1, numSamples);
	
	// charge recursion, alternating the pos/neg order every other sample
	// exactly as in the float version
	double c = This->cd;
	const double s = This->s, halfSR = This->halfSR;
	for(size_t i=0; i<numSamples; i++){
		double charge = halfSR * (1.0 - c);
		double oPos = outputPos[i] * (c + charge);
		c = c - (oPos * s) + charge;
		charge = halfSR * (1.0 - c);
		double oNeg = outputNeg[i] * (c + charge);
		c = c + (oNeg * s) + charge;
		outputPos[i] = oPos;
		outputNeg[i] = oNeg;
		
		i++;
		charge = halfSR * (1.0 - c);
		oNeg = outputNeg[i] * (c + charge);
		c = c + (oNeg * s) + charge;
		charge = halfSR * (1.0 - c);
		oPos = outputPos[i] * (c + charge);
		c = c - (oPos * s) + charge;
		outputPos[i] = oPos;
		outputNeg[i] = oNeg;
	}
	This->cd = c;
	This->c = (float)c;
	
	// scale to compensate for gain loss
	double oneOverR = This->oneOverR;
	vDSP_vsmulD(outputPos, 1, &oneOverR, outputPos, 1, numSamples);
	vDSP_vsmulD(outputNeg, 1, &oneOverR, outputNeg, 1, numSamples);
}



void BMHysteresisLimiter_processStereoRectifiedD(BMHysteresisLimiter *This,
												 const double *inputPosL, const double *inputPosR,
												 const double *inputNegL, const double *inputNegR,
												 double *outputPosL, double *outputPosR,
												 double *outputNegL, double *outputNegR,
												 size_t numSamples){
	assert(This->AAFilter.numChannels == 4 && numSamples % 2 == 0);
	
	BMHysteresisLimiter_asymptoticLimitRectifiedD(inputPosL, inputNegL, outputPosL, outputNegL,
												  This->sampleRate, This->sag, numSamples);
	BMHysteresisLimiter_asymptoticLimitRectifiedD(inputPosR, inputNegR, outputPosR, outputNegR,
												  This->sampleRate, This->sag, numSamples);
	
	const double *in4 [4] = {outputPosL, outputNegL, outputPosR, outputNegR};
	double *out4 [4] = {outputPosL, outputNegL, outputPosR, outputNegR};
	vDSP_biquadmD(This->AAFilterD, in4, 1, out4, 1, numSamples);
	
	// charge recursion, alternating the pos/neg order every other sample
	// exactly as in the float version
	simd_double2 cs = This->csd;
	const double s = This->s, halfSR = This->halfSR;
	for(size_t i=0; i<numSamples; i++){
		// positive, then negative
		simd_double2 charge = halfSR * (1.0 - cs);
		simd_double2 iPos = simd_make_double2(outputPosL[i], outputPosR[i]);
		simd_double2 oPos = iPos * (cs + charge);
		cs = cs - (s * oPos) + charge;
		charge = halfSR * (1.0 - cs);
		simd_double2 iNeg = simd_make_double2(outputNegL[i], outputNegR[i]);
		simd_double2 oNeg = iNeg * (cs + charge);
		cs = cs + (s * oNeg) + charge;
		outputPosL[i] = oPos.x;
		outputPosR[i] = oPos.y;
		outputNegL[i] = oNeg.x;
		outputNegR[i] = oNeg.y;
		
		// negative, then positive
		i++;
		charge = halfSR * (1.0 - cs);
		iNeg = simd_make_double2(outputNegL[i], outputNegR[i]);
		oNeg = iNeg * (cs + charge);
		cs = cs + (s * oNeg) + charge;
		charge = halfSR * (1.0 - cs);
		iPos = simd_make_double2(outputPosL[i], outputPosR[i]);
		oPos = iPos * (cs + charge);
		cs = cs - (s * oPos) + charge;
		outputPosL[i] = oPos.x;
		outputPosR[i] = oPos.y;
		outputNegL[i] = oNeg.x;
		outputNegR[i] = oNeg.y;
	}
	This->csd = cs;
	This->cs = simd_make_float2((float)cs.x, (float)cs.y);
	
	double oneOverR = This->oneOverR;
	vDSP_vsmulD(outputPosL, 1, &oneOverR, outputPosL, 1, numSamples);
	vDSP_vsmulD(outputPosR, 1, &oneOverR, outputPosR, 1, numSamples);
	vDSP_vsmulD(outputNegL, 1, &oneOverR, outputNegL, 1, numSamples);
	vDSP_vsmulD(outputNegR, 1, &oneOverR, outputNegR, 1, numSamples);
}
