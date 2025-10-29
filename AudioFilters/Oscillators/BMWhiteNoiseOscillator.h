//
//  BMWhiteNoiseOscillator.h
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 10/13/25.
//  Copyright © 2025 BlueMangoo. All rights reserved.
//

#ifndef BMWhiteNoiseOscillator_h
#define BMWhiteNoiseOscillator_h

#include <stdio.h>
#include "../MathUtilities/TinyMT/tinymt32.h"

typedef struct BMWhiteNoiseOscillator {
	tinymt32_t randomGenerator;
} BMWhiteNoiseOscillator;

void BMWhiteNoiseOscillator_init(BMWhiteNoiseOscillator *This);

void BMWhiteNoiseOscillator_process(BMWhiteNoiseOscillator *This, float *output, size_t numSamples);

#endif /* BMWhiteNoiseOscillator_h */
