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
	float quality;
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
 * @param qualityGoal the function will stop searching and return a result immediately if it finds something that is as at least as good as this quality goal. The max value for this is 0.0, which is a perfect loop, like what you get on synth sounds. Any loop that isn't perfect has a negative number for quality. So, for example a really great loop that isn't 100% perfect might be quality of -0.001. You can set this to 0.0 if you just want to find the best loop every time. But sometimes that might take too much time. So if you find you can get a loop that's good enough with quality = -0.1, for example, then set it like that and the code will run faster.
 *
 * @returns a pair of loop points, indices in buffer
 */
bmLoopPoints BMFindLoop(float *audioBuffer,
						float sampleRate,
						float frequency,
						size_t audioBufferLength,
						size_t minLoopLength,
						size_t maxLoopLength,
						float qualityGoal);

#endif /* BMLoopFinder_h */
