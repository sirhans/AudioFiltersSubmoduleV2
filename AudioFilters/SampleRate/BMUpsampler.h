//
//  BMUpsampler.h
//  BMAudioFilters
//
//  Created by Hans on 10/9/17.
//  Anyone may use this file without restrictions of any kind.
//

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BMUpsampler_h
#define BMUpsampler_h


#include <MacTypes.h>
#include "BMIIRUpsampler2x.h"
#include "BMMultiLevelBiquad.h"

#define BM_UPSAMPLER_STAGE0_TRANSITION_BANDWIDTH_FULL_SPECTRUM 0.05
#define BM_UPSAMPLER_STAGE0_TRANSITION_BANDWIDTH_96KHZ_INPUT 0.1
#define BM_UPSAMPLER_STAGE0_TRANSITION_BANDWIDTH_GUITAR 0.1
#define BM_UPSAMPLER_STOPBAND_ATTENUATION_DB 110.0
#define BM_UPSAMPLER_SECOND_STAGE_AA_FILTER_NUMLEVELS_FULL_SPECTRUM 4
#define BM_UPSAMPLER_SECOND_STAGE_AA_FILTER_NUMLEVELS_96KHZ_INPUT 2
#define BM_UPSAMPLER_SECOND_STAGE_AA_FILTER_BW_FULL_SPECTRUM 0.177
#define BM_UPSAMPLER_SECOND_STAGE_AA_FILTER_BW_96KHZ_INPUT 0.45
// GUITAR: for guitar amp signal paths we are willing to give up some response
// near Nyquist in exchange for cheaper resampling (so we can oversample
// further). The second-stage AA (anti-ringing) filter therefore follows the
// 96 kHz-input setting: cutoff at (1 - 0.45) = 55% of Nyquist (13.2 kHz for a
// 48 kHz output, 12.1 kHz at 44.1 kHz), 4th order. It is 0.5 dB down at
// 10 kHz, 34 dB down at Nyquist, and rings for less than half as long as the
// 8th-order full-spectrum filter. The half-band stages use the 96 kHz-input
// transition bandwidth.
#define BM_UPSAMPLER_SECOND_STAGE_AA_FILTER_NUMLEVELS_GUITAR BM_UPSAMPLER_SECOND_STAGE_AA_FILTER_NUMLEVELS_96KHZ_INPUT
#define BM_UPSAMPLER_SECOND_STAGE_AA_FILTER_BW_GUITAR BM_UPSAMPLER_SECOND_STAGE_AA_FILTER_BW_96KHZ_INPUT

enum resamplerType {BMRESAMPLER_FULL_SPECTRUM, BMRESAMPLER_GUITAR, BMRESAMPLER_INPUT_96KHZ, BMRESAMPLER_FULL_SPECTRUM_NO_STAGE2_FILTER};

typedef struct BMUpsampler {
	BMIIRUpsampler2x* upsamplers2x;
	BMMultiLevelBiquad secondStageAAFilter;
	float *bufferL, *bufferR;
	size_t numStages, upsampleFactor;
	bool useSecondStageFilter;
} BMUpsampler;


/*!
 *BMUpsampler_init
 *
 * @param This   pointer to an upsampler struct
 * @param stereo set true if you need stereo processing; false for mono
 * @param upsampleFactor supported values: 2^n
 * @param type   BM_RESAMPLER_FULL_SPECTRUM | BM_RESAMPLER_GUITAR | BM_RESAMPLER_96KHZ_INPUT
 */
void BMUpsampler_init(BMUpsampler* This, bool stereo, size_t upsampleFactor, enum resamplerType type);



/*!
 *BMUpsampler_processBufferMono
 * @abstract upsample a buffer that contains a single channel of audio samples
 *
 * @param input    length = numSamplesIn
 * @param output   length = numSamplesIn * upsampleFactor
 * @param numSamplesIn  number of input samples to process
 */
void BMUpsampler_processBufferMono(BMUpsampler *This, const float* input, float* output, size_t numSamplesIn);


/*!
 *BMUpsampler_processBufferStereo
 * @abstract upsample a buffer that contains a single channel of audio samples
 *
 * @param inputL    length = numSamplesIn
 * @param inputR    length = numSamplesIn
 * @param outputL   length = numSamplesIn * upsampleFactor
 * @param outputR   length = numSamplesIn * upsampleFactor
 * @param numSamplesIn  number of input samples to process
 */
void BMUpsampler_processBufferStereo(BMUpsampler *This, const float* inputL, const float* inputR, float* outputL, float* outputR, size_t numSamplesIn);


void BMUpsampler_free(BMUpsampler *This);


/*!
 *BMUpsampler_impulseResponse
 * @param IR array for IR output with length = IRLength
 * @param IRLength must be divisible by upsampleFactor
 */
void BMUpsampler_impulseResponse(BMUpsampler *This, float* IR, size_t IRLength);



/*!
 *BMUpsampler_getLatencyInSamples
 *
 * @returns the latency of the upsampler at 300 hz. Since latency is the product of filter group delay, it is frequency dependent.
 */
float BMUpsampler_getLatencyInSamples(BMUpsampler *This);


#endif /* BMUpsampler_h */

#ifdef __cplusplus
}
#endif
