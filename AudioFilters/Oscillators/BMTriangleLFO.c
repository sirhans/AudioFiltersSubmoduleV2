//
//  BMTriangleLFO.c
//  BMTherapist
//
//  Created by hans anderson on 12/3/24.
//
//  We release this file into the public domain. Use it as you wish without
//  restrictions of any kind.
//

#include "BMTriangleLFO.h"
#include "../AudioFilter.h"
#define BMTriangleLFO_PARAMETER_UPDATE_TIME_SECONDS 1.0





/*!
 *BMTriangleLFO_init
 *
 * @abstract Initialize the rotation speed and set initial values (no memory is allocated for this)
 *
 * @param This       pointer to an oscillator struct
 * @param fHz        frequency in Hz
 * @param min 		 min value of the triangle wave
 * @param max		 max value of the triangle wave
 * @param sampleRate sample rate in Hz
 */
void BMTriangleLFO_init(BMTriangleLFO *This,
						float fHz,
						float min,
						float max,
						float sampleRate){
	This->phase = 0.0;
	This->sampleRate = sampleRate;
	BMTriangleLFO_setFrequency(This, fHz);
	BMSmoothValue_init(&This->minValue, BMTriangleLFO_PARAMETER_UPDATE_TIME_SECONDS, sampleRate);
	BMSmoothValue_init(&This->scale, BMTriangleLFO_PARAMETER_UPDATE_TIME_SECONDS, sampleRate);
	BMTriangleLFO_setMinMaxImmediately(This, min, max);
	
	This->buffer = malloc(sizeof(float) * BM_BUFFER_CHUNK_SIZE);
}





void BMTriangleLFO_free(BMTriangleLFO *This){
	free(This->buffer);
	This->buffer = NULL;
}




/*!
 *BMTriangleLFO_setFrequency
 */
void BMTriangleLFO_setFrequency(BMTriangleLFO *This, float fHz){
	This->freq = fHz;
}



/*!
 *BMTriangleLFO_triangleWave
 *
 * Generate a triangle wave with output in range [-1,1] based on input array of phases in range [0,1]. Phases do not actually need to be clipped to that range because they will be wrapped automatically if they fall outside. However, for better numerical stability, phase values should not be so large that the mod operation results in significant loss of precision.
 */
void BMTriangleLFO_triangleWave(float *phases, float *output, size_t numSamples){
	// triangle wave prototype tested in Mathematica:
	//   -1 + 4*Abs[-1/2 + Mod[3/4 + (t_), 1]]
	//
	// written in c form:
	//   4.0 * fabsf(fmodf(t + 0.75, 1.0) - 0.5) - 1.0
	//
	// below we implement this in a vectorised format
	float threeOverFour = 0.75;
	// t + 0.75
	vDSP_vsadd(output, 1, &threeOverFour, output, 1, numSamples);
	// fmod(t + 0.75, 1.0)
	for(size_t i=0; i<numSamples; i++) output[i] = fmodf(output[i], 1.0);
	// (fmodf(t + 0.75, 1.0) - 0.5)
	float neg1Over2 = -0.5;
	vDSP_vsadd(output, 1, &neg1Over2, output, 1, numSamples);
	// fabsf(fmodf(t + 0.75, 1.0) - 0.5)
	vDSP_vabs(output, 1, output, 1, numSamples);
	//   4.0 * fabsf(fmodf(t + 0.75, 1.0) - 0.5) - 1.0
	float four = 4.0;
	float negOne = -1.0;
	vDSP_vsmsa(output, 1, &four, &negOne, output, 1, numSamples);
}



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
						   size_t numSamples){
	size_t samplesRemaining = numSamples;
	size_t samplesProcessed = 0;
	while(samplesRemaining > 0){
		size_t samplesProcessing = BM_MIN(samplesRemaining, BM_BUFFER_CHUNK_SIZE);
		
		// compute the starting phase for this buffer of output
		float phaseIncrementPerSample = This->freq / This->sampleRate;
		This->phase += phaseIncrementPerSample;
		
		// wrap around to keep the number in [0,1] for better numerical accuracy
		This->phase = fmodf(This->phase, 1.0f);
		
		// compute the phase at the end of the buffer.
		// casts to double prevent precision errors during the calculation
		float phaseIncrementPerBuffer = (float)((double)This->freq * (double)(samplesProcessing-1) / (double)This->sampleRate);
		float endPhase = This->phase + phaseIncrementPerBuffer;
		
		// get an output pointer, shifted according to where in the output we are
		// processing this time through the loop
		float *outputSft = output + samplesProcessed;
		
		// generate a ramp of ascending values representing the phase of the
		// oscillator at each sample in output.
		// note that phase for this oscillator is between 0 and 1, not 0 and 2 Pi.
		vDSP_vgen(&This->phase, &endPhase, outputSft, 1, samplesProcessing);
		
		// set This->phase to the end of the buffer
		This->phase = endPhase;
		
		// generate the triangle wave in [-1,1]
		BMTriangleLFO_triangleWave(outputSft, outputSft, samplesProcessing);
		
		// scale and shift the wave so that the min and max values are correct
		//
		// scale the sine wave
		BMSmoothValue_process(&This->scale, This->buffer, samplesProcessing);
		vDSP_vmul(outputSft, 1, This->buffer, 1, outputSft, 1, samplesProcessing);
		
		// shift the sine wave
		BMSmoothValue_process(&This->minValue, This->buffer, samplesProcessing);
		vDSP_vadd(outputSft, 1, This->buffer, 1, outputSft, 1, samplesProcessing);
		
		// advance the output index shift
		samplesProcessed += samplesProcessing;
		
		// update the progress count
		samplesRemaining -= samplesProcessing;
	}
}




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
						   size_t numSamples){
	
	// compute the phase at the end of the buffer.
	// casts to double prevent precision errors during the calculation
	float phaseIncrementPerBuffer = (float)((double)This->freq * (double)(numSamples) / (double)This->sampleRate);
	float endPhase = This->phase + phaseIncrementPerBuffer;
	
	// wrap around to keep the number in [0,1] for better numerical accuracy
	This->phase = fmodf(endPhase, 1.0f);
	
	// compute the value of the triangle wave after advanceing numSamples ahead
	//
	// do the calculation (based on textbook formula for a triangle wave with period p)
	*output = 4.0 * fabs(fmod(This->phase + 0.75, 1.0) - 0.5) - 1.0;
	
	// get the scale and apply it to the oscillator output.
	float scale = BMSmoothValue_advance(&This->scale, numSamples);
	*output *= scale;
		
	// get the shift and apply it to the oscillator output.
	float shift = BMSmoothValue_advance(&This->minValue, numSamples);
	*output += shift;
}






/*!
 *BMTriangleLFO_setMinMaxSmoothly
 *
 * @abstract This sets the min and max values of the oscillator output with smoothing applied to prevent harsh clicks.
 *
 * @param minVal minimum value of the oscillator output
 * @param maxVal maximum value of the oscillator output
 */
void BMTriangleLFO_setMinMaxSmoothly(BMTriangleLFO *This, float minVal, float maxVal){
	
	float scale = maxVal - minVal;
	
	// the scale of the quadrature oscillator output is [-1,1] so we need to
	// multiply by 0.5 here
	BMSmoothValue_setValueSmoothly(&This->scale, 0.5 * scale);
	//
	// after multiplying by this scale, the range of oscillator output will be
	// [-scale/2, scale/2]
	
	// because the quadrature oscillator outputs in the range [-1,1], this
	// math is more complex than we originally hoped
	float minShifted = (scale * 0.5) + minVal;
	BMSmoothValue_setValueSmoothly(&This->minValue, minShifted);
	//
	// after adding this minShifted, the range of the oscillator output will be
	//    [-scale/2, scale/2] + minShifted
	// == [-(max-min)*0.5,(max-min)*0.5] + ((max - min) * 0.5) + min
	// == [-(max-min)*0.5 + ((max - min) * 0.5), (max-min)*0.5 + ((max - min) * 0.5)] + min
	// == [0,(max-min)] + min
	// == [min,max]
}




/*!
 *BMTriangleLFO_setMinMaxImmediately
 *
 * @abstract This sets the min and max values of the oscillator output immediately, without smoothing
 *
 * @param minVal minimum value of the oscillator output
 * @param maxVal maximum value of the oscillator output
 */
void BMTriangleLFO_setMinMaxImmediately(BMTriangleLFO *This, float minVal, float maxVal){
	float scale = maxVal - minVal;
	
	// the scale of the quadrature oscillator output is [-1,1] so we need to
	// multiply by 0.5 here
	BMSmoothValue_setValueImmediately(&This->scale, 0.5 * scale);
	
	// because the quadrature oscillator outputs in the range [-1,1], this
	// math is more complex than we originally hoped
	float minShifted = (scale * 0.5) + minVal;
	BMSmoothValue_setValueImmediately(&This->minValue, minShifted);
	//
	// after adding this minShifted, the range of the oscillator output will be
	//    [-scale/2, scale/2] + minShifted
	// == [-(max-min)*0.5,(max-min)*0.5] + ((max - min) * 0.5) + min
	// == [-(max-min)*0.5 + ((max - min) * 0.5), (max-min)*0.5 + ((max - min) * 0.5)] + min
	// == [0,(max-min)] + min
	// == [min,max]
}

