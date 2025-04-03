//
//  BMSimpleDecimator.c
//  BMTherapist
//
//  Created by hans anderson on 4/2/25.
//

#include "BMSimpleDecimator.h"

/*!
 *BMSimpleDecimate
 */
void BMSimpleDecimate(const float *input,
					  float *output,
					  size_t inputLength,
					  size_t outputLength){
	// check that the input length is a multiple of the output length
	assert(inputLength % outputLength == 0);
	
	// this does not work in-place
	assert(input != output);
	
	size_t decimationFactor = inputLength / outputLength;
	
	for (size_t i=0; i<outputLength; i++){
		output[i] = input[i*decimationFactor];
	}
}

