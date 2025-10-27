//
//  BMDither.h
//  AudioFiltersXcodeProject
//
//  This is a dithering effect that takes audio buffers as input and adds
//  dithering noise to them. It automatically stops generating dithering noise
//  when the input is silent. We filter the dithering to make it less audible by
//  boosting the dithering volume in frequency ranges that are less audible and
//  cutting in more audible ranges. 
//
//
//  Created by hans anderson on 10/13/25.
//  I release this file into the public domain.
//

#ifndef BMDither_h
#define BMDither_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "BMMultiLevelBiquad.h"
#include "BMMultiLevelSVF.h"
#include "BMSmoothSwitch.h"
#include "BMWhiteNoiseOscillator.h"

typedef enum {BMDITHER_16} DitherBitDepth;


typedef struct BMDither {
	BMMultiLevelBiquad filter;
	BMMultiLevelSVF dcBlocker;
	BMSmoothSwitch smoothSwitch;
	BMWhiteNoiseOscillator whiteNoise;
	float *bufferL, *bufferR;
	float noiseGain;
} BMDither;


/*!
 *BMDither_init
 */
void BMDither_init(BMDither *This, DitherBitDepth depth, float sampleRate);


/*!
 *BMDither_processStereo
 */
void BMDither_processStereo(BMDither *This, float *inL, float *inR, float *outL, float *outR, size_t numSamples);


/*!
 *BMDither_free
 */
void BMDither_free(BMDither *This);

#ifdef __cplusplus
}
#endif

#endif /* BMDither_h */
