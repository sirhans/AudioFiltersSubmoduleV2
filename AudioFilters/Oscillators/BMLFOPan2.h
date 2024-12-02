//
//  BMLFOPan2.h
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 5/8/24.
//
//  This is an auto-pan effect, controlled by an LFO
//

#ifndef BMLFOPan2_h
#define BMLFOPan2_h

#include <stdio.h>
#include "BMLFO.h"

typedef struct BMLFOPan2 {
	BMLFO lfo;
	float *mixControlSignalL, *mixControlSignalR, *buffer;
	bool depthInDb;
} BMLFOPan2;


/*!
 *BMLFOPan2_processStereo
 *
 * Input is mixed to mono before processing.
 */
void BMLFOPan2_processStereoMixdown(BMLFOPan2 *This,
								  float *inL, float *inR,
								  float *outL, float *outR,
								  size_t numSamples);


/*!
 *BMLFOPan2_processStereoQuadratureDb
 *
 * A stereo in, stereo out LFO pan where the depth is modulated in decibel scale and the gain in the L and R channels are modulated by a quadrature phase oscillator.
 *
 */
void BMLFOPan2_processStereoQuadratureDb(BMLFOPan2 *This,
								  float *inL, float *inR,
								  float *outL, float *outR,
								  size_t numSamples);


/*!
 *BMLFOPan2_init
 *
 * @param This pointer to a struct
 * @param LFOFreqHz the LFO frequency in Hz
 * @param depth in [0,1]. 0 means always panned center. 1 means full L-R panning
 * @param sampleRate sample rate in Hz
 */
void BMLFOPan2_init(BMLFOPan2 *This, float LFOFreqHz, float depth, float sampleRate);



/*!
 *BMLFOPan2_initQuadratureDb
 *
 * @abstract This version of the init function is used to set the pan depth in dB scale. The gain of the signal in L and R channels are modulated by a quadrature phase oscillator that controls the gain in decibels. The pan effect is achieved by gain cut only. In other words, the max gain is 0 dB.
 *
 * @param This pointer to a struct
 * @param LFOFreqHz the LFO frequency in Hz
 * @param depthDb depth of cut in dB. MUST BE A NEGATIVE NUMBER.
 * @param sampleRate sample rate in Hz
 */
void BMLFOPan2_initQuadratureDb(BMLFOPan2 *This, float LFOFreqHz, float depthDb, float sampleRate);


/*!
 *BMLFOPan2_setDepthSmoothlyDb
 *
 * @abstract This changes the depth of the LFO smoothly to avoid causing a click sound
 */
void BMLFOPan2_setDepthSmoothlyDb(BMLFOPan2 *This, float depthDb);



/*!
 *BMLFOPan2_free
 */
void BMLFOPan2_free(BMLFOPan2 *This);

#endif /* BMLFOPan2_h */
