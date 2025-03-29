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
void BMCompressor_ProcessBufferMono(BMCompressor *This, const float* input, float* output, float* minGainDb, size_t numSamples);


void BMCompressor_ProcessBufferMonoWithSideChain(BMCompressor *This,
												 const float *input,
												 const float *scInput,
												 float* output,
												 float* minGainDb,
												 size_t numSamples);


/*!
 *BMCompressor_ProcessBufferStereo
 */
void BMCompressor_ProcessBufferStereo(BMCompressor *This, float* inputL, float* inputR, float* outputL, float* outputR, float* minGainDb, size_t numSamples);


void BMCompressor_ProcessBufferStereoWithSideChain(BMCompressor *This,
                                                   float* inputL, float* inputR,
                                                   float* scInputL, float* scInputR,
                                                   float* outputL, float* outputR,
                                                   float* minGainDb, size_t numSamples);

void BMCompressor_SetThresholdInDB(BMCompressor *This, float threshold);
void BMCompressor_SetRatio(BMCompressor *This, float ratio);

/*!
 * BMCompressor_SetAttackTime
 *
 * Set attack time in units of seconds
 *
 * @param This pointer to an initialised compressor struct
 * @param attackTime time in seconds
 */
void BMCompressor_SetAttackTime(BMCompressor *This, float attackTime);

/*!
 * BMCompressor_SetReleaseTime
 *
 * Set release time in units of seconds
 *
 * @param This pointer to an initialised compressor struct
 * @param releaseTime time in seconds
 */
void BMCompressor_SetReleaseTime(BMCompressor *This, float releaseTime);

void BMCompressor_SetSampleRate(BMCompressor *This, float sampleRate);

void BMCompressor_SetKneeWidthInDB(BMCompressor *This, float kneeWidth);

/*!
 *BMCompressor_SetOutputGain
 *
 * Set the output gain in decibels. (0 means no gain change)
 *
 * @param This pointer to an initialized struct
 * @param outputGain output gain in decibels, also called make-up gain
 */
void BMCompressor_SetOutputGain(BMCompressor *This, float outputGain);

void BMCompressor_Free(BMCompressor *This);

#ifdef __cplusplus
}
#endif

#endif /* BMCompressor_h */
