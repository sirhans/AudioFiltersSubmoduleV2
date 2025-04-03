//
//  BMRepeatAndHoldUpsampler.h
//  BMTherapist
//
//  Created by hans anderson on 4/2/25.
//
//  The author of this file releases it into the public domain with no
//  restrictions on its use.
//

#ifndef BMRepeatAndHoldUpsampler_h
#define BMRepeatAndHoldUpsampler_h

#include <stdio.h>

/*!
 *BMRepeatAndHoldUpsample
 */
void BMRepeatAndHoldUpsample(const float *input,
							 float *output,
							 size_t inputLength,
							 size_t outputLength);

#endif /* BMRepeatAndHoldUpsampler_h */
