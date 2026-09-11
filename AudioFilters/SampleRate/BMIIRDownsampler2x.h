//
//  BMIIRDownsampler2x.h
//  AudioFiltersXcodeProject
//
//  Based on Downsampler2xFpu.h from Laurent de Soras HIIR library
//  http://ldesoras.free.fr/prod.html
//
//  Created by Hans on 9/7/19.
//  Anyone may use this file without restrictions of any kind
//

#ifndef BMIIRDownsampler2x_h
#define BMIIRDownsampler2x_h

#include <stdio.h>
#include "BMMultiLevelBiquad.h"

typedef struct BMIIRDownsampler2x {
    BMMultiLevelBiquad even;
    BMMultiLevelBiquad odd;
    float *b1L, *b2L, *b1R, *b2R;
    size_t numCoefficients,numBiquadStages;
    bool stereo;
    // optional double-precision processing (see setDoublePrecision): vDSP
    // double biquads built from the same allpass coefficients, and double
    // work buffers
    bool doublePrecision;
    vDSP_biquadm_SetupD evenD, oddD;
    double *db1L, *db2L, *db1R, *db2R;
} BMIIRDownsampler2x;


/*!
 *BMIIRDownsampler2x_init
 *
 * @param This                         pointer to an uninitialised BMIIRDownsampler2x struct
 * @param minStopbandAttenuationDb     the AA filters will acheive at least this much stopband attenuation. specified in dB as a positive number.
 * @param maxTransitionBandwidth       the AA filters will not let the transition bandwidth exceed this value. In (0,0.5).
 * @param stereo                       Set this to true to process audio in stereo; false for mono
 */
size_t BMIIRDownsampler2x_init (BMIIRDownsampler2x *This,
                                float minStopbandAttenuationDb,
                                float maxTransitionBandwidth,
                                bool stereo);

void BMIIRDownsampler2x_free (BMIIRDownsampler2x *This);

/*!
 *BMIIRDownsampler2x_setDoublePrecision
 *
 * @abstract Process this stage in double precision. The polyphase allpass
 * sections have poles close to the unit circle, and in float32 their
 * rounding noise is correlated with the signal, producing image tones at
 * fs/4 +- f and fs/2 - f of the output rate about 150 dB below the signal.
 * In double those tones disappear. The transfer function is unchanged. Meant
 * for the final stage of a multi-stage downsampler, where the images land in
 * the audio band; the earlier stages' images are removed by the later
 * stages. Costs about the same as the float version on Apple silicon.
 */
void BMIIRDownsampler2x_setDoublePrecision (BMIIRDownsampler2x *This, bool doublePrecision);

void BMIIRDownsampler2x_setCoefs (BMIIRDownsampler2x *This, const double* coef_arr);

void BMIIRDownsampler2x_processBufferMono (BMIIRDownsampler2x *This, const float* input, float* output, size_t numSamplesIn);

void BMIIRDownsampler2x_processBufferStereo (BMIIRDownsampler2x *This, const float* inputL, const float* inputR, float* outputL, float* outputR, size_t numSamplesIn);


#endif /* BMIIRDownsampler2x_h */
