//
//  BMLoopFinder.h
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 9/26/25.
//  We hereby release this code into the public domain.
//

#ifndef BMLoopFinder_h
#define BMLoopFinder_h

#include <stdio.h>

typedef struct bmLoopPoints {
	size_t start;
	size_t end;
	float loopNoise; // this measures how much noise is caused by errors in the loop points. Less noise is better
} bmLoopPoints;

/*!
 *BMFindLoop
 *
 * Finds the best pair of indices for looping in *buffer
 *
 * @param audioBuffer an array of audio samples in which you want to find loop points
 * @param sampleRate audio buffer sample rate
 * @param frequency fundamaental frequency of the tone in audioBuffer (it's ok if it's not 100% stable)
 * @param audioBufferLength length of audio buffer in samples
 * @param minLoopLength the minimum acceptable length, in samples, of the loop. Usually this is the wavelength of the sound, minus a few percent in case the pitch isn't stable.
 * @param maxLoopLength must be <= numSamples
 * @param loopNoiseTargetDb loopNoise is the unwanted noise caused by loop point errors. This function will stop searching and return a result immediately if it finds something has less noise than this target, in decibels. If it can't find a loop with less noise than the target, it returns the lowest noise loop. Setting a higher value for this makes the code run faster because it doesn't have to search as long to find the perfect loop. Setting it lower gives you better quality loop points. Try setting -50 dB as a starting point for the loop noise target.
 *
 * @returns a pair of loop points, indices in buffer
 */
bmLoopPoints BMFindLoop(float *audioBuffer,
						float sampleRate,
						float frequency,
						size_t audioBufferLength,
						size_t minLoopLength,
						size_t maxLoopLength,
						float loopNoiseTargetDb);

#endif /* BMLoopFinder_h */
