//
//  BMMultiLevelBiquad.c
//  VelocityFilter
//
//  Created by Hans on 14/3/16.
//
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
//

#include "BMMultiLevelBiquad.h"
#include "../Constants.h"
#include "../MathUtilities/BMComplexMath.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <float.h>

//#ifdef __cplusplus
//extern "C" {
//#endif

/*
 * function declarations for use within this file
 */

// evaluate the combined transfer function of all levels of the filter
// at the frequency specified by z
DSPDoubleComplex BMMultiLevelBiquad_tfEval(BMMultiLevelBiquad *This, DSPDoubleComplex z);
DSPDoubleComplex BMMultiLevelBiquad_tfEvalAtLevel(BMMultiLevelBiquad *This, DSPDoubleComplex z,size_t level);

// Destroy the current filter setup and create a new one.
// This function must be called only from the audio processing thread.
void BMMultiLevelBiquad_recreate(BMMultiLevelBiquad *This);

// Create a filter setup object
void BMMultiLevelBiquad_create(BMMultiLevelBiquad *This);


static void BMMultiLevelBiquad_setSectionCoefs(BMMultiLevelBiquad *This, size_t level, BMBiquadSectionCoefs c);

// this is a thread-safe way to update filter coefficients
void BMMultiLevelBiquad_enqueueUpdate(BMMultiLevelBiquad *This);


// this function updates the filter immediately and is safe to call
// only from the audio thread. It changes the filter coefficients in realtime
// if possible; otherwise it calls _recreate.
void BMMultiLevelBiquad_updateNow(BMMultiLevelBiquad *This);


// Copy active / inactive state settings from BMMultiLevelBiquad into vDSP_biquadm
void BMMultiLevelBiquad_updateActiveLevels(BMMultiLevelBiquad *This);


void BMMultiLevelBiquad_resetState(BMMultiLevelBiquad *This){
    vDSP_biquadm_ResetState(This->filterSetup);
    This->needsClearState = false;
}

/* end internal function declarations */


void BMMultiLevelBiquad_clearBuffers(BMMultiLevelBiquad *This){
    This->needsClearState = true;
}


void BMMultiLevelBiquad_processBufferStereo(BMMultiLevelBiquad *This, const float* inL, const float* inR, float* outL, float* outR, size_t numSamples){
    // this function is only for two channel filtering
    assert(This->numChannels == 2);
    
    // clear state if necessary
    if(This->needsClearState) BMMultiLevelBiquad_resetState(This);
    
    // update filter coefficients if necessary
    if (This->needsUpdate) BMMultiLevelBiquad_updateNow(This);
    
    //Levels
    BMMultiLevelBiquad_updateActiveLevels(This);
    
    // link the two input buffers into a single multidimensional array
    const float* twoChannelInput [2] = {inL, inR};
    
    // link the two output buffers into a single multidimensional array
    float* twoChannelOutput [2] = {outL, outR};
    
    // apply a multilevel biquad filter to both channels
    vDSP_biquadm(This->filterSetup, (const float* _Nonnull * _Nonnull)twoChannelInput, 1, twoChannelOutput, 1, numSamples);
    
    BMSmoothGain_processBuffer(&This->gain, outL, outR, outL, outR, numSamples);
}






void BMMultiLevelBiquad_processBuffer4(BMMultiLevelBiquad *This,
                                       const float* in1, const float* in2, const float* in3, const float* in4,
                                       float* out1, float* out2, float* out3, float* out4,
                                       size_t numSamples){
    // this function is only for four channel filtering
    assert(This->numChannels == 4);
    
    // clear state if necessary
    if(This->needsClearState) BMMultiLevelBiquad_resetState(This);
    
    // update filter coefficients if necessary
    if (This->needsUpdate) BMMultiLevelBiquad_updateNow(This);
    
    //Levels
    BMMultiLevelBiquad_updateActiveLevels(This);
    
    // link the two input buffers into a single multidimensional array
    const float* fourChannelInput [4] = {in1, in2, in3, in4};
    
    // link the two output buffers into a single multidimensional array
    float* fourChannelOutput [4] = {out1, out2, out3, out4};
    
    
    // apply a multilevel biquad filter to both channels
    vDSP_biquadm(This->filterSetup, (const float* _Nonnull * _Nonnull)fourChannelInput, 1, fourChannelOutput, 1, numSamples);
    
    // apply a gain adjustment
    const float *inputs [4] = {out1, out2, out3, out4};
    float *outputs [4] = {out1, out2, out3, out4};
    BMSmoothGain_processBuffers(&This->gain, inputs, outputs, 4, numSamples);
}





void BMMultiLevelBiquad_processBufferMono(BMMultiLevelBiquad *This, const float* input, float* output, size_t numSamples){
    
    // this function is only for single channel filtering
    assert(This->numChannels == 1);
    
    // clear state if necessary
    if(This->needsClearState) BMMultiLevelBiquad_resetState(This);
    
    // update filter coefficients if necessary
    if (This->needsUpdate) BMMultiLevelBiquad_updateNow(This);
    
    //Levels
    BMMultiLevelBiquad_updateActiveLevels(This);
    
    // biquadm requires arrays of pointers as input and output
    const float* inputP [1] = {input};
    float* outputP [1] = {output};
        
    // apply a multiChannel biquad filter
    vDSP_biquadm(This->filterSetup, (const float* _Nonnull * _Nonnull)inputP, 1, outputP, 1, numSamples);
        
    
    BMSmoothGain_processBufferMono(&This->gain, output, output, numSamples);
}





void BMMultiLevelBiquad_init(BMMultiLevelBiquad *This,
                             size_t numLevels,
                             float sampleRate,
                             bool isStereo,
                             bool monoRealTimeUpdate,
                             bool smoothUpdate){
    // initialize pointers to null to prevent errors when calling
    // free() in the destroy function
    This->filterSetup = NULL;
    This->coefficients_d = NULL;
    
    This->needsUpdate = false;
    This->sampleRate = sampleRate;
    This->numLevels = numLevels;
    This->numChannels = isStereo ? 2 : 1;
    This->useSmoothUpdate = smoothUpdate;
    This->needUpdateActiveLevels = true;
    This->needsClearState = false;
    This->activeLevels = malloc(sizeof(bool)*numLevels);
    
    // Allocate memory for 5*numChannels coefficients per filter
    This->coefficients_d = malloc(numLevels*5*This->numChannels*sizeof(double));
    
    // start with all levels on bypass
    for (size_t i=0; i<numLevels; i++) {
        BMMultiLevelBiquad_setBypass(This, i);
    }
    
    // set 0db of gain
    BMSmoothGain_init(&This->gain, sampleRate);
    BMSmoothGain_init(&This->gain2, sampleRate);
    BMMultiLevelBiquad_setGainInstant(This,0.0);
    
    // setup filter struct
    BMMultiLevelBiquad_create(This);
    
    // set all filter levels active
    for(int i=0;i<numLevels;i++){
        This->activeLevels[i] = true;
    }
    BMMultiLevelBiquad_updateActiveLevels(This);
}




/*!
 *BMMultiLevelBiquad_init4
 */
void BMMultiLevelBiquad_init4(BMMultiLevelBiquad *This,
                             size_t numLevels,
                             float sampleRate,
                             bool smoothUpdate){
    
    // init as stereo to make use of the code that is in the existing init function
    BMMultiLevelBiquad_init(This, numLevels, sampleRate, true, false, smoothUpdate);
    
    // change the number of channels to 4
    This->numChannels = 4;
    
    // Allocate memory for 5*numChannels coefficients per filter.
    free(This->coefficients_d);
    This->coefficients_d = malloc(numLevels*5*This->numChannels*sizeof(double));
    
    // start with all levels on bypass
    for (size_t i=0; i<numLevels; i++) {
        BMMultiLevelBiquad_setBypass(This, i);
    }
    
    // set 0db of gain
    BMMultiLevelBiquad_setGain(This,0.0f);
    
    // update the filter struct
    BMMultiLevelBiquad_recreate(This);
}





void BMMultiLevelBiquad_setGain(BMMultiLevelBiquad *This, float gain_db){
    BMSmoothGain_setGainDb(&This->gain, gain_db);
    BMSmoothGain_setGainDb(&This->gain2, gain_db);
}

void BMMultiLevelBiquad_setGainInstant(BMMultiLevelBiquad *This, float gain_db){
    BMSmoothGain_setGainDbInstant(&This->gain, gain_db);
    BMSmoothGain_setGainDbInstant(&This->gain2, gain_db);
}



void BMMultiLevelBiquad_queueUpdate(BMMultiLevelBiquad *This){
    This->needsUpdate = true;
}




inline void BMMultiLevelBiquad_updateNow(BMMultiLevelBiquad *This){
    
    if(This->useSmoothUpdate){
        // rate close to 1 mean it's change more slowly
        vDSP_biquadm_SetTargetsDouble(This->filterSetup, This->coefficients_d, 0.995, 0.05, 0, 0, This->numLevels, This->numChannels);
    }else{
        // update the coefficients
        vDSP_biquadm_SetCoefficientsDouble(This->filterSetup, This->coefficients_d, 0, 0, This->numLevels, This->numChannels);
    }
    
    This->needsUpdate = false;
}


void BMMultiLevelBiquad_create(BMMultiLevelBiquad *This){
        This->filterSetup = vDSP_biquadm_CreateSetup(This->coefficients_d, This->numLevels, This->numChannels);
}



inline void BMMultiLevelBiquad_recreate(BMMultiLevelBiquad *This){
        vDSP_biquadm_DestroySetup(This->filterSetup);
        This->filterSetup = vDSP_biquadm_CreateSetup(This->coefficients_d, This->numLevels, This->numChannels);
}




// we are doing this to change the name of the function from destroy to free
// without breaking old code that calls destroy
void BMMultiLevelBiquad_free(BMMultiLevelBiquad* This){
    // the pragma commands silence the compiler warning when we call this deprecated function
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
    BMMultiLevelBiquad_destroy(This);
    #pragma clang diagnostic pop
}




void BMMultiLevelBiquad_destroy(BMMultiLevelBiquad *This){
    if(This->coefficients_d) free(This->coefficients_d);
    This->coefficients_d = NULL;
    
    vDSP_biquadm_DestroySetup(This->filterSetup);
    This->filterSetup = NULL;
}



#pragma mark - Active level
void BMMultiLevelBiquad_updateActiveLevels(BMMultiLevelBiquad *This){
    if(This->needUpdateActiveLevels){
        This->needUpdateActiveLevels = false;
        vDSP_biquadm_SetActiveFilters(This->filterSetup, This->activeLevels);
    }
}


/*
 MultiLevelBiquad may contain many filters. We can active/disable each of them by the setActiveOnLevel function.
 */
void BMMultiLevelBiquad_setActiveOnLevel(BMMultiLevelBiquad *This,bool active,size_t level){
    //Keep track which should be active
    This->activeLevels[level] = active;
    This->needUpdateActiveLevels = true;
}

#pragma mark - Set filter parameters
void BMMultiLevelBiquad_setCoefficientZ(BMMultiLevelBiquad *This,size_t level,double* coeff){
    assert(level < This->numLevels);
    
    // for left and right channels, set coefficients
    for(size_t i=0; i < This->numChannels; i++){
        double* b0 = This->coefficients_d + level*This->numChannels*5 + i*5;
        double* b1 = b0 + 1;
        double* b2 = b0 + 2;
        double* a1 = b0 + 3;
        double* a2 = b0 + 4;
        
        *b0 = coeff[0 + i*5];
        *b1 = coeff[1 + i*5];
        *b2 = coeff[2 + i*5];
        *a1 = coeff[3 + i*5];
        *a2 = coeff[4 + i*5];
    }
    
    BMMultiLevelBiquad_queueUpdate(This);
}

//Bypass function is still allow filter to be processed. It only set all parameters back to 0 to achieve the bypass effects. If you want to actually disable it, call setActiveOnLevel function.
void BMMultiLevelBiquad_setBypass(BMMultiLevelBiquad *This, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designBypass());
}



// based on formula in 2.3.10 of Digital Filters for Everyone by Rusty Allred
void BMMultiLevelBiquad_setHighShelf(BMMultiLevelBiquad *This, float fc, float gain_db, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designHighShelf(fc, gain_db, This->sampleRate));
}





/*!
 *BMMultiLevelBiquad_setHighShelfAdjustableSlope
 *
 * @abstract adjustable high shelf filter from Robert Bristow-Johnson cookbook
 *
 * @param This pointer to an initialized filter struct
 * @param fc   filter cutoff frequency
 * @param gain_db shelf gain in decibels
 * @param slope in [0.3,1], where 0.5 is equivalent to first order shelf slope and 1 is equivalent to second order shelf slope
 * @param level the index of the filter in the biquad cascade
 */
void BMMultiLevelBiquad_setHighShelfAdjustableSlope(BMMultiLevelBiquad *This, float fc, float gain_db, float slope, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designHighShelfAdjustableSlope(fc, gain_db, slope, This->sampleRate));
}



void BMMultiLevelBiquad_setHighShelfFirstOrder(BMMultiLevelBiquad *This, float fc, float gain_db, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designHighShelfFirstOrder(fc, gain_db, This->sampleRate));
}





void BMMultiLevelBiquad_setLowShelfFirstOrder(BMMultiLevelBiquad *This, float fc, float gain_db, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designLowShelfFirstOrder(fc, gain_db, This->sampleRate));
}






// set a low shelf filter at on the specified level in both
// channels and update filter settings
// based on formula in 2.3.10 of Digital Filters for Everyone by Rusty Allred
void BMMultiLevelBiquad_setLowShelf(BMMultiLevelBiquad *This, float fc, float gain_db, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designLowShelf(fc, gain_db, This->sampleRate));
}




/*!
 *BMMultiLevelBiquad_setLowShelfAdjustableSlope
 *
 * @abstract adjustable high shelf filter from Robert Bristow-Johnson cookbook
 *
 * @param This pointer to an initialized filter struct
 * @param fc   filter cutoff frequency
 * @param gain_db shelf gain in decibels
 * @param slope in [0.3,1], where 0.5 is equivalent to first order shelf slope and 1 is equivalent to second order shelf slope
 * @param level the index of the filter in the biquad cascade
 */
void BMMultiLevelBiquad_setLowShelfAdjustableSlope(BMMultiLevelBiquad *This, float fc, float gain_db, float slope, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designLowShelfAdjustableSlope(fc, gain_db, slope, This->sampleRate));
}




/*!
 *BMMultiLevelBiquad_QToBW
 *
 * @abstract This keeps the filter width approximately constant as fc gets near Nyquist. It is not based on any theory; we derived it by observing and guessing.
 *
 * @param This pointer to an initialised struct
 * @param Q the Q factor of the filter
 * @param fc the cutoff frequency of the filter
 */
float BMMultiLevelBiquad_QToBWAtSampleRate(float Q, float fc, float sampleRate){
	float nyq = sampleRate / 2.0f;
	float c = fc / nyq;
	if(Q <= 1.0f)
		// for fc=0 return fc/Q. For fc=Nyquist return 0.5*fc
		// note that this means the Q does nothing when fc=Nyquist, but it still
		// works ok when fc is near Nyquist.
		return (fc/Q) * powf(0.5f*Q,c);
	// else Q > 1.0f
	// for fc=0 return fc/Q. For fc=Nyquist return 0.66*fc/Q
	return (fc / Q) * powf(0.66f,c);
}

float BMMultiLevelBiquad_QToBW(BMMultiLevelBiquad *This, float Q, float fc){
	return BMMultiLevelBiquad_QToBWAtSampleRate(Q, fc, This->sampleRate);
}






void BMMultiLevelBiquad_setBellQ(BMMultiLevelBiquad *This, float fc, float Q, float gain_db, size_t level){
    BMMultiLevelBiquad_setBell(This,
                               fc,
                               BMMultiLevelBiquad_QToBW(This,Q,fc),
                               gain_db,
                               level);
}






// based on formulae in 2.3.8 in Digital Filters are for Everyone,
// 2nd ed. by Rusty Allred
void BMMultiLevelBiquad_setBell(BMMultiLevelBiquad *This, float fc, float bandwidth, float gain_db, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designBell(fc, bandwidth, gain_db, This->sampleRate));
}






/*!
 *BMMultiLevelBiquad_setBellWithSkirt
 *
 * @abstract In addition to the usual controls for a bell filter, this function allows you to set the skirt gain, which is the gain at the DC and Nyquist frequencies.
 *
 *  @param This pointer to an initialised struct
 *  @param fc   bell centre frequency
 *  @param Q    Q = fc / bandwidth see Effect Design part 1 by Jon Dattorro for details
 *  @param bellGainDb the gain of the bell at peak or valley
 *  @param skirtGainDb the gain at DC and Nyquist
 *  @param level The index within this BMMultiLevelBiquad filter array
 */
void BMMultiLevelBiquad_setBellWithSkirt(BMMultiLevelBiquad *This, float fc, float Q, float bellGainDb, float skirtGainDb, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designBellWithSkirt(fc, Q, bellGainDb, skirtGainDb, This->sampleRate));
}





/*
  *This function approximates the integral of the magnitude transfer function
 * of a bell filter on [0,Nyquist], normalized so that the result is 1 when
 * the gain setting is 0 db.
 */
double approximateBellIntegral(BMMultiLevelBiquad *This,double bwHertz, double gainDb){
    // we normalize the bandwidth so that Nyquist = M_PI
    double bw = M_PI * bwHertz / (This->sampleRate/2.0f);
    
    // the following approximation was generated by Mathematica and output
    // using the "// CForm" command so that it could be entered below in
    // the exact form in which it came out from Mathematica. The method
    // of approximation was not clever and this approximation is accurate
    // only to about 92%. However, it obeys some important boundary conditions:
    //
    // 1. output is 1.0 when bw == 0
    // 2. output is 1.0 when gain == 0
    // 3. output is dbToGain(gainDb) when bw == Nyquist frequency
    //
    // positive gain approximation
    if(gainDb >= 0.0)
        return pow(pow(10.,gainDb/20.),0.07062002693643497*
                   pow(1. - 0.5797783118516802*pow(bw,0.47619047619047616),4.)*pow(bw,1.9047619047619047)*
                   gainDb + pow(bw,11./(11. + gainDb))/pow(M_PI,11./(11. + gainDb)));
    //
    // negative gain approximation
    return  pow(pow(10.,-gainDb/20.),-0.000282948025718253*
                pow(pow(bw,2.1) - 0.09036188195622674*pow(bw,4.2),1.5)*gainDb -
                pow(M_PI,25./(-25. + gainDb))/pow(bw,25./(-25. + gainDb)));
    
    // the following is the result of an exact symbolic integration of the integral
    //        double gainL = BM_DB_TO_GAIN(gainDb);
    //        if (gainL < 1.0)
    //           return (bw*M_PI*(cos(gainL/2.) + sin(gainL/2.)))
    //                    /
    //                  (bw*cos(gainL/2.) + sin(gainL/2.));
    //
    //        return (M_PI*(cos(gainL/2.) + bw*sin(gainL/2.)))
    //                /
    //               (cos(gainL/2.) + sin(gainL/2.));
}


/*
  *This is based on the setBell function above. The difference is that it
 * attempts to keep the overall gain at unity by adjusting the broadband
 * gain to compensate for the boost or cut of the bell filter. This allows
 * us to acheive extreme filter curves that approach the behavior of bandpass
 * and notch filters without clipping or loosing too much signal gain.
 */
void BMMultiLevelBiquad_setNormalizedBell(BMMultiLevelBiquad *This, float fc, float bandwidth, float controlGainDb, size_t level){
    assert(level < This->numLevels);
    
    // we start by finding out what gain we actually need to set on the bell
    // filter to place the peak at gain_db.
    // we iteratively converge on a value of gain_db that puts the peak of
    // of the bell near controlGainDb
    float gain_db = controlGainDb;
    float actualGain = FLT_MAX;
    while(fabs(actualGain - controlGainDb) > 0.1f){
        // how much will the gain be compensated, as compared to the bell filter
        float gainCompensation = BM_GAIN_TO_DB(1.0 / approximateBellIntegral(This,bandwidth, gain_db));
        // find out what the actual gain at fc will be
        actualGain = gain_db + gainCompensation;
        // adjust the gain setting of the bell filter to get the actual gain
        // closer to the gain control setting
        gain_db -= 0.5f * (actualGain - controlGainDb);
    }
    
    
    // compute the final gain compensation value in volt scale (not dB)
    float gainCompensationV = 1.0 / approximateBellIntegral(This,bandwidth, gain_db);
    
    // set the standard bell filter
    BMMultiLevelBiquad_setBell(This, fc, bandwidth, gain_db, level);
    
    // adjust the gain to keep the full spectrum magnitude near unity
    for(size_t i=0; i < This->numChannels; i++){
        
        double* b0 = This->coefficients_d + level*This->numChannels*5 + i*5;
        double* b1 = b0 + 1;
        double* b2 = b0 + 2;
        
        *b0 *= gainCompensationV;
        *b1 *= gainCompensationV;
        *b2 *= gainCompensationV;
    }
    
    BMMultiLevelBiquad_queueUpdate(This);
}


void BMMultiLevelBiquad_setLowPass12db(BMMultiLevelBiquad *This, double fc, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designLowPass12db(fc, This->sampleRate));
}





void BMMultiLevelBiquad_setLowPassQ12db(BMMultiLevelBiquad *This, double fc,double q, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designLowPassQ12db(fc, q, This->sampleRate));
}



void BMMultiLevelBiquad_setLowpass18db(BMMultiLevelBiquad *This,
                                       double fc,
                                       size_t firstLevel){
    // We need two biquad levels for this filter. Make sure we have space.
    assert(firstLevel + 1 < This->numLevels);
    
    // The third-order Butterworth polynomial is (s + 1)(s^2 + s + 1).
    // Therefore a third-order Butterworth filter can be factored into
    // a first-order filter followed by a second-order filter with Q=1.
    //
    // Reasoning:
    // 1. (s + 1) is the first-order Butterworth polynomial.
    // 2. The transfer function of an analog lowpass filter prototype
    //    with quality factor Q is 1 / (s^2 + s/Q + 1)
    // 3. (s^2 + s + 1) corresponds to a lowpass filter with Q = 1.
    
    // Set the first level to be the 1st order Butterworth lowpass
    BMMultiLevelBiquad_setLowPass6db(This, fc, firstLevel);
    
    // Set the second level to be the 2nd order lowpass with Q = 1
    double Q = 1.0;
    BMMultiLevelBiquad_setLowPassQ12db(This, fc, Q, firstLevel + 1);
}



void BMMultiLevelBiquad_setHighPass12db(BMMultiLevelBiquad *This, double fc,size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designHighPass12db(fc, This->sampleRate));
}




void BMMultiLevelBiquad_setHighPass12dbNeg(BMMultiLevelBiquad *This, double fc,size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designHighPass12dbNeg(fc, This->sampleRate));
}





void BMMultiLevelBiquad_setHighPassQ12db(BMMultiLevelBiquad *This, double fc,double q,size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designHighPassQ12db(fc, q, This->sampleRate));
}





void BMMultiLevelBiquad_setHighpass18db(BMMultiLevelBiquad *This,
                                        double fc,
                                        size_t firstLevel){
    // We need two biquad levels for this filter. Make sure we have space.
    assert(firstLevel + 1 < This->numLevels);
    
    // The third-order Butterworth polynomial is (s + 1)(s^2 + s + 1).
    // Therefore a third-order Butterworth filter can be factored into
    // a first-order filter followed by a second-order filter with Q=1.
    //
    // Reasoning:
    // 1. (s + 1) is the first-order Butterworth polynomial.
    // 2. The transfer function of an analog lowpass filter prototype
    //    with quality factor Q is 1 / (s^2 + s/Q + 1)
    // 3. (s^2 + s + 1) corresponds to a lowpass filter with Q = 1.
    
    // Set the first level to be the 1st order Butterworth highpass
    BMMultiLevelBiquad_setHighPass6db(This, fc, firstLevel);
    
    // Set the second level to be the 2nd order highpass with Q = 1
    double Q = 1.0;
    BMMultiLevelBiquad_setHighPassQ12db(This, fc, Q, firstLevel + 1);
}





void BMMultiLevelBiquad_setLinkwitzRileyLP(BMMultiLevelBiquad *This, double fc, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designLinkwitzRileyLP(fc, This->sampleRate));
}








void BMMultiLevelBiquad_setLinkwitzRileyHP(BMMultiLevelBiquad *This, double fc, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designLinkwitzRileyHP(fc, This->sampleRate));
}




void BMMultiLevelBiquad_setLinkwitzRileyLP4thOrder(BMMultiLevelBiquad *This, double fc, size_t firstLevel){
    
    // a cascade of two butterworth filters makes a linkwitz-riley filter.
    // the highpass and lowpass pair sums to unity gain without the need to
    // invert the sign of one pair
    BMMultiLevelBiquad_setLowPass12db(This, fc, firstLevel);
    BMMultiLevelBiquad_setLowPass12db(This, fc, firstLevel+1);
}




void BMMultiLevelBiquad_setLinkwitzRileyHP4thOrder(BMMultiLevelBiquad *This, double fc, size_t firstLevel){
    
    // a cascade of two butterworth filters makes a linkwitz-riley filter.
    // the highpass and lowpass pair sums to unity gain without the need to
    // invert the sign of one pair
    BMMultiLevelBiquad_setHighPass12db(This, fc, firstLevel);
    BMMultiLevelBiquad_setHighPass12db(This, fc, firstLevel+1);
}




void BMMultiLevelBiquad_setLowPass6db(BMMultiLevelBiquad *This, double fc, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designLowPass6db(fc, This->sampleRate));
}


/*!
 *packFirstOrder
 *
 * packs two first order filters into a single biquad section. The first six inputs are the coefficients of the two first order filters. the last five outputs are the packed biquad coefficients.
 */
void packFirstOrder(double b0a, double b1a, double a1a,
                    double b0b, double b1b, double a1b,
                    double* b0, double* b1, double* b2, double* a1, double* a2){
    *b0 = b0a * b0b;
    *b1 = (b0a*b1b) + (b0b*b1a);
    *b2 = b1a * b1b;
    
    *a1 = a1a + a1b;
    *a2 = a1a * a1b;
}



// packs two first order filters into a single biquad section
void BMMultiLevelBiquad_setHighPassLowPass(BMMultiLevelBiquad *This, double highPassFc, double lowPassFc, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designHighPassLowPass(highPassFc, lowPassFc, This->sampleRate));
}





void BMMultiLevelBiquad_setHighPass6db(BMMultiLevelBiquad *This, double fc, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designHighPass6db(fc, This->sampleRate));
}



void BMMultilevelBiquad_setAllpass2ndOrder(BMMultiLevelBiquad *This, double c1, double c2, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designAllpass2ndOrder(c1, c2));
}






void BMMultilevelBiquad_setAllpass1stOrder(BMMultiLevelBiquad *This, double c, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designAllpass1stOrder(c));
}




void BMMultilevelBiquad_setCriticallyDampedPhaseCompensator(BMMultiLevelBiquad  *This, double lowpassFC, size_t level){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designCriticallyDampedPhaseCompensator(lowpassFC, This->sampleRate));
}




/*
 * Computes the coefficient of the s^1 term in butterworth
 * polynomials
 *
 * Mathematica prototype
 *
 * BTWC[N_, k_] :=
 * -2 Cos[\[Pi] (2 k + N - 1)/(2 N)]
 *
 * reference: https://en.wikipedia.org/wiki/Butterworth_filter#Normalized_Butterworth_polynomials
 */
double butterworthCHelper(size_t filterOrder, size_t sectionNumber){
    double k = sectionNumber;
    double N = filterOrder;
    return -2.0 * cos(M_PI * (2.0 * k + N - 1) / (2.0 * N));
}



// DigitalToAnalogFcWarp[omega_] := 2 Tan[omega /2]
double digitalToAnalogFCWarp(float fcDigital){
    return 2.0 * tan(fcDigital / 2.0);
}




/*
 * sets up a single section of a butterworth lowpass filter that
 * is factored across several biquad sections.
 *
 * @param fc  filter cutoff in hz
 */
void BMMultiLevelBiquad_setBWLPSection(BMMultiLevelBiquad *This,
                                       double fc,
                                       size_t level,
                                       size_t filterOrder,
                                       size_t sectionNumber){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designBWLPSection(fc, filterOrder, sectionNumber, This->sampleRate));
}






/*
 * sets up a butterworth lowpass filter of order 2*numLevels
 *
 * @param fc           cutoff frequency in hz
 * @param firstLevel   the filter will consume numLevels contiguous
 *                     biquad sections, beginning with firstLevel
 * @param numLevels    number of biquad sections to use (order / 2)
 */
void BMMultiLevelBiquad_setHighOrderBWLP(BMMultiLevelBiquad *This, double fc, size_t firstLevel, size_t numLevels){
    
    // N is the filter order
    size_t N = numLevels * 2;
    
    for(size_t i=0; i<numLevels; i++)
        BMMultiLevelBiquad_setBWLPSection(This, fc, i+firstLevel, N, i+1);
}


bool BMMLBQ_isEven(size_t x){
    return x % 2 == 0;
}



/*BMMultiLevelBiquad_setLS2OSection
 *
 * set up a second-order section of a low-shelf filter of arbitrary order
 */
void BMMultiLevelBiquad_setLS2OSection(BMMultiLevelBiquad *This,
                                       double fc,
                                       double gain_v,
                                       size_t order,
                                       size_t section,
                                       size_t level){
    assert(level < This->numLevels);
    
    double M = order;
    double m = section;
    double g = gain_v;
    double B = fc;
    double K = tan(M_PI * B / This->sampleRate);
    
    // Mathematica prototype for second-order shelf denominator coefficients:
    // {
    //    Power(g,1/M) + Power(K,2) - 2*Power(g,1/(2.*M))*K*Sin(((-1 + 2*m)*Pi)/(2.*M)),
    // -2*Power(g,1/M) + 2*Power(K,2),
    //    Power(g,1/M) + Power(K,2) + 2*Power(g,1/(2.*M))*K*Sin(((-1 + 2*m)*Pi)/(2.*M))
    // }
    double a2 =     pow(g,1./M) +   pow(K,2.) - 2.*pow(g,1./(2.*M))*K*sin(((-1. + 2.*m)*M_PI)/(2.*M));
    double a1 = -2.*pow(g,1./M) + 2*pow(K,2.);
    double a0 =     pow(g,1./M) +   pow(K,2.) + 2.*pow(g,1./(2.*M))*K*sin(((-1. + 2.*m)*M_PI)/(2.*M));
    
    // Mathematica prototype for second-order low-shelf numerator coefficients:
    // {
    //    Power(g,1/M) +   Power(g,2/M)*Power(K,2) - 2*Power(g,3/(2.*M))*K*Sin(((-1 + 2*m)*Pi)/(2.*M)),
    // -2*Power(g,1/M) + 2*Power(g,2/M)*Power(K,2),
    //    Power(g,1/M) +   Power(g,2/M)*Power(K,2) + 2*Power(g,3/(2.*M))*K*Sin(((-1 + 2*m)*Pi)/(2.*M))
    // }
    double b2 = pow(g,1./M) +   pow(g,2./M)*pow(K,2.) - 2.*pow(g,3./(2.*M))*K*sin(((-1. + 2.*m)*M_PI)/(2.*M));
    double b1 = -2.*pow(g,1./M) + 2.*pow(g,2./M)*pow(K,2.);
    double b0 = pow(g,1./M) +   pow(g,2./M)*pow(K,2.) + 2.*pow(g,3./(2.*M))*K*sin(((-1. + 2.*m)*M_PI)/(2.*M));
    
    // normalize so a0 = 1.0;
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 = 1.0;
    
//    printf("\n2nd order Low-shelf\n");
//    printf(" gain = %f\n", gain_v);
//    printf("   fc = %f\n", fc);
//    printf("{1.0, %f, %f, %f, %f, %f}\n", a1, a2, b0, b1, b2);

    
    // for each channel
    for(size_t i=0; i < This->numChannels; i++){
        
        // get pointers to the filter coefficients for this channel
        double* b0p = This->coefficients_d + level*This->numChannels*5 + i*5;
        double* b1p = b0p + 1;
        double* b2p = b1p + 1;
        double* a1p = b2p + 1;
        double* a2p = a1p + 1;
        
        // set the coefficients as calculated above
        *b0p = b0;
        *b1p = b1;
        *b2p = b2;
        *a1p = a1;
        *a2p = a2;
        
//        // hardcode the coefficients for testing
//        printf("Hardcoded coefficients! Remove this code!\n");
//        *b0p = 0.97728675076025017;
//        *b1p = -1.802568811373876;
//        *b2p = 0.83627899020705332;
//        *a1p = -1.7988364314277792;
//        *a2p = 0.81729812091340048;
    }
    
    BMMultiLevelBiquad_queueUpdate(This);
}




/*BMMultiLevelBiquad_setLS1OSection
 *
 * set up the first-order section of a low-shelf filter of odd order
 */
void BMMultiLevelBiquad_setLS1OSection(BMMultiLevelBiquad *This,
                                       double fc,
                                       double gain_v,
                                       size_t order,
                                       size_t level){
    assert(level < This->numLevels);
    
    double M = order;
    double g = gain_v;
    double B = fc;
    double K = tan(M_PI * B / This->sampleRate);
    
    // Mathematica prototype for second-order shelf denominator coefficients:
    // {
    //     -Power(g,1/(2.*M)) + K,
    //   Power(g,1/(2.*M)) + K,
    //   0
    // }
    double a2 =  0.0;
    double a1 = -pow(g,1./(2.*M)) + K;
    double a0 =  pow(g,1./(2.*M)) + K;

    
    // Mathematica:
    // {
    //   -Power(g,1/(2.*M)) + Power(g,1/M)*K,
    //    Power(g,1/(2.*M)) + Power(g,1/M)*K
    // }
    double b2 =  0.0;
    double b1 = -pow(g,1./(2.*M)) + pow(g,1./M)*K;
    double b0 =  pow(g,1./(2.*M)) + pow(g,1./M)*K;
    
    // normalize so a0 = 1.0;
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 = 1.0;
    
//    printf("\n1st order Low-shelf\n");
//    printf(" gain = %f\n", gain_v);
//    printf("   fc = %f\n", fc);
//    printf("{1.0, %f, %f, %f, %f, %f}\n", a1, a2, b0, b1, b2);
    
    // for each channel
    for(size_t i=0; i < This->numChannels; i++){
        
        // get pointers to the filter coefficients for this channel
        double* b0p = This->coefficients_d + level*This->numChannels*5 + i*5;
        double* b1p = b0p + 1;
        double* b2p = b1p + 1;
        double* a1p = b2p + 1;
        double* a2p = a1p + 1;
        
        // set the coefficients as calculated above
        *b0p = b0;
        *b1p = b1;
        *b2p = b2;
        *a1p = a1;
        *a2p = a2;
        
    }
    
    BMMultiLevelBiquad_queueUpdate(This);
}




/*BMMultiLevelBiquad_setHS2OSection
 *
 * set up a second-order section of a high-shelf filter of arbitrary order
 */
void BMMultiLevelBiquad_setHS2OSection(BMMultiLevelBiquad *This,
                                       double fc,
                                       double gain_v,
                                       size_t order,
                                       size_t section,
                                       size_t level){
    assert(level < This->numLevels);
    
    // The coefficient calculation for the high-shelf filter is the same as the
    // low-shelf except that the Bandwidth, B is calculated as the distance from
    // fc to Nyquist and the a1 and b1 terms are negated.
    
    double M = order;
    double m = section;
    double g = gain_v;
    double B = (This->sampleRate / 2.0) - fc; // different from low-shelf
    double K = tan(M_PI * B / This->sampleRate);
    
    // ALL FORMULAE BELOW ARE COPIED VERBATIM FROM THE LOW-SHELF FILTER
    //
    // Mathematica prototype for second-order shelf denominator coefficients:
    // {
    //    Power(g,1/M) + Power(K,2) - 2*Power(g,1/(2.*M))*K*Sin(((-1 + 2*m)*Pi)/(2.*M)),
    // -2*Power(g,1/M) + 2*Power(K,2),
    //    Power(g,1/M) + Power(K,2) + 2*Power(g,1/(2.*M))*K*Sin(((-1 + 2*m)*Pi)/(2.*M))
    // }
    double a2 =     pow(g,1./M) +   pow(K,2.) - 2.*pow(g,1./(2.*M))*K*sin(((-1. + 2.*m)*M_PI)/(2.*M));
    double a1 = -2.*pow(g,1./M) + 2*pow(K,2.);
    double a0 =     pow(g,1./M) +   pow(K,2.) + 2.*pow(g,1./(2.*M))*K*sin(((-1. + 2.*m)*M_PI)/(2.*M));
    
    // Mathematica prototype for second-order low-shelf numerator coefficients:
    // {
    //    Power(g,1/M) +   Power(g,2/M)*Power(K,2) - 2*Power(g,3/(2.*M))*K*Sin(((-1 + 2*m)*Pi)/(2.*M)),
    // -2*Power(g,1/M) + 2*Power(g,2/M)*Power(K,2),
    //    Power(g,1/M) +   Power(g,2/M)*Power(K,2) + 2*Power(g,3/(2.*M))*K*Sin(((-1 + 2*m)*Pi)/(2.*M))
    // }
    double b2 = pow(g,1./M) +   pow(g,2./M)*pow(K,2.) - 2.*pow(g,3./(2.*M))*K*sin(((-1. + 2.*m)*M_PI)/(2.*M));
    double b1 = -2.*pow(g,1./M) + 2.*pow(g,2./M)*pow(K,2.);
    double b0 = pow(g,1./M) +   pow(g,2./M)*pow(K,2.) + 2.*pow(g,3./(2.*M))*K*sin(((-1. + 2.*m)*M_PI)/(2.*M));
    
    // negate the b1 and a1 terms to transform low-shelf into high-shelf
    a1 *= -1.0;
    b1 *= -1.0;
    
    // normalize so a0 = 1.0;
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 /= a0;
    
//    printf("\n2nd order High-shelf\n");
//    printf(" gain = %f\n", gain_v);
//    printf("   fc = %f\n", fc);
//    printf("{%f, %f, %f, %f, %f, %f}\n", a0, a1, a2, b0, b1, b2);

    
    // for each channel
    for(size_t i=0; i < This->numChannels; i++){
        
        // get pointers to the filter coefficients for this channel
        double* b0p = This->coefficients_d + level*This->numChannels*5 + i*5;
        double* b1p = b0p + 1;
        double* b2p = b1p + 1;
        double* a1p = b2p + 1;
        double* a2p = a1p + 1;
        
        // set the coefficients as calculated above
        *b0p = b0;
        *b1p = b1;
        *b2p = b2;
        *a1p = a1;
        *a2p = a2;
    }
    
    BMMultiLevelBiquad_queueUpdate(This);
}




/*BMMultiLevelBiquad_setHS1OSection
 *
 * set up the first-order section of a high-shelf filter of odd order
 */
void BMMultiLevelBiquad_setHS1OSection(BMMultiLevelBiquad *This,
                                       double fc,
                                       double gain_v,
                                       size_t order,
                                       size_t level){
    assert(level < This->numLevels);
    
    double M = order;
    double g = gain_v;
    double B = (This->sampleRate / 2.0) - fc; // different from low-shelf
    double K = tan(M_PI * B / This->sampleRate);
    
    // THE FORMULAE BELOW ARE COPIED VERBATIM FROM THE LOW-SHELF VERSION
    //
    // Mathematica prototype for second-order shelf denominator coefficients:
    // {
    //   0,
    //     -Power(g,1/(2.*M)) + K,
    //   Power(g,1/(2.*M)) + K,
    // }
    double a2 =  0.0;
    double a1 = -pow(g,1./(2.*M)) + K;
    double a0 =  pow(g,1./(2.*M)) + K;
    
    // Mathematica:
    // {
    //   -Power(g,1/(2.*M)) + Power(g,1/M)*K,
    //    Power(g,1/(2.*M)) + Power(g,1/M)*K
    // }
    double b2 =  0.0;
    double b1 = -pow(g,1./(2.*M)) + pow(g,1./M)*K;
    double b0 =  pow(g,1./(2.*M)) + pow(g,1./M)*K;
    
    // negate the b1 and a1 terms to transform low-shelf into high-shelf
    a1 *= -1.0;
    b1 *= -1.0;
    
    // normalize so a0 = 1.0;
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 = 1.0;
    
//    printf("\n1st order High-shelf\n");
//    printf(" gain = %f\n", gain_v);
//    printf("   fc = %f\n", fc);
//    printf("{1.0, %f, %f, %f, %f, %f}\n", a1, a2, b0, b1, b2);
    
    // for each channel
    for(size_t i=0; i < This->numChannels; i++){
        
        // get pointers to the filter coefficients for this channel
        double* b0p = This->coefficients_d + level*This->numChannels*5 + i*5;
        double* b1p = b0p + 1;
        double* b2p = b1p + 1;
        double* a1p = b2p + 1;
        double* a2p = a1p + 1;
        
        // set the coefficients as calculated above
        *b0p = b0;
        *b1p = b1;
        *b2p = b2;
        *a1p = a1;
        *a2p = a2;

    }
    
    BMMultiLevelBiquad_queueUpdate(This);
    
}




void BMMultiLevelBiquad_setLowShelfHighOrder(BMMultiLevelBiquad *This, double fc, double gain_db, size_t order, size_t firstLevel, size_t numLevels){
    
    if(BMMLBQ_isEven(order))
        assert(numLevels = order / 2);
    else
        assert(numLevels = 1 + (order / 2));
    
    double gain_v = BM_DB_TO_GAIN(gain_db);
    
    // set the second order sections
    for(size_t i=0; i<order/2; i++){
        size_t section = i + 1;
        BMMultiLevelBiquad_setLS2OSection(This, fc, gain_v, order, section, firstLevel + i);
    }
    
    // set the first order section if there is one
    if (!BMMLBQ_isEven(order))
        BMMultiLevelBiquad_setLS1OSection(This, fc, gain_v, order, firstLevel + order/2);
}




void BMMultiLevelBiquad_setHighShelfHighOrder(BMMultiLevelBiquad *This, double fc, double gain_db, size_t order, size_t firstLevel, size_t numLevels){
    
    if(order % 2 == 0)
        assert(numLevels = order / 2);
    else
        assert(numLevels = 1 + (order / 2));
    
    double gain_v = BM_DB_TO_GAIN(gain_db);
    
    // set the second order sections
    for(size_t i=0; i<order/2; i++){
        size_t section = i + 1;
        BMMultiLevelBiquad_setHS2OSection(This, fc, gain_v, order, section, firstLevel + i);
    }
    
    // set the first order section if there is one
    if (!BMMLBQ_isEven(order))
        BMMultiLevelBiquad_setHS1OSection(This, fc, gain_v, order, firstLevel + order/2);
}





/*
 * generate the s-domain coefficients for the analog prototype filter
 *
 * @param sectionNumber  biquad section number (1 is first section)
 * @param output         {a0, a1, a2, b0, b1, b2}
 */
void BMMultiLevelBiquad_getAnalogLegendreLPSection(size_t filterOrder,
                                                   size_t sectionNumber,
                                                   float* output){
    
    // filter order is an even number in [2,10]
    assert(filterOrder <= 10 && filterOrder >= 2);
    assert(filterOrder % 2 == 0);
    
    // number of sections is filterOrder / 2
    assert(sectionNumber <= filterOrder / 2);
    
    // o2s1 = {1, 1.4142, 1, 0, 0, 1};
    if (filterOrder == 2){
        float coefficients [6] = {1, 1.4142, 1, 0, 0, 1};
        memcpy(output, coefficients, sizeof(float)*6);
        return;
    }
    
    // o4s1 = {1, 1.0994, 0.4308, 0, 0, 0.4308};
    // o4s2 = {1, 0.4634, 0.9477, 0, 0, 0.9477};
    if (filterOrder == 4){
        if (sectionNumber == 1){
            float coefficients [6] = {1, 1.0994, 0.4308, 0, 0, 0.4308};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 2){
            float coefficients [6] = {1, 0.4634, 0.9477, 0, 0, 0.9477};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
    }
    
    // o6s1 = {1, 0.6180, 0.5830, 0, 0, 0.5830};
    // o6s2 = {1, 0.8778, 0.2502, 0, 0, 0.2502};
    // o6s3 = {1, 0.2304, 0.9696, 0, 0, 0.9696};
    if (filterOrder == 6){
        if (sectionNumber == 1){
            float coefficients [6] = {1, 0.6180, 0.5830, 0, 0, 0.5830};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 2){
            float coefficients [6] = {1, 0.8778, 0.2502, 0, 0, 0.2502};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 3){
            float coefficients [6] = {1, 0.2304, 0.9696, 0, 0, 0.9696};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
    }
    
    // o8s1 = {1, 0.6006, 0.3829, 0, 0, 0.3829};
    // o8s2 = {1, 0.3886, 0.7180, 0, 0, 0.7180};
    // o8s3 = {1, 0.13788, 0.9809, 0, 0, 0.9809};
    // o8s4 = {1, 0.7344, 0.1676, 0, 0, 0.1676};
    if (filterOrder == 8){
        if (sectionNumber == 1){
            float coefficients [6] = {1, 0.6006, 0.3829, 0, 0, 0.3829};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 2){
            float coefficients [6] = {1, 0.3886, 0.7180, 0, 0, 0.7180};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 3){
            float coefficients [6] = {1, 0.13788, 0.9809, 0, 0, 0.9809};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 4){
            float coefficients [6] = {1, 0.7344, 0.1676, 0, 0, 0.1676};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
    }
    
    // o10s1 = {1, 0.5548, 0.2702, 0, 0, 0.2702};
    // o10s2 = {1, 0.0918, 0.9870, 0, 0, 0.9870};
    // o10s3 = {1, 0.4284, 0.5282, 0, 0, 0.5282};
    // o10s4 = {1, 0.2650, 0.8013, 0, 0, 0.8013};
    // o10s5 = {1, 0.6344, 0.1218, 0, 0, 0.1218};
    if (filterOrder == 10){
        if (sectionNumber == 1){
            float coefficients [6] = {1, 0.5548, 0.2702, 0, 0, 0.2702};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 2){
            float coefficients [6] = {1, 0.0918, 0.9870, 0, 0, 0.9870};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 3){
            float coefficients [6] = {1, 0.4284, 0.5282, 0, 0, 0.5282};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 4){
            float coefficients [6] = {1, 0.2650, 0.8013, 0, 0, 0.8013};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
        if (sectionNumber == 5){
            float coefficients [6] = {1, 0.6344, 0.1218, 0, 0, 0.1218};
            memcpy(output, coefficients, sizeof(float)*6);
            return;
        }
    }
}




/*
 * adjust the coefficients of an analog s-domain prototype with cutoff
 * frequency at 1 radian so that the cutoff frequency will be omega radians
 * after we do the bilinear transform to go to the digital domain.
 *
 * @param omega             desired cutoff frequency in z domain, in radians
 * @param coefficientListIn s-domain prototype filter coefficients {a0, a1, a2, b0, b1, b2}
 * @param coefficientListOut s-domain coefficients after warping {a0, a1, a2, b0, b1, b2}
 */
void BMMuiltiLevelBiquad_freqWarpSDomain(float omega,
                                         const float* coefficientListIn,
                                         float* coefficientListOut){
    
    coefficientListOut[0] = coefficientListIn[0] / (omega*omega);
    coefficientListOut[1] = coefficientListIn[1] / omega;
    coefficientListOut[2] = coefficientListIn[2];
    coefficientListOut[3] = coefficientListIn[3] / (omega*omega);
    coefficientListOut[4] = coefficientListIn[4] / omega;
    coefficientListOut[5] = coefficientListIn[5];
    
}




/*
 * Convert the coefficient list of an s-domain filter to the z-domain
 * coefficients list using the bilinear transform:
 *
 * s = (z-1)/(z+1)
 *
 * @param sDomainCoefficients   {a0, a1, a2, b0, b1, b2}
 * @param zDomainCoefficients   {a0, a1, a2, b0, b1, b2}
 */
void BMMuiltiLevelBiquad_coefficientsStoZ(const float* sDomainCoefficients,
                                          float* zDomainCoefficients){
    float a0s = sDomainCoefficients[0];
    float a1s = sDomainCoefficients[1];
    float a2s = sDomainCoefficients[2];
    float b0s = sDomainCoefficients[3];
    float b1s = sDomainCoefficients[4];
    float b2s = sDomainCoefficients[5];
    
    // {a0s + a1s + a2s, -2 a0s + 2 a2s, a0s - a1s + a2s,
    //  b0s + b1s + b2s, -2 b0s + 2 b2s, b0s - b1s + b2s}
    zDomainCoefficients[0] = a0s + a1s + a2s;
    zDomainCoefficients[1] = -2.0 * a0s + 2.0 * a2s;
    zDomainCoefficients[2] = a0s - a1s + a2s;
    zDomainCoefficients[3] = b0s + b1s + b2s;
    zDomainCoefficients[4] = -2.0 * b0s + 2.0 * b2s;
    zDomainCoefficients[5] = b0s - b1s + b2s;
}





/*
 * Normalize the a0 coefficient to 1
 *
 * @param inputCoefficients          {a0, a1, a2, b0, b1, b2}
 * @param normalizedA0Coefficients   {a0, a1, a2, b0, b1, b2}
 */
void BMMuiltiLevelBiquad_NormalizeA0(const float* inputCoefficients,
                                     float* normalizedA0Coefficients){
    float a0 = inputCoefficients[0];
    float a1 = inputCoefficients[1];
    float a2 = inputCoefficients[2];
    float b0 = inputCoefficients[3];
    float b1 = inputCoefficients[4];
    float b2 = inputCoefficients[5];
    
    normalizedA0Coefficients[0] = 1.0;
    normalizedA0Coefficients[1] = a1/a0;
    normalizedA0Coefficients[2] = a2/a0;
    normalizedA0Coefficients[3] = b0/a0;
    normalizedA0Coefficients[4] = b1/a0;
    normalizedA0Coefficients[5] = b2/a0;
}






/*
 * sets up a single section of a Legendre lowpass filter that
 * is factored across several biquad sections.
 *
 * @param fc  filter cutoff in hz
 */
void BMMultiLevelBiquad_setLegendreLPSection(BMMultiLevelBiquad *This,
                                             double fc,
                                             size_t level,
                                             size_t filterOrder,
                                             size_t sectionNumber){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designLegendreLPSection(fc, filterOrder, sectionNumber, This->sampleRate));
}






/*
 * sets up a Legendre Lowpass filter of order 2*numLevels
 *
 * @param firstLevel   the filter will consume numLevels contiguous
 *                     biquad sections, beginning with firstLevel
 * @param numLevels    number of biquad sections to use. numLevels = (filterOrder / 2)
 */
void BMMultiLevelBiquad_setLegendreLP(BMMultiLevelBiquad *This, double fc, size_t firstLevel, size_t numLevels){
    
    // N is the filter order
    size_t N = numLevels * 2;
    
    for(size_t i=0; i<numLevels; i++)
        BMMultiLevelBiquad_setLegendreLPSection(This, fc, i+firstLevel, N, i+1);
}






/*!
 * besselFilterSectionGetQAndFcMultiplier
 *
 * @param filterOrder     the order of the filter, must be even
 * @param sectionNumber   the filter is divided into second order sections, numbered starting with one
 * @param fcMultiplier    multiply this number by the desired cutoff frequency when you set the cutoff for this second order section
 * @param Q               the Q for this section
 *
 * @abstract gets the Q and the factor to be multiplied by fc to build a bessel lowpass filter by cascading 2nd order adjustable Q lowpass filters. Source: https://gist.github.com/endolith/4982787 .
 * @discussion this method is questionable because the bilinear transform does not preserve group delay when converting bessel filters from the s to the z domain. We have not tested to see how well this method works in that respect. However, these filters are mainly intended to be used for smoothing low-frequency control signals, where the cutoff will be near zero, so this issue will not be a problem.
 */
void besselFilterSectionGetQAndFcMultiplier(size_t filterOrder,
                                            size_t sectionNumber,
                                            float* fcMultiplier,
                                            float* Q){
    assert(filterOrder % 2 == 0);
    assert(sectionNumber <= filterOrder/2);
    
    /*
     https://gist.github.com/endolith/4982787
     2: 0.57735026919
     4: 0.805538281842 0.521934581669
     6: 1.02331395383  0.611194546878 0.510317824749
     8: 1.22566942541  0.710852074442 0.559609164796 0.505991069397
     10: 1.41530886916  0.809790964842 0.620470155556 0.537552151325 0.503912727276
     12: 1.59465693507  0.905947107025 0.684008068137 0.579367238641 0.525936202016 0.502755558204
     14: 1.76552743493  0.998998442993 0.747625068271 0.624777082395 0.556680772868 0.519027293158 0.502045428643
     16: 1.9292718407   1.08906376917  0.810410302962 0.671382379377 0.591144659703 0.542678365981 0.514570953471 0.501578400482
     18: 2.08691792612  1.17637337045  0.872034231424 0.718163551101 0.627261751983 0.569890924765 0.533371782078 0.511523796759 0.50125489338
     20: 2.23926560629  1.26117120993  0.932397288146 0.764647810579 0.664052481472 0.598921924986 0.555480327396 0.526848630061 0.509345928377 0.501021580965
     */
    float QArray [10][10] = {
        {0.57735026919f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {0.805538281842f, 0.521934581669f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {1.02331395383f,  0.611194546878f, 0.510317824749f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {1.22566942541f,  0.710852074442f, 0.559609164796f, 0.505991069397f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {1.41530886916f,  0.809790964842f, 0.620470155556f, 0.537552151325f, 0.503912727276f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {1.59465693507f,  0.905947107025f, 0.684008068137f, 0.579367238641f, 0.525936202016f, 0.502755558204f,0.0f,0.0f,0.0f,0.0f},
        {1.76552743493f,  0.998998442993f, 0.747625068271f, 0.624777082395f, 0.556680772868f, 0.519027293158f, 0.502045428643f,0.0f,0.0f,0.0f},
        {1.9292718407f,   1.08906376917f,  0.810410302962f, 0.671382379377f, 0.591144659703f, 0.542678365981f, 0.514570953471f, 0.501578400482f, 0.0f, 0.0f},
        {2.08691792612f,  1.17637337045f,  0.872034231424f, 0.718163551101f, 0.627261751983f, 0.569890924765f, 0.533371782078f, 0.511523796759f, 0.50125489338f,0.0f},
        {2.23926560629f,  1.26117120993f,  0.932397288146f, 0.764647810579f, 0.664052481472f, 0.598921924986f, 0.555480327396f, 0.526848630061f, 0.509345928377f, 0.501021580965f}
    };
    
    /*
     2: 1.27201964951
     4: 1.60335751622   1.43017155999
     6: 1.9047076123    1.68916826762   1.60391912877
     8: 2.18872623053   1.95319575902   1.8320926012    1.77846591177
     10: 2.45062684305   2.20375262593   2.06220731793   1.98055310881   1.94270419166
     12: 2.69298925084   2.43912611431   2.28431825401   2.18496722634   2.12472538477   2.09613322542
     14: 2.91905714471   2.66069088948   2.49663434571   2.38497976939   2.30961462222   2.26265746534   2.24005716132
     16: 3.13149167404   2.87016099416   2.69935018044   2.57862945683   2.49225505119   2.43227707449   2.39427710712   2.37582307687
     18: 3.33237300564   3.06908580184   2.89318259511   2.76551588399   2.67073340527   2.60094950474   2.55161764546   2.52001358804   2.50457164552
     20: 3.52333123464   3.25877569704   3.07894353744   2.94580435024   2.84438325189   2.76691082498   2.70881411245   2.66724655259   2.64040228249   2.62723439989
     
     */
    float fMulArray [10][10] = {
        {1.27201964951f, 0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {1.60335751622f, 1.43017155999f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {1.9047076123f,    1.68916826762f,   1.60391912877f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {2.18872623053f,   1.95319575902f,   1.8320926012f,    1.77846591177f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {2.45062684305f,   2.20375262593f,   2.06220731793f,   1.98055310881f,   1.94270419166f,0.0f,0.0f,0.0f,0.0f,0.0f},
        {2.69298925084f,   2.43912611431f,   2.28431825401f,   2.18496722634f,   2.12472538477f,   2.09613322542f,0.0f,0.0f,0.0f,0.0f},
        {2.91905714471f,   2.66069088948f,   2.49663434571f,   2.38497976939f,   2.30961462222f,   2.26265746534f,   2.24005716132f,0.0f,0.0f,0.0f},
        {3.13149167404f,   2.87016099416f,   2.69935018044f,   2.57862945683f,   2.49225505119f,   2.43227707449f,   2.39427710712f,   2.37582307687f,0.0f,0.0f},
        {3.33237300564f,   3.06908580184f,   2.89318259511f,   2.76551588399f,   2.67073340527f,   2.60094950474f,   2.55161764546f,   2.52001358804f,   2.50457164552f,0.0f},
        {3.52333123464f,   3.25877569704f,   3.07894353744f,   2.94580435024f,   2.84438325189f,   2.76691082498f,   2.70881411245f,   2.66724655259f,   2.64040228249f,   2.62723439989f}
    };
    
    *Q = QArray[(filterOrder/2)-1][sectionNumber-1];
    *fcMultiplier = fMulArray[(filterOrder/2)-1][sectionNumber-1];
}






/*!
 * BMMultiLevelBiquad_setBesselLPSection
 *
 * @param fc            filter cutoff in hz
 * @param level         level in the BMMultiLevelBiquad filter cascade
 * @param filterOrder   must be an even number
 * @param sectionNumber because there could be other unrelated filters in the cascade, level = startLevel + sectionNumber - 1. Note that section numbers start from one, not zero.
 *
 * @abstract sets up a single section of a Bessel lowpass filter that is factored across several biquad sections.
 */
void BMMultiLevelBiquad_setBesselLPSection(BMMultiLevelBiquad *This,
                                           double fc,
                                           size_t level,
                                           size_t filterOrder,
                                           size_t sectionNumber){
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designBesselLPSection(fc, filterOrder, sectionNumber, This->sampleRate));
}


/*!
 * BMMultiLevelBiquad_setBesselLP
 *
 * @abstract sets up a Bessel Lowpass filter of order 2*numLevels
 *
 * @param fc           cutoff frequency
 * @param firstLevel   the filter will consume numLevels contiguous
 *                     biquad sections, beginning with firstLevel
 * @param numLevels    number of biquad sections to use. numLevels = (filterOrder / 2)
 */
void BMMultiLevelBiquad_setBesselLP(BMMultiLevelBiquad *This, double fc, size_t firstLevel, size_t numLevels){
    
    // N is the filter order
    size_t N = numLevels * 2;
    
    for(size_t i=0; i<numLevels; i++)
        BMMultiLevelBiquad_setBesselLPSection(This, fc, i+firstLevel, N, i+1);
}






/*!
 * BMMultiLevelBiquad_setCriticallyDampedLPSection
 *
 * @param fc            filter cutoff in hz
 * @param level         level in the BMMultiLevelBiquad filter cascade
 * @param filterOrder   must be an even number
 *
 * @abstract sets up a single section of a Bessel lowpass filter that is factored across several biquad sections.
 */
void BMMultiLevelBiquad_setCriticallyDampedLPSection(BMMultiLevelBiquad *This,
                                                     double fc,
                                                     size_t level,
                                                     size_t filterOrder){
    (void)filterOrder;
    BMMultiLevelBiquad_setSectionCoefs(This, level, BMMultiLevelBiquad_designCriticallyDampedLPSection(fc, This->sampleRate));
}






/*!
 * BMMultiLevelBiquad_setCriticallyDampedLP
 *
 * @abstract sets up a Critically-damped Lowpass filter of order 2*numLevels
 *
 * @param fc           cutoff frequency
 * @param firstLevel   the filter will consume numLevels contiguous
 *                     biquad sections, beginning with firstLevel
 * @param numLevels    number of biquad sections to use. numLevels = (filterOrder / 2)
 */
void BMMultiLevelBiquad_setCriticallyDampedLP(BMMultiLevelBiquad *This, double fc, size_t firstLevel, size_t numLevels){
    
    // N is the filter order
    size_t N = numLevels * 2;
    
    for(size_t i=0; i<numLevels; i++)
        BMMultiLevelBiquad_setCriticallyDampedLPSection(This, fc, i+firstLevel, N);
}




#pragma mark - Coefficient design (shared with BMMultiLevelSVF)

/*
 * The BMMultiLevelBiquad_design* functions hold the coefficient formulae of
 * the setters above, as pure functions of the setter's arguments and the
 * sample rate. Each setter is that function plus
 * BMMultiLevelBiquad_setSectionCoefs; BMMultiLevelSVF's ...AsBiquad setters
 * call the same functions and convert the result with
 * BMMultiLevelSVF_fromBiquadCoefs, so both filters compute one design from
 * one piece of code. The arithmetic (including the float intermediates of
 * the older setters) is unchanged from the setters it was moved out of.
 */

static inline BMBiquadSectionCoefs BMBiquad_coefs(double b0, double b1, double b2, double a1, double a2){
    BMBiquadSectionCoefs c = {b0, b1, b2, a1, a2};
    return c;
}

static void BMMultiLevelBiquad_setSectionCoefs(BMMultiLevelBiquad *This, size_t level, BMBiquadSectionCoefs c){
    assert(level < This->numLevels);
    for(size_t i=0; i < This->numChannels; i++){
        double* b0 = This->coefficients_d + level*This->numChannels*5 + i*5;
        b0[0] = c.b0; b0[1] = c.b1; b0[2] = c.b2; b0[3] = c.a1; b0[4] = c.a2;
    }
    BMMultiLevelBiquad_queueUpdate(This);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designBypass(void){
    return BMBiquad_coefs(1.0, 0.0, 0.0, 0.0, 0.0);
}

// based on formula in 2.3.10 of Digital Filters for Everyone by Rusty Allred
BMBiquadSectionCoefs BMMultiLevelBiquad_designHighShelf(float fc, float gain_db, double sampleRate){
    float gainV = BM_DB_TO_GAIN(gain_db);
    
    double gamma = tanf(M_PI * fc / sampleRate);
    double gamma_2 = gamma*gamma;
    double sqrt_gain = sqrtf(gainV);
    double g_d;
    
    // conditionally set G
    double G;
    if (gainV > 2.0){
        G = gainV * M_SQRT2 * 0.5;
        double G_2 = G*G;
        g_d = pow((G_2 - 1.0)/(gainV*gainV - G_2), 0.25);
    }
    else {
        if (gainV >= 0.5) {
            G = sqrt_gain;
            g_d = pow(1/gainV,0.25);
        }
        else{
            G = gainV * M_SQRT2;
            double G_2 = G*G;
            g_d = pow((G_2 - 1.0)/(gainV*gainV - G_2), 0.25);
        }
    }
    (void)G;
    
    // compute reuseable variables
    double g_d_2 = g_d*g_d;
    double g_n = g_d * sqrt_gain;
    double g_n_2 = g_n * g_n;
    double sqrt_2_g_d_gamma = M_SQRT2 * g_d * gamma;
    double sqrt_2_g_n_gamma = M_SQRT2 * g_n * gamma;
    double gamma_2_plus_g_d_2 = gamma_2 + g_d_2;
    double gamma_2_plus_g_n_2 = gamma_2 + g_n_2;
    
    double one_over_denominator = 1.0f / (gamma_2_plus_g_d_2 + sqrt_2_g_d_gamma);
    
    return BMBiquad_coefs((gamma_2_plus_g_n_2 + sqrt_2_g_n_gamma) * one_over_denominator,
                          2.0f * (gamma_2 - g_n_2) * one_over_denominator,
                          (gamma_2_plus_g_n_2 - sqrt_2_g_n_gamma) * one_over_denominator,
                          2.0f * (gamma_2 - g_d_2) * one_over_denominator,
                          (gamma_2_plus_g_d_2 - sqrt_2_g_d_gamma)*one_over_denominator);
}

// based on formula in 2.3.10 of Digital Filters for Everyone by Rusty Allred
BMBiquadSectionCoefs BMMultiLevelBiquad_designLowShelf(float fc, float gain_db, double sampleRate){
    float gainV = BM_DB_TO_GAIN(gain_db);
    
    double gamma = tanf(M_PI * fc / sampleRate);
    double gamma_2 = gamma*gamma;
    double sqrt_gain = sqrtf(gainV);
    double g_d;
    
    // conditionally set G
    double G;
    if (gainV > 2.0){
        G = gainV * M_SQRT2 * 0.5;
        double G_2 = G*G;
        g_d = pow((G_2 - 1.0)/(gainV*gainV - G_2), 0.25);
    }
    else {
        if (gainV >= 0.5) {
            G = sqrt_gain;
            g_d = pow(1/gainV,0.25);
        }
        else{
            G = gainV * M_SQRT2;
            double G_2 = G*G;
            g_d = pow((G_2 - 1.0)/(gainV*gainV - G_2), 0.25);
        }
    }
    (void)G;
    
    // compute reuseable variables
    double g_d_2 = g_d*g_d;
    double g_n = g_d * sqrt_gain;
    double g_n_2 = g_n * g_n;
    double g_n_2_gamma_2 = g_n_2 * gamma_2;
    double g_d_2_gamma_2 = g_d_2 * gamma_2;
    double sqrt_2_g_d_gamma = M_SQRT2 * g_d * gamma;
    double sqrt_2_g_n_gamma = M_SQRT2 * g_n * gamma;
    double g_d_2_gamma_2_plus_1 = g_d_2_gamma_2 + 1.0;
    double g_n_2_gamma_2_plus_1 = g_n_2_gamma_2 + 1.0;
    
    double one_over_denominator = 1.0 / (g_d_2_gamma_2_plus_1 + sqrt_2_g_d_gamma);
    
    return BMBiquad_coefs((g_n_2_gamma_2_plus_1 + sqrt_2_g_n_gamma) * one_over_denominator,
                          2.0 * (g_n_2_gamma_2 - 1.0) * one_over_denominator,
                          (g_n_2_gamma_2_plus_1 - sqrt_2_g_n_gamma) * one_over_denominator,
                          2.0 * (g_d_2_gamma_2 - 1.0) * one_over_denominator,
                          (g_d_2_gamma_2_plus_1 - sqrt_2_g_d_gamma)*one_over_denominator);
}

// Robert Bristow-Johnson cookbook shelves with the slope parameter
static BMBiquadSectionCoefs BMMultiLevelBiquad_designShelfRBJ(float fc, float gain_db, float slope, double sampleRate, bool highShelf){
    assert(0.3 <= slope && slope <= 1.0);
    
    double A = pow(10.0,gain_db/40.0);
    double w0 = 2.0 * M_PI * (fc / sampleRate);
    double alpha = sin(w0)/2.0 * sqrt( (A + 1.0/A) * (1.0/slope - 1.0) + 2.0);
    double twoSqrtAalpha = 2.0 * sqrt(A) * alpha;
    
    double a0, a1, a2, b0, b1, b2;
    if (highShelf){
        b0 =      A*( (A+1.0) + (A-1.0)*cos(w0) + twoSqrtAalpha );
        b1 = -2.0*A*( (A-1.0) + (A+1.0)*cos(w0)                 );
        b2 =      A*( (A+1.0) + (A-1.0)*cos(w0) - twoSqrtAalpha );
        a0 =          (A+1.0) - (A-1.0)*cos(w0) + twoSqrtAalpha;
        a1 =    2.0*( (A-1.0) - (A+1.0)*cos(w0)                 );
        a2 =          (A+1.0) - (A-1.0)*cos(w0) - twoSqrtAalpha;
    } else {
        b0 =      A*( (A+1.0) - (A-1.0)*cos(w0) + twoSqrtAalpha );
        b1 =  2.0*A*( (A-1.0) - (A+1.0)*cos(w0)                 );
        b2 =      A*( (A+1.0) - (A-1.0)*cos(w0) - twoSqrtAalpha );
        a0 =          (A+1.0) + (A-1.0)*cos(w0) + twoSqrtAalpha;
        a1 =   -2.0*( (A-1.0) + (A+1.0)*cos(w0)                 );
        a2 =          (A+1.0) + (A-1.0)*cos(w0) - twoSqrtAalpha;
    }
    
    // normalize a0 to 1
    return BMBiquad_coefs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designHighShelfAdjustableSlope(float fc, float gain_db, float slope, double sampleRate){
    return BMMultiLevelBiquad_designShelfRBJ(fc, gain_db, slope, sampleRate, true);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designLowShelfAdjustableSlope(float fc, float gain_db, float slope, double sampleRate){
    return BMMultiLevelBiquad_designShelfRBJ(fc, gain_db, slope, sampleRate, false);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designHighShelfFirstOrder(float fc, float gain_db, double sampleRate){
    float gainV = BM_DB_TO_GAIN(gain_db);
    double gamma = tanf(M_PI * fc / sampleRate);
    double one_over_denominator;
    double b0, b1, a1;
    if(gainV>1.0f){
        one_over_denominator = 1.0f / (gamma + 1.0f);
        b0 = (gamma + gainV) * one_over_denominator;
        b1 = (gamma - gainV) * one_over_denominator;
        a1 = (gamma - 1.0f) * one_over_denominator;
    }else{
        one_over_denominator = 1.0f / (gamma*gainV + 1.0f);
        b0 = gainV*(gamma + 1.0f) * one_over_denominator;
        b1 = gainV*(gamma - 1.0f) * one_over_denominator;
        a1 = (gainV*gamma - 1.0f) * one_over_denominator;
    }
    return BMBiquad_coefs(b0, b1, 0.0f, a1, 0.0f);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designLowShelfFirstOrder(float fc, float gain_db, double sampleRate){
    float gainV = BM_DB_TO_GAIN(gain_db);
    double gamma = tanf(M_PI * fc / sampleRate);
    double one_over_denominator;
    double b0, b1, a1;
    if(gainV>1.0f){
        one_over_denominator = 1.0f / (gamma + 1.0f);
        b0 = (gamma * gainV + 1.0f) * one_over_denominator;
        b1 = (gamma * gainV - 1.0f) * one_over_denominator;
        a1 = (gamma - 1.0f) * one_over_denominator;
    }else{
        one_over_denominator = 1.0f / (gamma + gainV);
        b0 = gainV*(gamma + 1.0f) * one_over_denominator;
        b1 = gainV*(gamma - 1.0f) * one_over_denominator;
        a1 = (gamma - gainV) * one_over_denominator;
    }
    return BMBiquad_coefs(b0, b1, 0.0f, a1, 0.0f);
}

// based on formulae in 2.3.8 in Digital Filters are for Everyone,
// 2nd ed. by Rusty Allred
BMBiquadSectionCoefs BMMultiLevelBiquad_designBell(float fc, float bandwidth, float gain_db, double sampleRate){
    float gainV = BM_DB_TO_GAIN(gain_db);
    
    // if gain is close to 1.0, bypass the filter
    if (fabsf(gain_db) < 0.01)
        return BMMultiLevelBiquad_designBypass();
    
    double alpha =  tan( (M_PI * bandwidth)   / sampleRate);
    double beta  = -cos( (2.0 * M_PI * fc) / sampleRate);
    double oneOverD;
    
    if (gainV < 1.0) {
        oneOverD = 1.0 / (alpha + gainV);
        return BMBiquad_coefs((gainV + alpha*gainV) * oneOverD,
                              2.0 * beta * gainV * oneOverD,
                              (gainV - alpha*gainV) * oneOverD,
                              2.0 * beta * gainV * oneOverD,
                              (gainV - alpha) * oneOverD);
    }
    // gain >= 1
    oneOverD = 1.0 / (alpha + 1.0);
    return BMBiquad_coefs((1.0 + alpha*gainV) * oneOverD,
                          2.0 * beta * oneOverD,
                          (1.0 - alpha*gainV) * oneOverD,
                          2.0 * beta * oneOverD,
                          (1.0 - alpha) * oneOverD);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designBellQ(float fc, float Q, float gain_db, double sampleRate){
    return BMMultiLevelBiquad_designBell(fc, BMMultiLevelBiquad_QToBWAtSampleRate(Q, fc, (float)sampleRate), gain_db, sampleRate);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designBellWithSkirt(float fc, float Q, float bellGainDb, float skirtGainDb, double sampleRate){
    float bellGainV = BM_DB_TO_GAIN(bellGainDb);
    float skirtGainV = BM_DB_TO_GAIN(skirtGainDb);
    float bellFilterGainV = bellGainV / skirtGainV;
    
    double bandwidth = BMMultiLevelBiquad_QToBWAtSampleRate(Q, fc, (float)sampleRate);
    double alpha =  tan( (M_PI * bandwidth)   / sampleRate);
    double beta  = -cos( (2.0 * M_PI * fc) / sampleRate);
    double oneOverD;
    
    double b0b,b1b,b2b,a1b,a2b;
    
    // set up bell filter coefficients to make the bell affect the
    // difference (in dB) between the bell and skirt
    if (bellFilterGainV < 1.0) {
        oneOverD = 1.0 / (alpha + bellFilterGainV);
        b0b = (bellFilterGainV + alpha*bellFilterGainV) * oneOverD;
        b1b = 2.0 * beta * bellFilterGainV * oneOverD;
        b2b = (bellFilterGainV - alpha*bellFilterGainV) * oneOverD;
        a1b = 2.0 * beta * bellFilterGainV * oneOverD;
        a2b = (bellFilterGainV - alpha) * oneOverD;
    } else { // gain >= 1
        oneOverD = 1.0 / (alpha + 1.0);
        b0b = (1.0 + alpha*bellFilterGainV) * oneOverD;
        b1b = 2.0 * beta * oneOverD;
        b2b = (1.0 - alpha*bellFilterGainV) * oneOverD;
        a1b = 2.0 * beta * oneOverD;
        a2b = (1.0 - alpha) * oneOverD;
    }
    
    // scale the entire filter so that the skirt matches the skirt gain
    // (float, as the setter always did)
    float b0bs = b0b*skirtGainV;
    float b1bs = b1b*skirtGainV;
    float b2bs = b2b*skirtGainV;
    
    return BMBiquad_coefs(b0bs, b1bs, b2bs, a1b, a2b);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designLowPass12db(double fc, double sampleRate){
    double gamma = tan(M_PI * fc / sampleRate);
    double gamma_sq = gamma * gamma;
    double sqrt_2_gamma = gamma * M_SQRT2;
    double one_over_denominator = 1.0 / (gamma_sq + sqrt_2_gamma + 1.0);
    double b0 = gamma_sq * one_over_denominator;
    return BMBiquad_coefs(b0, 2.0 * b0, b0,
                          2.0 * (gamma_sq - 1.0) * one_over_denominator,
                          (gamma_sq - sqrt_2_gamma + 1.0) * one_over_denominator);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designLowPassQ12db(double fc, double q, double sampleRate){
    double gamma = tan(M_PI * fc / sampleRate);
    double gamma_sq = gamma * gamma;
    double one_over_denominator = 1.0 / (q*gamma_sq + gamma + q);
    double b0 = q * gamma_sq * one_over_denominator;
    return BMBiquad_coefs(b0, 2.0 * b0, b0,
                          2.0 * q * (gamma_sq - 1.0) * one_over_denominator,
                          (q*gamma_sq - gamma + q) * one_over_denominator);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designHighPass12db(double fc, double sampleRate){
    double gamma = tan(M_PI * fc / sampleRate);
    double gamma_sq = gamma * gamma;
    double sqrt_2_gamma = gamma * M_SQRT2;
    double one_over_denominator = 1.0 / (gamma_sq + sqrt_2_gamma + 1.0);
    double b0 = 1.0 * one_over_denominator;
    return BMBiquad_coefs(b0, -2.0 * one_over_denominator, b0,
                          2.0 * (gamma_sq - 1.0) * one_over_denominator,
                          (gamma_sq - sqrt_2_gamma + 1.0) * one_over_denominator);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designHighPass12dbNeg(double fc, double sampleRate){
    double gamma = tan(M_PI * fc / sampleRate);
    double gamma_sq = gamma * gamma;
    double sqrt_2_gamma = gamma * M_SQRT2;
    double one_over_denominator = 1.0 / (gamma_sq + sqrt_2_gamma + 1.0);
    double b0 = -1.0 * one_over_denominator;
    return BMBiquad_coefs(b0, 2.0 * one_over_denominator, b0,
                          2.0 * (gamma_sq - 1.0) * one_over_denominator,
                          (gamma_sq - sqrt_2_gamma + 1.0) * one_over_denominator);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designHighPassQ12db(double fc, double q, double sampleRate){
    double gamma = tan(M_PI * fc / sampleRate);
    double gamma_sq = gamma * gamma;
    double one_over_denominator = 1.0 / (q*gamma_sq + gamma + q);
    double b0 = q * one_over_denominator;
    return BMBiquad_coefs(b0, -2.0 * b0, b0,
                          2.0 * q * (gamma_sq - 1.0) * one_over_denominator,
                          (q*gamma_sq - gamma + q) * one_over_denominator);
}

// Digital Filters for Everyone, 2nd. ed. by Rusty Allred (tested in Mathematica)
BMBiquadSectionCoefs BMMultiLevelBiquad_designLinkwitzRileyLP(double fc, double sampleRate){
    double gamma = tan(M_PI * fc / sampleRate);
    double gamma_sq = gamma * gamma;
    double two_gamma = gamma * 2.0;
    double one_over_denominator = 1.0 / (gamma_sq + two_gamma + 1.0);
    double b0 = gamma_sq * one_over_denominator;
    return BMBiquad_coefs(b0, 2.0 * b0, b0,
                          2.0 * (gamma_sq - 1.0) * one_over_denominator,
                          (gamma_sq - two_gamma + 1.0) * one_over_denominator);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designLinkwitzRileyHP(double fc, double sampleRate){
    double gamma = tan(M_PI * fc / sampleRate);
    double gamma_sq = gamma * gamma;
    double two_gamma = gamma * 2.0;
    double one_over_denominator = 1.0 / (gamma_sq + two_gamma + 1.0);
    return BMBiquad_coefs(-1.0 * one_over_denominator, 2.0 * one_over_denominator, -1.0 * one_over_denominator,
                          2.0 * (gamma_sq - 1.0) * one_over_denominator,
                          (gamma_sq - two_gamma + 1.0) * one_over_denominator);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designLowPass6db(double fc, double sampleRate){
    double gamma = tan(M_PI * fc / sampleRate);
    double one_over_denominator = 1.0 / (gamma + 1.0);
    double b0 = gamma * one_over_denominator;
    return BMBiquad_coefs(b0, b0, 0.0, (gamma - 1.0) * one_over_denominator, 0.0);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designHighPass6db(double fc, double sampleRate){
    double gamma = tan(M_PI * fc / sampleRate);
    double one_over_denominator = 1.0 / (gamma + 1.0);
    return BMBiquad_coefs(1.0 * one_over_denominator, -1.0 * one_over_denominator, 0.0,
                          (gamma - 1.0) * one_over_denominator, 0.0);
}

// two first order filters packed into one section
BMBiquadSectionCoefs BMMultiLevelBiquad_designHighPassLowPass(double highPassFc, double lowPassFc, double sampleRate){
    double gamma = tan(M_PI * highPassFc / sampleRate);
    double one_over_denominator = 1.0 / (gamma + 1.0);
    double b0h = 1.0 * one_over_denominator;
    double b1h = -b0h;
    double a1h = (gamma - 1.0) * one_over_denominator;
    
    gamma = tan(M_PI * lowPassFc / sampleRate);
    one_over_denominator = 1.0 / (gamma + 1.0);
    double b0l = gamma * one_over_denominator;
    double b1l = b0l;
    double a1l = (gamma - 1.0) * one_over_denominator;
    
    double b0, b1, b2, a1, a2;
    packFirstOrder(b0h, b1h, a1h, b0l, b1l, a1l, &b0, &b1, &b2, &a1, &a2);
    return BMBiquad_coefs(b0, b1, b2, a1, a2);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designAllpass2ndOrder(double c1, double c2){
    //        1 + (c1 + c2) z + c1 c2 z^2
    // H(z) = ---------------------------
    //         c1 c2 + (c1 + c2) z + z^2
    return BMBiquad_coefs(c1 * c2, c1 + c2, 1.0, c1 + c2, c1 * c2);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designAllpass1stOrder(double c){
    //         c + z^-1
    // H(z) = ----------
    //        1 + c z^-1
    return BMBiquad_coefs(c, 1.0f, 0.0, c, 0.0);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designCriticallyDampedPhaseCompensator(double lowpassFC, double sampleRate){
    // The allpass coefficient Beta that yields the same phase response as the
    // critically damped lowpass at lowpassFC (Mathematica, see the setter).
    double c1 = 2.0 * M_2_PI; // 4 / pi
    double c2 = -1.0 + M_SQRT2 + sqrt(10.0 - 7.0*M_SQRT2);
    double c3 = 1.0 - c2;
    double wc = 2.0 * M_PI * lowpassFC / sampleRate;
    double allpassBeta = -2.0 +
                         (c1 * wc) +
                         (c2 * (cos(wc/2.0) - sin(wc/2.0))) +
                         (c3 * cos(wc));
    return BMMultiLevelBiquad_designAllpass1stOrder(allpassBeta);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designBWLPSection(double fc, size_t filterOrder, size_t sectionNumber, double sampleRate){
    double c = butterworthCHelper(filterOrder, sectionNumber);
    fc = digitalToAnalogFCWarp(M_PI*fc/(sampleRate/2.0));
    double fc2 = fc*fc;
    
    // BTWzDenominator[z_, N_, n_, fc_] :=
    //      (4 + 2 BTWC[N, n] fc + fc*fc) +
    //      (-8 + 2 fc*fc)*z^(-1) +
    //      (4 - 2 BTWC[N, n] fc  + fc*fc)*z^-2
    double a0 = 4.0 + 2.0 * c * fc + fc2;
    double a1 = (-8.0 + 2.0 * fc2);
    double a2 = (4.0 - 2.0 * c * fc + fc2);
    
    // BTWzNumerator[z_, fc_] := fc*fc*(1 + 2 z^(-1) + z^(-2))
    double b0 = fc2;
    double b1 = 2.0 * fc2;
    double b2 = fc2;
    
    // normalize the a0 term to 1.0
    return BMBiquad_coefs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designLegendreLPSection(double fc, size_t filterOrder, size_t sectionNumber, double sampleRate){
    // get the coefficients for the prototype filter in the analog domain
    float sDomainPrototypeCoefficients [6];
    BMMultiLevelBiquad_getAnalogLegendreLPSection(filterOrder,
                                                  sectionNumber,
                                                  sDomainPrototypeCoefficients);
    
    // compute the warped cutoff frequency of the analog prototype to
    // prepare for s to z domain transformation.
    double fcInRadians = M_PI * (fc / (sampleRate/2.0));
    double warpedAnalogFc = tan(0.5*fcInRadians);
    
    // warp the frequency of the s-domain prototype
    float sDomainCoefficientsWarped [6];
    BMMuiltiLevelBiquad_freqWarpSDomain(warpedAnalogFc,
                                        sDomainPrototypeCoefficients,
                                        sDomainCoefficientsWarped);
    
    // convert from s-domain to z-domain using the bilinear transform
    float zDomainCoefficients [6];
    BMMuiltiLevelBiquad_coefficientsStoZ(sDomainCoefficientsWarped,
                                         zDomainCoefficients);
    
    // normalize the A0 coefficient to 1
    BMMuiltiLevelBiquad_NormalizeA0(zDomainCoefficients,zDomainCoefficients);
    
    // {a0, a1, a2, b0, b1, b2}
    return BMBiquad_coefs(zDomainCoefficients[3], zDomainCoefficients[4], zDomainCoefficients[5],
                          zDomainCoefficients[1], zDomainCoefficients[2]);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designBesselLPSection(double fc, size_t filterOrder, size_t sectionNumber, double sampleRate){
    float fcMultiplier, q;
    besselFilterSectionGetQAndFcMultiplier(filterOrder,sectionNumber,&fcMultiplier,&q);
    return BMMultiLevelBiquad_designLowPassQ12db(fc*fcMultiplier, q, sampleRate);
}

BMBiquadSectionCoefs BMMultiLevelBiquad_designCriticallyDampedLPSection(double fc, double sampleRate){
    // critically damped second order filter has Q of 0.5
    return BMMultiLevelBiquad_designLowPassQ12db(fc, 0.5, sampleRate);
}


#pragma mark - Per-section formulae (shared with BMMultiLevelSVF)

/*
 * The BMBiquadSection_* functions evaluate one direct form section
 *
 *     H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
 *
 * (the layout of coefficients_d, a0 normalised to 1). The whole-cascade
 * functions below sum or multiply them over the levels. BMMultiLevelSVF
 * converts its levels to biquad coefficients and calls the same functions, so
 * the two filters plot identically for the same transfer function.
 */

// frequency response at the complex point z (see DSPDoubleComplex_z, which
// uses z = exp(-i w))
DSPDoubleComplex BMBiquadSection_tfEval(double b0, double b1, double b2, double a1, double a2, DSPDoubleComplex z){
    DSPDoubleComplex z2 = DSPDoubleComplex_cmul(z, z);
    
    DSPDoubleComplex numerator =
    DSPDoubleComplex_add3(DSPDoubleComplex_smul(b0, z2),
                          DSPDoubleComplex_smul(b1, z),
                          DSPDoubleComplex_init(b2, 0.0));
    
    DSPDoubleComplex denominator =
    DSPDoubleComplex_add3(z2,
                          DSPDoubleComplex_smul(a1, z),
                          DSPDoubleComplex_init(a2, 0.0));
    
    return DSPDoubleComplex_divide(numerator, denominator);
}



// group delay in samples at the radian frequency w = 2 pi f / fs
double BMBiquadSection_groupDelay(double b0, double b1, double b2, double a1, double a2, double w){
    // normalize the feed forward coefficients so that b0=1
    // see: see: http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
    b1 /= b0;
    b2 /= b0;
    b0 = 1.0;
    
    // calculate the group delay of the normalized filter using a cookbook formula
    // http://music-dsp.music.columbia.narkive.com/9F6BIvHy/group-delay
    // or
    // http://music.columbia.edu/pipermail/music-dsp/1998-April/053307.html
    //
    //    T(w) =
    //
    //      b1^2 + 2*b2^2 + b1*(1 + 3*b2)*cos(w) + 2*b2*cos(2*w)
    //    --------------------------------------------------------
    //     1 + b1^2 + b2^2 + 2*b1*(1 + b2)*cos(w) + 2*b2*cos(2*w)
    //
    //
    //        a1^2 + 2*a2^2 + a1*(1 + 3*a2)*cos(w) + 2*a2*cos(2*w)
    //    - --------------------------------------------------------
    //        1 + a1^2 + a2^2 + 2*a1*(1 + a2)*cos(w) + 2*a2*cos(2*w)
    //
    //
    //    w is normalized radian frequency and T(w) is measured in sample units.
    
    
    //      b1^2 + 2*b2^2 + b1*(1 + 3*b2)*cos(w) + 2*b2*cos(2*w)
    double num1 = b1*b1 + 2.0*b2*b2 + b1*(1.0 + 3.0*b2)*cos(w) + 2.0*b2*cos(2.0*w);
    //     1 + b1^2 + b2^2 + 2*b1*(1 + b2)*cos(w) + 2*b2*cos(2*w)
    double den1 = 1.0 + b1*b1 + b2*b2 + 2.0*b1*(1.0 + b2)*cos(w) + 2.0*b2*cos(2.0*w);
    double frac1 = num1/den1;
    
    
    //        a1^2 + 2*a2^2 + a1*(1 + 3*a2)*cos(w) + 2*a2*cos(2*w)
    double num2 = a1*a1 + 2.0*a2*a2 + a1*(1.0 + 3.0*a2)*cos(w) + 2.0*a2*cos(2.0*w);
    //        1 + a1^2 + a2^2 + 2*a1*(1 + a2)*cos(w) + 2*a2*cos(2*w)
    double den2 = 1.0 + a1*a1 + a2*a2 + 2.0*a1*(1.0 + a2)*cos(w) + 2.0*a2*cos(2.0*w);
    double frac2 = num2/den2;
    
    return frac1 - frac2;
}



// unwrapped phase in radians at the radian frequency w = 2 pi f / fs, in the
// sign convention of this library (z = exp(-i w), so this is minus the
// textbook arg H(exp(i w)): a lowpass has a positive phase, a lag)
double BMBiquadSection_phaseResponse(double b0, double b1, double b2, double a1, double a2, double w){
    // Mathematica prototype:
    //
    // biquadPR[w_, b0_, b1_, b2_, a0_, a1_, a2_] :=
    // -ArcTan[(b0 Sin[0 w] + b1 Sin[1 w] + b2 Sin[2 w]),
    //         (b0 Cos[0 w] + b1 Cos[1 w] + b2 Cos[2 w])] +
    //  ArcTan[-(a0 Sin[0 w] + a1 Sin[1 w] + a2 Sin[2 w]),
    //         -(a0 Cos[0 w] + a1 Cos[1 w] + a2 Cos[2 w])]
    //
    // based on equation (24) in
    // http://www.rs-met.com/documents/dsp/BasicDigitalFilters.pdf
    double w2 = w * 2.0;
    double sinw = sin(w);
    double cosw = cos(w);
    double sin2w = sin(w2);
    double cos2w = cos(w2);
    //
    double sb =       (b1 * sinw) + (b2 * sin2w);
    double cb = b0  + (b1 * cosw) + (b2 * cos2w);
    double sa =       (a1 * sinw) + (a2 * sin2w);
    double ca = 1.0 + (a1 * cosw) + (a2 * cos2w);
    
    return atan2(sb, cb) - atan2(sa, ca);
}



// wrap a phase in radians into (-pi, pi]
double BMBiquadSection_wrapPhase(double phase){
    double wrapped = phase - 2.0 * M_PI * floor((phase + M_PI) / (2.0 * M_PI));
    // floor puts the result in [-pi, pi); move -pi to +pi
    if(wrapped <= -M_PI) wrapped += 2.0 * M_PI;
    return wrapped;
}




#pragma mark - Transfer function, group delay and phase of the cascade

// evaluate the transfer function of the filter at all levels for the
// frequency specified by the complex number z
inline DSPDoubleComplex BMMultiLevelBiquad_tfEval(BMMultiLevelBiquad *This, DSPDoubleComplex z){
    DSPDoubleComplex out = DSPDoubleComplex_init(BMSmoothGain_getGainLinear(&This->gain), 0.0);
    
    for (size_t level = 0; level < This->numLevels; level++) {
        // both channels are the same so we just check the left one
        const double* c = This->coefficients_d + level*This->numChannels*5;
        out = DSPDoubleComplex_cmul(out, BMBiquadSection_tfEval(c[0], c[1], c[2], c[3], c[4], z));
    }
    
    return out;
}


inline DSPDoubleComplex BMMultiLevelBiquad_tfEvalAtLevel(BMMultiLevelBiquad *This, DSPDoubleComplex z,size_t level){
    DSPDoubleComplex out = DSPDoubleComplex_init(BMSmoothGain_getGainLinear(&This->gain), 0.0);
    
    // both channels are the same so we just check the left one
    const double* c = This->coefficients_d + level*This->numChannels*5;
    return DSPDoubleComplex_cmul(out, BMBiquadSection_tfEval(c[0], c[1], c[2], c[3], c[4], z));
}








/*!
 * BMMultiLevelBiquad_tfMagVector
 *
 * @param frequency   an an array specifying frequencies at which we want to evaluate
 * the transfer function magnitude of the filter
 *
 * @param magnitude   an array for storing the result
 * @param length      the number of elements in frequency and magnitude
 *
 */
void BMMultiLevelBiquad_tfMagVector(BMMultiLevelBiquad *This, const float *frequency, float *magnitude, size_t length){
    for (size_t i = 0; i<length; i++) {
        // convert from frequency into z (complex angular velocity)
        DSPDoubleComplex z = DSPDoubleComplex_z(frequency[i], This->sampleRate);
        // evaluate the transfer function at z and take absolute value
        magnitude[i] = DSPDoubleComplex_abs(BMMultiLevelBiquad_tfEval(This,z));
    }
}

void BMMultiLevelBiquad_tfMagVectorAtLevel(BMMultiLevelBiquad *This, const float *frequency, float *magnitude, size_t length,size_t level){
    for (size_t i = 0; i<length; i++) {
        // convert from frequency into z (complex angular velocity)
        DSPDoubleComplex z = DSPDoubleComplex_z(frequency[i], This->sampleRate);
        // evaluate the transfer function at z and take absolute value
        magnitude[i] = DSPDoubleComplex_abs(BMMultiLevelBiquad_tfEvalAtLevel(This,z,level));
    }
}





/*!
 * BMMultiLevelBiquad_groupDelay
 *
 * returns the total group delay (in samples) of all levels of the filter at the specified frequency.
 *
 * @discussion uses a cookbook formula for group delay of biquad filters, based on the fft derivative method.
 *
 * @param freq the frequency at which you need to compute the group delay of the filter cascade
 * @return the group delay in samples at freq
 */
double BMMultiLevelBiquad_groupDelay(BMMultiLevelBiquad *This, double freq){
    // radian normalised frequency: w = 2 pi f / fs. (Until 2026-09-12 the
    // factor pi was missing, so this function returned the group delay at
    // f / pi. Callers that had tuned themselves to the old numbers:
    // BMSaturator2's gain stage compensation, scaled to compensate.)
    double w = M_PI * freq / (0.5*This->sampleRate);
    
    double delay = 0.0;
    for (size_t level=0; level<This->numLevels; level++) {
        // both channels hold the same coefficients; read the left one. (Until
        // 2026-09-12 this used the stride 5*level, which for a stereo filter
        // read level 0 again instead of level 1.)
        const double* c = This->coefficients_d + level*This->numChannels*5;
        delay += BMBiquadSection_groupDelay(c[0], c[1], c[2], c[3], c[4], w);
    }
    
    return delay;
}

/*!
 * BMMiltiLevelBiquad_phaseResponse
 *
 * @abstract Calculates the real-valued phase response of the filter cascade at the frequency freq. The formula for this generalizes to filters of any order. It is based on equation (24) in this document: http://www.rs-met.com/documents/dsp/BasicDigitalFilters.pdf
 * @param This       pointer to an initialized struct
 * @param freq      the frequency at which we want to evaluate the phase response
 */
double BMMiltiLevelBiquad_phaseResponse(BMMultiLevelBiquad *This, double freq){
    double totalPhaseShift = 0;
    double w = 2.0 * M_PI * freq / This->sampleRate;
    
    for (size_t level=0; level<This->numLevels; level++) {
        const double* c = This->coefficients_d + level*This->numChannels*5;
        totalPhaseShift += BMBiquadSection_phaseResponse(c[0], c[1], c[2], c[3], c[4], w);
    }
    
    // wrap the result into (-pi, pi] and return. (Until 2026-09-12 this
    // called modf, which returned the fractional part of the phase in
    // radians, not a wrapped angle.)
    return BMBiquadSection_wrapPhase(totalPhaseShift);
}


//#ifdef __cplusplus
//}
//#endif

