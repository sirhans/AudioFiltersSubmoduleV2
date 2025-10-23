//
//  BMTriangleLFO.h
//  Blue Mangoo Signal Processing Toolbox
//
//  Created by hans anderson on 12/3/24.
//
//  This is a triangle wave oscillator suitable for low frequency use. It
//  includes no antialiasing features, so it should not be used at audio
//  frequencies. However, a triangle wave without anti-aliasing is preferable
//  as a control signal for modulating filters and other effects because it
//  is free from the ringing effects the anti-aliasing filters produce.
//
//  We release this file into the public domain. Use it as you wish without
//  restrictions of any kind.
//

#ifndef BMTriangleLFO_h
#define BMTriangleLFO_h

#include <stdio.h>
#include "../OtherEffects/BMSmoothValue.h"

typedef struct BMTriangleLFO {
	float sampleRate, freq, phase, min, max;
	BMSmoothValue minValue, scale;
	float *buffer;
} BMTriangleLFO;




/*!
 *BMTriangleLFO_init
 *
 * @abstract Initialize the rotation speed and set initial values (no memory is allocated for this)
 *
 * @param This       pointer to an oscillator struct
 * @param fHz        frequency in Hz
 * @param min		 min value of the triangle wave
 * @param max		 max value of the triangle wave
 * @param sampleRate sample rate in Hz
 */
void BMTriangleLFO_init(BMTriangleLFO *This,
						float fHz,
						float min,
						float max,
						float sampleRate);




/*!
 *BMTriangleLFO_free
 */
void BMTriangleLFO_free(BMTriangleLFO *This);



/*!
 *BMTriangleLFO_setFrequency
 */
void BMTriangleLFO_setFrequency(BMTriangleLFO *This, float fHz);





/*!
 *BMTriangleLFO_process
 *
 * @abstract Generate numSamples of oscillation into the array output.  Continues smoothly from the previous function call.
 *
 * @param output     an array for output
 * @param numSamples length of output
 *
 */
void BMTriangleLFO_process(BMTriangleLFO *This,
						   float* output,
						   size_t numSamples);




/*!
 *BMTriangleLFO_advance
 *
 * @abstract skip ahead by numSamples, outputting only the first sample
 *
 * @param output     pointer to a single floating point value for output
 * @param numSamples number of samples to skip
 */
void BMTriangleLFO_advance(BMTriangleLFO *This,
						   float *output,
						   size_t numSamples);






/*!
 *BMTriangleLFO_setMinMaxSmoothly
 *
 * @abstract This sets the min and max values of the oscillator output with smoothing applied to prevent harsh clicks.
 *
 * @param minVal minimum value of the oscillator output
 * @param maxVal maximum value of the oscillator output
 */
void BMTriangleLFO_setMinMaxSmoothly(BMTriangleLFO *This, float minVal, float maxVal);




/*!
 *BMTriangleLFO_setMinMaxImmediately
 *
 * @abstract This sets the min and max values of the oscillator output immediately, without smoothing
 *
 * @param minVal minimum value of the oscillator output
 * @param maxVal maximum value of the oscillator output
 */
void BMTriangleLFO_setMinMaxImmediately(BMTriangleLFO *This, float minVal, float maxVal);





#endif /* BMTriangleLFO_h */
