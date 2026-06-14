//
//  BMAWeightFilter.h
//  AudioFilters
//
//  Created by Hans Anderson on 6/14/26.
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
//

#ifndef BMAWeightFilter_h
#define BMAWeightFilter_h

#include <stdio.h>
#include "BMMultiLevelBiquad.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BMAWeightFilter {
    BMMultiLevelBiquad monoFilter;
    BMMultiLevelBiquad stereoFilter;
} BMAWeightFilter;

/*!
 * BMAWeightFilter_init
 *
 * @param This pointer to an initialized BMAWeightFilter struct
 * @param sampleRate audio sample rate
 */
void BMAWeightFilter_init(BMAWeightFilter *This, float sampleRate);

/*!
 * BMAWeightFilter_free
 */
void BMAWeightFilter_free(BMAWeightFilter *This);

/*!
 * BMAWeightFilter_processMono
 *
 * @param input input buffer length >= numSamples
 * @param output output buffer length >= numSamples
 */
void BMAWeightFilter_processMono(BMAWeightFilter *This,
                                 const float *input,
                                 float *output,
                                 size_t numSamples);

/*!
 * BMAWeightFilter_processStereo
 *
 * @param inputL left input buffer length >= numSamples
 * @param inputR right input buffer length >= numSamples
 * @param outputL left output buffer length >= numSamples
 * @param outputR right output buffer length >= numSamples
 */
void BMAWeightFilter_processStereo(BMAWeightFilter *This,
                                   const float *inputL,
                                   const float *inputR,
                                   float *outputL,
                                   float *outputR,
                                   size_t numSamples);

#ifdef __cplusplus
}
#endif

#endif /* BMAWeightFilter_h */
