//
//  BMVelvetNoise.c
//  BMAudioFilters
//
//  Created by Hans on 1/6/17.
//
//  The author releases this file into the public domain with no restrictions
//  of any kind.
//

#ifdef __cplusplus
extern "C" {
#endif

#include "BMVelvetNoise.h"
#include "BMVelvetNoiseInternal.h"
#include <stdatomic.h>
#include <stdlib.h>

// One counter for all automatic velvet-noise streams. Relaxed atomic access
// gives concurrent initializers distinct seeds without sharing generator state.
// Seeds repeat only after 2^32 allocations; this is not an entropy source.
static _Atomic(uint32_t) BMVelvetNoise_nextSeed = UINT32_C(20260912);

void BMVelvetNoise_initRandomizer(BMRandom *rng){
    uint32_t seed = atomic_fetch_add_explicit(&BMVelvetNoise_nextSeed, 1, memory_order_relaxed);
    BMRandom_init(rng, seed);
}

void BMVelvetNoise_setTapIndices(float startTimeMS, float endTimeMS,
                                size_t *indicesOut, float sampleRate, size_t numTaps){
    BMRandom rng;
    BMVelvetNoise_initRandomizer(&rng);
    BMVelvetNoise_setTapIndicesInternal(&rng, startTimeMS, endTimeMS,
                                       indicesOut, sampleRate, numTaps);
}

void BMVelvetNoise_setTapSigns(float *tapSigns, size_t numTaps){
    BMRandom rng;
    BMVelvetNoise_initRandomizer(&rng);
    BMVelvetNoise_setTapSignsInternal(&rng, tapSigns, numTaps);
}

	
    /*!
	 *BMVelvetNoise_setTapIndices
     *
	 * @abstract Set tap indices using Velvet Noise method
     *
     * @notes (See: "Reverberation modeling using velvet noise" by M. Karjalainen and Hanna Järveläinen. https://www.researchgate.net/publication/283247924_Reverberation_modeling_using_velvet_noise
     *
     * @param startTimeMS   the first tap time will start soon after this
     * @param endTimeMS     the last tap will end before this time
     * @param indicesOut    array of indices for setting delay taps
     * @param sampleRate    sample rate of the audio system
     * @param numTaps       length of indicesOut
     */
    void BMVelvetNoise_setTapIndicesInternal(BMRandom *rng,
                                     float startTimeMS,
                                     float endTimeMS,
                                     size_t* indicesOut,
                                     float sampleRate,
                                     size_t numTaps){
        
        // compute the spacing for evenly spaced between startTime and endTime.
        // In double precision: in single, a jitter within a few millionths of
        // the top of its cell rounded up into the next cell's first sample,
        // which matters once the cells are only a few samples wide.
        double incrementSamples = (double)sampleRate * ((double)endTimeMS - (double)startTimeMS) / 1000.0 / (double)numTaps;
        double startTimeSamples = (double)sampleRate * (double)startTimeMS / 1000.0;
        
        for(size_t i=0; i< numTaps; i++){
            // generate an evenly spaced tap time
            double tapTime = (double)i * incrementSamples + startTimeSamples;
            
            // add a random jitter in [0, one cell) to get uneven spacing: every
            // point of the cell is equally likely, and so is every sample of it
            tapTime += incrementSamples * (double)BMRandom_float01(rng);
            
            // convert to unsigned long (size_t)
            indicesOut[i] = (size_t)tapTime;
        }
    }
    
	
	
	
    /*
     * Swap the values at positions i and j in the array A
     */
    void BMVelvetNoise_swapAt(float* A, size_t i, size_t j){
        float t = A[i];
        A[i] = A[j];
        A[j] = t;
    }
    
    
    
    
    /*
     * randomly shuffle the order of elements in A
     */
    void BMVelvetNoise_randomShuffle(float* A, size_t length){
        BMRandom rng;
        BMVelvetNoise_initRandomizer(&rng);
        BMRandom_shuffleFloats(&rng, A, length);
    }
    
    
    
    /*!
	 *BMVelvetNoise_setTapSigns
	 *
     * @abstract Set the values in tapSigns randomly to -1 and 1, with an equal number of + and - values.
     *
     * @param tapSigns   input array
     * @param numTaps    length of tapSigns
     */
    void BMVelvetNoise_setTapSignsInternal(BMRandom *rng,
                                   float* tapSigns,
                                   size_t numTaps){
        // set half the signs negative and the other half positive
        size_t i=0;
        for(; i<numTaps/2; i++)
            tapSigns[i] = 1.0f;
        for(; i<numTaps; i++)
            tapSigns[i] = -1.0f;
        
        // shuffle the order of the signs randomly
        BMRandom_shuffleFloats(rng, tapSigns, numTaps);
    }
    

#ifdef __cplusplus
}
#endif
