//
//  BMImitationFilter.h
//  AudioFiltersXcodeProject
//
//  This is an implementation of a Least Mean Squares filter. It takes three
//  inputs:
//  1. an unfiltered audio signal.
//  2. a copy of signal 1 that has been filtered through an unknown filter.
//  3. a second unfiltered signal, to which we will apply an approximate
//     imitation of the unknown filter that produced signal 2.
//
//  We assume that the unknown filter is linear and time-invariant.
//  - linear means it does the same thing regardless of input volume. examples
//  of non-linear filters include compressors and limiters.
//  - time-invariant means the filter doesn't change over time
//
//  The assumption of time-invariance can be relaxed to accomodate filters that
//  change slowly over time. However, if the unknown makes any sudden changes,
//  it will take this filter some time to adapt to match it.
//
//  For details about how a Least Mean Squares filter works, see here:
//  https://en.wikipedia.org/wiki/Least_mean_squares_filter#LMS_algorithm_summary
//
//  Created by hans anderson on 5/12/25.
//  We release this file into the public domain with no restrictions.
//

#ifndef BMImitationFilter_h
#define BMImitationFilter_h

#include <stdio.h>
#include "TPCircularBuffer.h"

typedef struct BMImitationFilter {
	TPCircularBuffer buffer1, buffer2;
	float *h;
	float mu;
	size_t filterOrder;
} BMImitationFilter;


/*!
 *BMImitationFilter_init
 *
 * @param This pointer to an uninitialized struct
 * @param filterOrder higher numbers yield more precise filter curves, but beware overfitting! If the unknown filter is simple then the filter order should be low. If you don't know, try starting with 16.
 * @param convergenceRate in (0,1). 0 means the filter doesn't adapt at all. 1 means it completely resets with every sample.
 */
void BMImitationFilter_init(BMImitationFilter *This,
							size_t filterOrder,
							float convergenceRate);

/*!
 *BMImitationFilter_process
 *
 * @param This 				  pointer to an initialized struct
 * @param unfilteredReference the input signal to the unknown filter
 * @param filteredReference   the input signal after passing through the unknown filter
 * @param input 	 		  the signal we want to filter
 * @param output			  we write the filtered version of input here
 * @param error				  we write the residual (error) here for debugging
 * @param numSamples 		  number of samples to process
 */
void BMImitationFilter_process(BMImitationFilter *This,
							   float *unfilteredReference,
							   float *filteredReference,
							   float *input,
							   float *output,
							   float *error,
							   size_t numSamples);

void BMImitationFilter_free(BMImitationFilter *This);


#endif /* BMImitationFilter_h */
