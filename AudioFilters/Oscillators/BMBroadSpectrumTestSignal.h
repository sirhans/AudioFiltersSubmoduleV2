//
//  BMBroadSpectrumTestSignal.h
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 7/23/25.
//  We release this file into the public domain.
//

#ifndef BMBroadSpectrumTestSignal_h
#define BMBroadSpectrumTestSignal_h

#include <stdio.h>
#include <stdbool.h>
#include "BMOscillatorArray.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BMBroadSpectrumTestSignal {
    float sampleRate;
    size_t lengthSamples;
    BMOscillatorArray array;
    double *outputBuffer;
} BMBroadSpectrumTestSignal;


/*!
 *BMBroadSpectrumTestSignal_init
 */
void BMBroadSpectrumTestSignal_init(BMBroadSpectrumTestSignal *This,
                                    float minFrequency,
                                    float maxFrequency,
                                    size_t numOscillators,
                                    bool randomPhase,
									bool logarithmic,
                                    float sampleRate);


/*!
 *BMBroadSpectrumTestSignal_free
 */
void BMBroadSpectrumTestSignal_free(BMBroadSpectrumTestSignal *This);


/*!
 *BMBroadSpectrumTestSignal_processMono
 */
void BMBroadSpectrumTestSignal_processMono(BMBroadSpectrumTestSignal *This,
                                           float *output,
                                           size_t numSamples);

/*!
 *BMBroadSpectrumTestSignal_processStereo
 */
void BMBroadSpectrumTestSignal_processStereo(BMBroadSpectrumTestSignal *This,
                                             float *outputL, float *outputR,
                                             size_t numSamples);


#ifdef __cplusplus
}
#endif

#endif /* BMBroadSpectrumTestSignal_h */
