//
//  BMOscillatorArray.h
//  BMAudioFilters
//
//
//  This file implements an array of the same oscillator in
//  BMQuadratureOscillator. The array implementation is vector optimised.
//
//  Created by Hans on 28/4/17.
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
// x
//

#ifndef BMOscillatorArray_h
#define BMOscillatorArray_h

#include "../MathUtilities/BM2x2Matrix.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef struct BMOscillatorArray {
        // arrays of matrix coefficients
        double* m11;
        double* m12;
		double* m21;
		double* m22;
        
        // temp storage array
		double* temp;

        // arrays of latent and active energy stores
		double* r;
		double* q;
        
        size_t length;
    } BMOscillatorArray;
    
    
    /*
     * Initialize
     *
     * @param This       pointer to an oscillator array struct
     * @param magnitude  magnitude energy output of each oscillator
     * @param phase      initial phase of each oscillator
     * @param frequency  frequency of each oscillator
     * @param sampleRate sample rate in Hz
     * @param length     number of oscillators in the array
     *
     * ALL ARRAYS MUST HAVE AT LEAST length ELEMENTS
     */
    void BMOscillatorArray_init(BMOscillatorArray *This,
								double* magnitude,
								double* phase,
								double* frequency,
								double  sampleRate,
                                size_t length);
    

    
    
    
    /*
     * free up memory used by the struct
     */
    void BMOscillatorArray_free(BMOscillatorArray *This);
    
    
    
    
    
    
    
    /*!
	 *BMOscillatorArray_processSample
	 *
     * Generate numSamples of oscillation into the array r.  Continues smoothly
     * from the previous function call.
     *
     * @param output output is written here
     *
     */
    void BMOscillatorArray_processSample(BMOscillatorArray *This,
										 double* output);
    
    
#ifdef __cplusplus
}
#endif

#endif /* BMOscillatorArray_h */

