//
//  BMCompressor.h
//  CompressorApp
//
//  Created by Duc Anh on 7/17/18.
//
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
//

#ifndef BMCompressor_h
#define BMCompressor_h


#include <stdio.h>
#include <stdbool.h>
#include "BMMultiLevelBiquad.h"
#include "BMEnvelopeFollower.h"
#include "BMQuadraticThreshold.h"
#include "BMSmoothGain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    float thresholdInDB, kneeWidthInDB, releaseTime, attackTime,slope;
    BMEnvelopeFollower envelopeFollower;
    BMQuadraticThreshold quadraticThreshold;
	BMSmoothGain outputGain;
    float *buffer1, *buffer2;
} BMCompressor;

void BMCompressor_init(BMCompressor *This, float sampleRate);

void BMCompressor_initWithSettings(BMCompressor *This, float sampleRate, float thresholdInDB, float kneeWidthInDB, float ratio, float attackTime, float releaseTime, float outputGainInDB);

/*!
 * BMCompressor_ProcessBufferMono
 * @param This         pointer to a compressor struct
 * @param input        input array of length "length"
 * @param output       output array of length "length"
 * @param minGainDb    the lowest gain setting the compressor reached while processing the buffer
 * @param numSamples   length of arrays
 * @brief apply dynamic range compression to input; result in output (MONO)
 * @notes result[i] is 1.0 where X[i] is within limits, 0.0 otherwise
 * @discussion returns floating point output for use in vectorised code without conditional branching
 * @code result[i] = -1.0f * (X[i] >= lowerLimit && X[i] <= upperLimit);
 * @warning no warnings
 */
void BMCompressor_processBufferMono(BMCompressor *This, const float* input, float* output, float* minGainDb, size_t numSamples);


void BMCompressor_processBufferMonoWithSideChain(BMCompressor *This,
												 const float *input,
												 const float *scInput,
												 float* output,
												 float* minGainDb,
												 size_t numSamples);


/*!
 *BMCompressor_ProcessBufferStereo
 */
void BMCompressor_processBufferStereo(BMCompressor *This, float* inputL, float* inputR, float* outputL, float* outputR, float* minGainDb, size_t numSamples);


void BMCompressor_processBufferStereoWithSideChain(BMCompressor *This,
                                                   float* inputL, float* inputR,
                                                   float* scInputL, float* scInputR,
                                                   float* outputL, float* outputR,
                                                   float* minGainDb, size_t numSamples);

void BMCompressor_setThresholdInDB(BMCompressor *This, float threshold);
void BMCompressor_setRatio(BMCompressor *This, float ratio);

/*!
 * BMCompressor_SetAttackTime
 *
 * Set attack time in units of seconds
 *
 * @param This pointer to an initialised compressor struct
 * @param attackTime time in seconds
 */
void BMCompressor_setAttackTime(BMCompressor *This, float attackTime);

/*!
 * BMCompressor_SetReleaseTime
 *
 * Set release time in units of seconds
 *
 * @param This pointer to an initialised compressor struct
 * @param releaseTime time in seconds
 */
void BMCompressor_setReleaseTime(BMCompressor *This, float releaseTime);

void BMCompressor_setSampleRate(BMCompressor *This, float sampleRate);

void BMCompressor_setKneeWidthInDB(BMCompressor *This, float kneeWidth);

/*!
 *BMCompressor_SetOutputGain
 *
 * Set the output gain in decibels. (0 means no gain change)
 *
 * @param This pointer to an initialized struct
 * @param outputGain output gain in decibels, also called make-up gain
 */
void BMCompressor_setOutputGain(BMCompressor *This, float outputGain);

void BMCompressor_free(BMCompressor *This);

#ifdef __cplusplus
}
#endif

#endif /* BMCompressor_h */
