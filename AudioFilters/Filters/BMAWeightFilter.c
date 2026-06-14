//
//  BMAWeightFilter.c
//  AudioFilters
//
//  Created by Hans Anderson on 6/14/26.
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
//

#include "BMAWeightFilter.h"

#define BM_A_WEIGHT_FILTER_NUM_LEVELS 4

static void BMAWeightFilter_setCoefficients(BMMultiLevelBiquad *filter) {
    BMMultiLevelBiquad_setHighPassQ12db(filter, 20.6, 0.5, 0);
    BMMultiLevelBiquad_setHighPass6db(filter, 107.7, 1);
    BMMultiLevelBiquad_setHighPass6db(filter, 737.9, 2);
    BMMultiLevelBiquad_setLowPassQ12db(filter, 11500.0, 0.5, 3);
    BMMultiLevelBiquad_setGainInstant(filter, 1.981927);
}

void BMAWeightFilter_init(BMAWeightFilter *This, float sampleRate) {
    BMMultiLevelBiquad_init(&This->monoFilter,
                            BM_A_WEIGHT_FILTER_NUM_LEVELS,
                            sampleRate,
                            false,
                            false,
                            false);
    BMAWeightFilter_setCoefficients(&This->monoFilter);

    BMMultiLevelBiquad_init(&This->stereoFilter,
                            BM_A_WEIGHT_FILTER_NUM_LEVELS,
                            sampleRate,
                            true,
                            false,
                            false);
    BMAWeightFilter_setCoefficients(&This->stereoFilter);
}

void BMAWeightFilter_free(BMAWeightFilter *This) {
    BMMultiLevelBiquad_free(&This->monoFilter);
    BMMultiLevelBiquad_free(&This->stereoFilter);
}

void BMAWeightFilter_processMono(BMAWeightFilter *This,
                                 const float *input,
                                 float *output,
                                 size_t numSamples) {
    BMMultiLevelBiquad_processBufferMono(&This->monoFilter,
                                         input,
                                         output,
                                         numSamples);
}

void BMAWeightFilter_processStereo(BMAWeightFilter *This,
                                   const float *inputL,
                                   const float *inputR,
                                   float *outputL,
                                   float *outputR,
                                   size_t numSamples) {
    BMMultiLevelBiquad_processBufferStereo(&This->stereoFilter,
                                           inputL,
                                           inputR,
                                           outputL,
                                           outputR,
                                           numSamples);
}
