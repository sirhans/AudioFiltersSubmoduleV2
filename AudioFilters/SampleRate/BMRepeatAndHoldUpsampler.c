//
//  BMRepeatAndHoldUpsampler.c
//  BMTherapist
//
//  Created by hans anderson on 4/2/25.
//
//  The author of this file releases it into the public domain with no
//  restrictions on its use.
//

#include "BMRepeatAndHoldUpsampler.h"
#include <assert.h>


void BMRepeatAndHoldUpsample(const float *input,
							 float *output,
							 size_t inputLength,
							 size_t outputLength){
	
	// check that the output length is a multiple of the input length
	assert(outputLength % inputLength == 0);
	
	// this does not work in-place
	assert(input != output);
	
	size_t upsampleFactor = outputLength / inputLength;
	
	// copy from input to output, repeating each input element upsampleFactor
	// times in the output.
	size_t j = 0;
	for(size_t i=0; i<outputLength; ){
		output[i++] = input[j];
		if (i % upsampleFactor == 0) j++;
	}
}
