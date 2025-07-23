//
//  BMLongFade.h
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 7/23/25.
//  We hereby release this file into the public domain.
//

#ifndef BMLongFade_h
#define BMLongFade_h

#include <stdio.h>

typedef struct BMLongFade{
	float gainDB, startGainDB, endGainDB;
	size_t lengthSamples, samplesProcessed;
	float *buffer;
} BMLongFade;

/*!
 *BMLongFade_init
 *
 * @param startGainDB gain at the beginning of the fade
 * @param endGainDB gain at the end of the fade
 * @param lengthInSeconds length of the fade in seconds. This doesn't need to be exact. If you process more than the specified length, it will just stop and rest at endGainDb.
 */
void BMLongFade_init(BMLongFade *This, float startGainDB, float endGainDB, float lengthInSeconds, float sampleRate);

/*!
 *BMLongFade_free
 */
void BMLongFade_free(BMLongFade *This);

/*!
 *BMLongFade_processStereo
 */
void BMLongFade_processStereo(BMLongFade *This,
							  float *inL, float *inR,
							  float *outL, float *outR,
							  size_t numSamples);

#endif /* BMLongFade_h */
