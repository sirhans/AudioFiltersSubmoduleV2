//
//  BMDither.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 10/13/25.
//  I release this file into the public domain.
//

#include "../AudioFilter.h"
#include "BMDither.h"
#include "../Constants.h"
#include <math.h>

#define BMDITHER_HIGH_SHELF_ORDER 8
#define BMDITHER_HIGH_SHELF_LEVELS BMDITHER_HIGH_SHELF_ORDER / 2
#define BMDITHER_FILTER_LEVELS 2 + BMDITHER_HIGH_SHELF_LEVELS

#define BMDITHER_LOW_SHELF_1_FC 1050.0
#define BMDITHER_LOW_SHELF_1_SLOPE 0.75
#define BMDITHER_LOW_SHELF_2_FC 2100.0
#define BMDITHER_LOW_SHELF_GAIN 13.5

#define BMDITHER_HIGH_SHELF_FC 15500.0
#define BMDITHER_HIGH_SHELF_GAIN 20.75

#define BMDITHER_LOW_SHELF_1_LEVEL 0
#define BMDITHER_LOW_SHELF_2_LEVEL 1
#define BMDITHER_HIGH_SHELF_LEVEL 2

#define BMDITHER_MIDRANGE_GAIN -102.0
#define BMDITHER_SILENT_VOLUME 0.00001525878906 // 2^(-16) // if the volume is less than this, we consider it silent.


/*!
 *BMDither_init
 */
void BMDither_init(BMDither *This, DitherBitDepth depth, float sampleRate){
	// we currently only support dithering for 16 bit audio.
	assert(depth == BMDITHER_16);
	
	// init the white noise oscillator
	BMWhiteNoiseOscillator_init(&This->whiteNoise);
	
	// init filter for noise-shaping to emphasize dithering in less audible
	// frequency ranges.
	BMMultiLevelBiquad_init(&This->filter, BMDITHER_FILTER_LEVELS, sampleRate, true, false, false);
	
	
	// We combine two low shelf filters at slightly different frequencies to more
	// closely match the target spectrum
	//
	// first low shelf boost
	//BMMultiLevelBiquad_setLowShelfFirstOrder(&This->filter, BMDITHER_LOW_SHELF_1_FC, BMDITHER_LOW_SHELF_GAIN * 0.5, BMDITHER_LOW_SHELF_1_LEVEL);
	BMMultiLevelBiquad_setLowShelfAdjustableSlope(&This->filter, BMDITHER_LOW_SHELF_1_FC, BMDITHER_LOW_SHELF_GAIN * 0.925, BMDITHER_LOW_SHELF_1_SLOPE, BMDITHER_LOW_SHELF_1_LEVEL);
	// second low-shelf boost
	BMMultiLevelBiquad_setLowShelf(&This->filter, BMDITHER_LOW_SHELF_2_FC, BMDITHER_LOW_SHELF_GAIN * 0.075, BMDITHER_LOW_SHELF_2_LEVEL);
	
	// higher order high shelf boost
	BMMultiLevelBiquad_setHighShelfHighOrder(&This->filter, BMDITHER_HIGH_SHELF_FC, BMDITHER_HIGH_SHELF_GAIN, BMDITHER_HIGH_SHELF_ORDER, BMDITHER_HIGH_SHELF_LEVEL, BMDITHER_HIGH_SHELF_LEVELS);
	
	// init DC blocker. The purpose of this is to eliminate DC offset in the input
	// before we measure the volume so that silent signals with DC offset actually
	// register as silence.
	BMMultiLevelSVF_init(&This->dcBlocker, 1, sampleRate, true);
	BMMultiLevelSVF_setHighpass12dB(&This->dcBlocker, 300.0, 0);
	
	// set dithering noise gain
	This->noiseGain = BM_DB_TO_GAIN(BMDITHER_MIDRANGE_GAIN);
	
	// init the on-off switch to turn off the dithering when the input is silent
	BMSmoothSwitch_init(&This->smoothSwitch, sampleRate);
	BMSmoothSwitch_setState(&This->smoothSwitch, true);
	
	// allocate buffers
	This->bufferL = malloc(sizeof(float) * BM_BUFFER_CHUNK_SIZE);
	This->bufferR = malloc(sizeof(float) * BM_BUFFER_CHUNK_SIZE);
}


/*!
 *BMDither_processStereo
 */
void BMDither_processStereo(BMDither *This, float *inL, float *inR, float *outL, float *outR, size_t numSamples){
	
	// chunked processing
	size_t samplesProcessed = 0;
	while(samplesProcessed < numSamples) {
		size_t samplesProcessing = BM_MIN(numSamples - samplesProcessed, BM_BUFFER_CHUNK_SIZE);
		
		// filter the input to remove DC offset. this will not affect the output
		// because we will overwrite these buffers after we use them to check
		// the input volume.
		BMMultiLevelSVF_processBufferStereo(&This->dcBlocker, inL + samplesProcessed, inR + samplesProcessed, This->bufferL, This->bufferR, samplesProcessing);
		
		// is the input silent?
		//
		// square every sample and take the sum for L and R channels
		float sumOfSquaresL, sumOfSquaresR;
		vDSP_svesq(This->bufferL, 1, &sumOfSquaresL, samplesProcessing);
		vDSP_svesq(This->bufferR, 1, &sumOfSquaresR, samplesProcessing);
		// use the sum of squares to estimate the max volume in both channels
		float volume = BM_MAX(sumOfSquaresL,sumOfSquaresR) / samplesProcessing;
		volume = sqrtf(volume);
		// if the volume is less than a threshold level, we'll call it silent.
		bool silent = false;
		if (volume < BMDITHER_SILENT_VOLUME) silent = true;
		
		
		// generate white noise at 0 dB volume into L and R channel buffers
		BMWhiteNoiseOscillator_process(&This->whiteNoise, This->bufferL, samplesProcessing);
		BMWhiteNoiseOscillator_process(&This->whiteNoise, This->bufferR, samplesProcessing);
		
		// filter the noise to increase the volume in less audible frequency ranges
		BMMultiLevelBiquad_processBufferStereo(&This->filter, This->bufferL, This->bufferR, This->bufferL, This->bufferR, samplesProcessing);
		
		// attenuate the gain of the dithering noise
		vDSP_vsmul(This->bufferL, 1, &This->noiseGain, This->bufferL, 1, samplesProcessing);
		vDSP_vsmul(This->bufferR, 1, &This->noiseGain, This->bufferR, 1, samplesProcessing);
		
		// stop dithering if the input is silent
		BMSmoothSwitch_setState(&This->smoothSwitch, !silent);
		BMSmoothSwitch_processBufferStereo(&This->smoothSwitch, This->bufferL, This->bufferR, This->bufferL, This->bufferR, samplesProcessing);
		
		// mix the dithering noise with the audio input and write to output
		vDSP_vadd(inL + samplesProcessed, 1, This->bufferL, 1, outL + samplesProcessed, 1, samplesProcessing);
		vDSP_vadd(inR + samplesProcessed, 1, This->bufferR, 1, outR + samplesProcessed, 1, samplesProcessing);
		
		samplesProcessed += samplesProcessing;
	}
}


/*!
 *BMDither_free
 */
void BMDither_free(BMDither *This){
	BMMultiLevelBiquad_free(&This->filter);
	free(This->bufferL);
	free(This->bufferR);
	This->bufferL = NULL;
	This->bufferR = NULL;
}
