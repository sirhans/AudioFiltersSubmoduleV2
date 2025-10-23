//
//  BMWhiteNoiseOscillator.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 10/13/25.
//  Copyright © 2025 BlueMangoo. All rights reserved.
//

#include "BMWhiteNoiseOscillator.h"
#include "../AudioFilter.h"


void BMWhiteNoiseOscillator_init(BMWhiteNoiseOscillator *This){
	// we are seeding with zero, so we get the same random noise every time.
	// if you don't like that, seed from the clock using (unsigned int)time(NULL)
	// by uncommenting the line below.
	tinymt32_init(&This->randomGenerator, 0);
	
	// uncomment to seed from clock
	//tinymt32_init(&This->randomGenerator, (unsigned int)time(NULL));
}



void BMWhiteNoiseOscillator_process(BMWhiteNoiseOscillator *This, float *output, size_t numSamples){
	
	// generate random numbers in [0,1]
	for (size_t i = 0; i<numSamples; i++){
		output[i] = tinymt32_generate_float(&This->randomGenerator);
	}
	
	// scale to get RMS = 0 dB and shift to get numbers centred around zero
	float scale = sqrtf(2.0 * 3.0);
	float shift = scale * -0.5;
	vDSP_vsmsa(output, 1, &scale, &shift, output, 1, numSamples);
}
