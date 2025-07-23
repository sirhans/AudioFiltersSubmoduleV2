//
//  BMLeveler.h
//  AudioFiltersXcodeProject
//
//  This is like a compressor except that it can both increase and decrease the
//  volume to approach a specified target value. The interface has the same
//  controls as the leveler component in Soundweb Designer.
//
//  Created by hans anderson on 7/8/25.
//  I hereby release this code into the public domain.
//

#ifndef BMLeveler_h
#define BMLeveler_h

#include <stdio.h>
#include "BMEnvelopeFollower.h"
//#include "BMQuadraticThreshold.h"

typedef struct BMLeveler {
	float targetOutputDb, ratio, gradient, maxGainDb, speedSeconds, thresholdDb, sampleRate;
	float *controlSignalBuffer;
	BMEnvelopeFollower envelopeFollowerL1, envelopeFollowerL2, envelopeFollowerR1, envelopeFollowerR2;
} BMLeveler;


/*!
 *BMLeveler_init
 *
 * @brief This leveler module has the same controls as the leveler object in Soundweb Designer. We do not know how similar it is to the Soundweb Designer leveler in terms of audio processing. The notes below on the function of each parameter are copied from the help file for Soundweb Designer.
 *
 * @param targetOutputDb Adjusts the level which the leveller tries to maintain. The more positive the value, the higher the output level will be.
 * @param ratio Adjusts the aggressiveness of levelling. Lower values allow more tolerance; allowing the output level to differ more from the target level, which preserves more of the original dynamics of the signal at the expense of firm control over the level. Higher ratio values cause the output level to be more tightly 'driven' towards the target level.
 * @param maxGainDb Determines how much gain the object is allowed to apply to bring a low level signal up to the target level. Higher gain values allow the leveller to add more gain when the signal level is low, but will also bring up the noise floor more.
 * @param speedSeconds Adjusts the time it takes for the leveller to recover from high levels when low levels are encountered. Higher values make the recovery slower, so there are no sudden jumps in gain. Lower (faster) values allow the leveller to track and counteract rapidly decreasing levels.
 * @param thresholdDb Determines the lowest input level that the Leveller will attempt to correct. Correct setting of this control is critical to the leveller's ability to discriminate unwanted noise from the wanted signal. Lower values will allow the leveller to 'pull-up' lower level signals, but will also make it more prone to activate on noise signals.
 * @param sampleRate audio processing sample rate
 */
void BMLeveler_init(BMLeveler *This,
					float targetOutputDb,
					float ratio,
					float maxGainDb,
					float speedSeconds,
					float thresholdDb,
					float sampleRate);




/*!
 *BMLeveler_free
 */
void BMLeveler_free(BMLeveler *This);





/*!
 *BMLeveler_processMono
 */
void BMLeveler_processMono(BMLeveler *This,
						   float *input, float *output,
						   size_t numSamples);




/*!
 *BMLeveler_processStereo
 */
void BMLeveler_processStereo(BMLeveler *This,
							 float *inL,float *inR,
							 float *outL, float *outR,
							 size_t numSamples);




#endif /* BMLeveler_h */
