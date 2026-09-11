//
//  BMIIRDownsampler2x.c
//  AudioFiltersXcodeProject
//
//  Based on Downsampler2xFpu.hpp from Laurent de Soras HIIR library
//  http://ldesoras.free.fr/prod.html
//
//  Created by Hans on 9/7/19.
//  Anyone may use this file without restrictions of any kind
//

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "BMIIRDownsampler2x.h"
#include "BMPolyphaseIIR2Designer.h"
#include "BMInterleaver.h"
#include "Constants.h"

#define BM_DOWNSAMPLER_CHUNK_SIZE BM_BUFFER_CHUNK_SIZE * 8

// forward declaration of internal function
double* BMIIRDownsampler2x_genCoefficients(BMIIRDownsampler2x *This, float minStopbandAttenuationDb, float maxTransitionBandwidth);





size_t BMIIRDownsampler2x_init (BMIIRDownsampler2x *This,
                              float minStopbandAttenuationDb,
                              float maxTransitionBandwidth,
                              bool stereo){
    This->stereo = stereo;
    
    // generate the filter coefficients. These will be first order allpass filter
    // coefficients.
    double* coefficientArray = BMIIRDownsampler2x_genCoefficients(This,
                                                                minStopbandAttenuationDb,
                                                                maxTransitionBandwidth);
    
    // set up the filters
    float sampleRate = 48000.0f; // the filters will ignore this, but we have to set it to some dummy value.
    BMMultiLevelBiquad_init(&This->even, This->numBiquadStages, sampleRate, stereo, true, false);
    BMMultiLevelBiquad_init(&This->odd, This->numBiquadStages, sampleRate, stereo, true, false);
    BMIIRDownsampler2x_setCoefs(This, coefficientArray);
    
    // double precision is off until requested
    This->doublePrecision = false;
    This->evenD = This->oddD = NULL;
    This->db1L = This->db2L = This->db1R = This->db2R = NULL;
    
    free(coefficientArray);
    
    // allocate memory for buffers
    This->b1L = malloc(sizeof(float)*BM_DOWNSAMPLER_CHUNK_SIZE);
    This->b2L = malloc(sizeof(float)*BM_DOWNSAMPLER_CHUNK_SIZE);
    if(stereo){
        This->b1R = malloc(sizeof(float)*BM_DOWNSAMPLER_CHUNK_SIZE);
        This->b2R = malloc(sizeof(float)*BM_DOWNSAMPLER_CHUNK_SIZE);
    } else {
        This->b2R = NULL;
        This->b2R = NULL;
    }
    
    // return the number of coefficients used
    return This->numCoefficients;
}





/*!
 *BMIIRDownsampler2x_genCoefficients
 *
 * @abstract generates a list of first order allpass filter coefficients such that the filter cascade composed of all the even numbered coefficients generates signal A and the filter cascade composed of all the odd numbered coefficients generates signal B such that A and B are in quadrature phase across most of the frequency spectrum.
 * @param This                      pointer to a struct
 * @param minStopbandAttenuationDb     the AA filters will acheive at least this much stopband attenuation. specified in dB as a positive number.
 * @param maxTransitionBandwidth       the AA filters will not let the transition bandwidth exceed this value. In (0,0.5).
 */
double* BMIIRDownsampler2x_genCoefficients(BMIIRDownsampler2x *This, float minStopbandAttenuationDb, float maxTransitionBandwidth){
    // find out how many allpass filter stages it will take to acheive the
    // required stopband attenuation and transition bandwidth
    This->numCoefficients = BMPolyphaseIIR2Designer_computeNbrCoefsFromProto(minStopbandAttenuationDb, maxTransitionBandwidth);
    
    printf("BMDownsampler: numCoefficients before rounding: %zu\n",This->numCoefficients);
    
//    // if numCoefficients is not divisible by four, increase to the nearest multiple of four
//    if(This->numCoefficients % 4 != 0)
//        This->numCoefficients += (4 - This->numCoefficients%4);
    
    // if numCoefficients is not divisible by two, increase to the nearest multiple of two
    if(This->numCoefficients % 2 != 0)
        This->numCoefficients += 1;
    
    printf("Downsampler: numCoefficients after rounding: %zu\n",This->numCoefficients);
    
    // Half of the biquad stages are used for even numbered samples and the
    // rest for odd. The total number of biquad filters is half the number of (first order)
    // coefficients. Therefore the number of biquad stages in each array (even,odd)
    // is numCoefficients / 4
    This->numBiquadStages = This->numCoefficients / 4;
    // add an extra stage if numCoefficients/2 is odd
    if(This->numCoefficients/2 % 2 == 1)
        This->numBiquadStages++;
    
    // generate filter coefficients
    double* coefficientArray = malloc(sizeof(double)*This->numCoefficients);
    BMPolyphaseIIR2Designer_computeCoefsSpecOrderTbw(coefficientArray,
                                                     (int)This->numCoefficients,
                                                     maxTransitionBandwidth);
    
    return coefficientArray;
}






void BMIIRDownsampler2x_free (BMIIRDownsampler2x *This){
    BMIIRDownsampler2x_setDoublePrecision(This, false);
    
    BMMultiLevelBiquad_free(&This->even);
    BMMultiLevelBiquad_free(&This->odd);
    
    free(This->b1L);
    free(This->b2L);
    if(This->stereo){
        free(This->b1R);
        free(This->b2R);
    }
    
    This->b1L = NULL;
    This->b2L = NULL;
    This->b1R = NULL;
    This->b2R = NULL;
}




void BMIIRDownsampler2x_setCoefs (BMIIRDownsampler2x *This, const double* coef_arr){
    assert (coef_arr != 0);
    
    /*
     * In theory, the ordering of the filters is irrelevant. We simply need
     * to put all the allpass filters with even numbered coefficients into
     * one filter array and all the odd numbered filters in the other.
     * However, when we combine two first order filters into a single
     * second-order biquad section, if both coefficients are small, the
     * coefficients of the resulting biquad section that must be set equal
     * to the product of the two first order allpass coefficients will be
     * near zero and therefore lead to a significant increase in
     * quantisation noise. To prevent this, we note that the coefficients
     * in coef_arr are sorted from smallest to largest and therefore we
     * avoid producing excessively small numbers in the biquad filters by
     * combining filter coefficients from opposite ends of the array.
     *
     * For example, if coef_array has length 8 then the even biquad filters
     * will use coefficient pairs {0,6} and {2,4}, rather than {0,2} and {4,6}.
     * The resulting filter cascade has the same transfer function either
     * way but by ordering them thus we keep quantisation noise below the
     * noise floor.
     */
    size_t biquadSection = 0;
    size_t i;
    for (i = 0; i < (This->numCoefficients-1)/2; i+=2){
        BMMultilevelBiquad_setAllpass2ndOrder(&This->even,
                                              coef_arr[i],coef_arr[This->numCoefficients - (i+2)],
                                              biquadSection);
        BMMultilevelBiquad_setAllpass2ndOrder(&This->odd,
                                              coef_arr[i+1],coef_arr[This->numCoefficients - (i+1)],
                                              biquadSection);
        biquadSection++;
    }
    // if numCoefficients/2 is odd, pick up the last coefficient with a first order section
    if(i<This->numCoefficients/2){
        BMMultilevelBiquad_setAllpass1stOrder(&This->even, coef_arr[i], biquadSection);
        BMMultilevelBiquad_setAllpass1stOrder(&This->odd, coef_arr[i+1], biquadSection);
    }
}






void BMIIRDownsampler2x_setDoublePrecision (BMIIRDownsampler2x *This, bool doublePrecision){
    if(doublePrecision && !This->doublePrecision){
        // the double biquads take the same coefficients as the float ones
        This->evenD = vDSP_biquadm_CreateSetupD(This->even.coefficients_d, This->even.numLevels, This->even.numChannels);
        This->oddD  = vDSP_biquadm_CreateSetupD(This->odd.coefficients_d,  This->odd.numLevels,  This->odd.numChannels);
        This->db1L = malloc(sizeof(double)*BM_DOWNSAMPLER_CHUNK_SIZE);
        This->db2L = malloc(sizeof(double)*BM_DOWNSAMPLER_CHUNK_SIZE);
        if(This->stereo){
            This->db1R = malloc(sizeof(double)*BM_DOWNSAMPLER_CHUNK_SIZE);
            This->db2R = malloc(sizeof(double)*BM_DOWNSAMPLER_CHUNK_SIZE);
        }
    }
    if(!doublePrecision && This->doublePrecision){
        vDSP_biquadm_DestroySetupD(This->evenD);
        vDSP_biquadm_DestroySetupD(This->oddD);
        This->evenD = This->oddD = NULL;
        free(This->db1L); free(This->db2L); free(This->db1R); free(This->db2R);
        This->db1L = This->db2L = This->db1R = This->db2R = NULL;
    }
    This->doublePrecision = doublePrecision;
}



/*
 * Double-precision versions of the process functions below. Same structure:
 * de-interleave, filter the odd-indexed samples through the even-coefficient
 * cascade and the even-indexed samples through the odd-coefficient cascade,
 * average. Conversion to double happens in the de-interleave (vDSP_vspdp
 * with stride 2) and back to float in the final store.
 */
static void BMIIRDownsampler2x_processBufferMonoD (BMIIRDownsampler2x *This, const float* input, float* output, size_t numSamplesIn){
    double* even = This->db1L;
    double* odd = This->db2L;
    while(numSamplesIn > 0){
        size_t samplesProcessing = BM_MIN(BM_DOWNSAMPLER_CHUNK_SIZE*2, numSamplesIn);
        size_t half = samplesProcessing / 2;
        vDSP_vspdp(input, 2, even, 1, half);
        vDSP_vspdp(input + 1, 2, odd, 1, half);
        const double *oddIn [1] = {odd}; double *oddOut [1] = {odd};
        vDSP_biquadmD(This->evenD, oddIn, 1, oddOut, 1, half);
        const double *evenIn [1] = {even}; double *evenOut [1] = {even};
        vDSP_biquadmD(This->oddD, evenIn, 1, evenOut, 1, half);
        double halfGain = 0.5;
        vDSP_vasmD(even, 1, odd, 1, &halfGain, even, 1, half);
        vDSP_vdpsp(even, 1, output, 1, half);
        numSamplesIn -= samplesProcessing;
        input += samplesProcessing;
        output += half;
    }
}

static void BMIIRDownsampler2x_processBufferStereoD (BMIIRDownsampler2x *This, const float* inputL, const float* inputR, float* outputL, float* outputR, size_t numSamplesIn){
    double *evenL = This->db1L, *oddL = This->db2L, *evenR = This->db1R, *oddR = This->db2R;
    while(numSamplesIn > 0){
        size_t samplesProcessing = BM_MIN(BM_DOWNSAMPLER_CHUNK_SIZE*2, numSamplesIn);
        size_t half = samplesProcessing / 2;
        vDSP_vspdp(inputL, 2, evenL, 1, half);
        vDSP_vspdp(inputL + 1, 2, oddL, 1, half);
        vDSP_vspdp(inputR, 2, evenR, 1, half);
        vDSP_vspdp(inputR + 1, 2, oddR, 1, half);
        const double *oddIn [2] = {oddL, oddR}; double *oddOut [2] = {oddL, oddR};
        vDSP_biquadmD(This->evenD, oddIn, 1, oddOut, 1, half);
        const double *evenIn [2] = {evenL, evenR}; double *evenOut [2] = {evenL, evenR};
        vDSP_biquadmD(This->oddD, evenIn, 1, evenOut, 1, half);
        double halfGain = 0.5;
        vDSP_vasmD(evenL, 1, oddL, 1, &halfGain, evenL, 1, half);
        vDSP_vasmD(evenR, 1, oddR, 1, &halfGain, evenR, 1, half);
        vDSP_vdpsp(evenL, 1, outputL, 1, half);
        vDSP_vdpsp(evenR, 1, outputR, 1, half);
        numSamplesIn -= samplesProcessing;
        inputL += samplesProcessing; inputR += samplesProcessing;
        outputL += half; outputR += half;
    }
}



void BMIIRDownsampler2x_processBufferMono (BMIIRDownsampler2x *This, const float* input, float* output, size_t numSamplesIn){
    assert(!This->stereo);
    assert (output != input);
    if(This->doublePrecision){ BMIIRDownsampler2x_processBufferMonoD(This, input, output, numSamplesIn); return; }
    
    float* even = This->b1L;
    float* odd = This->b2L;
    
    // chunk processing
    while(numSamplesIn > 0){
        size_t samplesProcessing = BM_MIN(BM_DOWNSAMPLER_CHUNK_SIZE*2, numSamplesIn);
        
        // copy even input samples to even, odd to odd
        BMDeInterleave(input, even, odd, samplesProcessing);
        
        // filter the odd-indexed inputs through the even-indexed filters
        BMMultiLevelBiquad_processBufferMono(&This->even, odd, odd, samplesProcessing/2);
    
        // filter the even-indexed inputs through the odd-indexed filters
        BMMultiLevelBiquad_processBufferMono(&This->odd, even, even, samplesProcessing/2);
        
        // sum the even and odd outputs into the main output and divide by two
        float half = 0.5;
        vDSP_vasm(even, 1, odd, 1, &half, output, 1, samplesProcessing/2);
        
        // advance pointers
        numSamplesIn -= samplesProcessing;
        input += samplesProcessing;
        output += samplesProcessing / 2;
    }
}



void BMIIRDownsampler2x_processBufferStereo (BMIIRDownsampler2x *This, const float* inputL, const float* inputR, float* outputL, float* outputR, size_t numSamplesIn){
    assert(This->stereo);
    assert (outputL != inputL);
    assert (outputR != inputR);
    if(This->doublePrecision){ BMIIRDownsampler2x_processBufferStereoD(This, inputL, inputR, outputL, outputR, numSamplesIn); return; }
    
    float* evenL = This->b1L;
    float* oddL = This->b2L;
    float* evenR = This->b1R;
    float* oddR = This->b2R;
    
    // chunk processing
    while(numSamplesIn > 0){
        size_t samplesProcessing = BM_MIN(BM_DOWNSAMPLER_CHUNK_SIZE*2, numSamplesIn);
        
        // copy even input samples to even, odd to odd
        BMDeInterleave(inputL, evenL, oddL, samplesProcessing);
        BMDeInterleave(inputR, evenR, oddR, samplesProcessing);
        
        // filter the odd-indexed inputs through the even-indexed filters
        BMMultiLevelBiquad_processBufferStereo(&This->even, oddL, oddR, oddL, oddR, samplesProcessing/2);
        
        // filter the even-indexed inputs through the odd-indexed filters
        BMMultiLevelBiquad_processBufferStereo(&This->odd, evenL, evenR, evenL, evenR, samplesProcessing/2);
        
        // sum the even and odd outputs into the main output and divide by two
        float half = 0.5;
        vDSP_vasm(evenL, 1, oddL, 1, &half, outputL, 1, samplesProcessing/2);
        vDSP_vasm(evenR, 1, oddR, 1, &half, outputR, 1, samplesProcessing/2);
        
        // advance pointers
        numSamplesIn -= samplesProcessing;
        inputL += samplesProcessing;
        outputL += samplesProcessing / 2;
        inputR += samplesProcessing;
        outputR += samplesProcessing / 2;
    }
}
