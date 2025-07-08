//
//  BMSimpleDecimator.h
//  BMTherapist
//
//  Created by hans anderson on 4/2/25.
//

#ifndef BMSimpleDecimator_h
#define BMSimpleDecimator_h

#include <stdio.h>
#include <assert.h>

void BMSimpleDecimate(const float *input,
					  float *output,
					  size_t inputLength,
					  size_t outputLength);

#endif /* BMSimpleDecimator_h */
