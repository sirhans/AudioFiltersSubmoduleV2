//
//  BMSaturator2.c
//  BMAUSaturator
//
//  Created by hans anderson on 7/5/19.
//  Copyright © 2019 bluemangoo. All rights reserved.
//

#include "BMSaturator2.h"
#include "../Constants.h"
#include <assert.h>
#include "BMAsymptoticLimiter.h"
#include <sys/param.h>
#include <string.h>
#include <math.h>


// preamp EQ settings
#define SAT2_PRE_EQ_FC_1 100.0f
#define SAT2_PRE_EQ_FC_2 450.0f
#define SAT2_PRE_EQ_FC_3 750.0f
#define SAT2_PRE_EQ_FC_4 2100.0f
#define SAT2_PRE_EQ_FC_5 6500.0f

#define SAT2_PRE_EQ_Q_1 1.0f
#define SAT2_PRE_EQ_Q_2 1.0f
#define SAT2_PRE_EQ_Q_3 1.0f
#define SAT2_PRE_EQ_Q_4 1.0f
#define SAT2_PRE_EQ_Q_5 1.0f

#define SAT2_PRE_EQ_ONE_SLOPE 0.5f
#define SAT2_PRE_EQ_FIVE_SLOPE 0.5f

#define SAT2_PRE_EQ_ONE_IS_SHELF false
#define SAT2_PRE_EQ_FIVE_IS_SHELF true

// post-amp EQ settings
#define SAT2_POST_EQ_FC_1 100.0f
#define SAT2_POST_EQ_FC_2 450.0f
#define SAT2_POST_EQ_FC_3 750.0f
#define SAT2_POST_EQ_FC_4 2100.0f
#define SAT2_POST_EQ_FC_5 6500.0f

#define SAT2_POST_EQ_Q_1 1.0f
#define SAT2_POST_EQ_Q_2 1.0f
#define SAT2_POST_EQ_Q_3 1.0f
#define SAT2_POST_EQ_Q_4 1.0f
#define SAT2_POST_EQ_Q_5 1.0f

#define SAT2_POST_EQ_ONE_SLOPE 0.5f
#define SAT2_POST_EQ_FIVE_SLOPE 0.5f

#define SAT2_POST_EQ_ONE_IS_SHELF false
#define SAT2_POST_EQ_FIVE_IS_SHELF true

// reverb settings
#define SAT2_REVERB_TIME_DV 0.30f
#define SAT2_REVERB_HF_DECAY_MULTIPLIER_DV 4.0f
#define SAT2_REVERB_WET_MIX_DV 0.40f
#define SAT2_REVERB_MIN_DELAY_DV 40.0f/48000.0f
#define SAT2_REVERB_MAX_DELAY_DV 0.020f
#define SAT2_REVERB_LOWPASS_FC_DV 5500.0f

// stereo cabinet settings
#define SAT2_STEREO_CABINET_WET_MIX_DB_DV -0.1f

// standard amp sag settings
#define SAT2_PREAMP_POWER_LIMIT_DB_STANDARD -45.0f
#define SAT2_PREAMP_SAG_STANDARD 1.0f / 4000.0f
#define SAT2_POWERAMP_POWER_LIMIT_DB_STANDARD SAT2_PREAMP_POWER_LIMIT_DB_STANDARD
#define SAT2_POWERAMP_SAG_STANDARD SAT2_PREAMP_SAG_STANDARD


// forward declarations
void BMGainstage_init(BMGainstage *This, float sampleRate, bool isStereo);
void BMGainstage_free(BMGainstage *This);
void BMSaturator2_setAmpType(BMSaturator2 *This, Sat2AmpType type);
void BMGainstage_setAAFilterFc(BMGainstage *This, float fc);
void BMGainstage_updateLimiterFilterMirror(BMGainstage *This);
void BMGainstage_setLimiterAAFilterFc(BMGainstage *This, float fc);
void BMGainstage_setMirrorLimiterFilters(BMGainstage *This, bool on);
void BMGainstage_setAAFilterFcRealtime(BMGainstage *This, float fc);
void BMGainstage_setGroupDelayScale(BMGainstage *This, float scale);
// Defined in BMHysteresisLimiter.c; its declaration is commented out of the
// header because the limiter's cutoff is not meant to be changed casually.
// The gain stage only calls it through BMGainstage_setLimiterAAFilterFc,
// which keeps the mirror filter on the lowpassed path in step.
void BMHysteresisLimiter_setAAFilterFC(BMHysteresisLimiter *This, float fc);
// Defined in BMMultiLevelBiquad.c without a header declaration: marks the
// coefficients as changed so the process functions push them to vDSP.
void BMMultiLevelBiquad_queueUpdate(BMMultiLevelBiquad *This);
void BMSaturator2_initReconfigurableFilters(BMSaturator2 *This);
void BMSaturator2_freeReconfigurableFilters(BMSaturator2 *This);
void BMSaturator2_resetEQConfig(BMSaturator2 *This);
void BMSaturator2_applyAmpType(BMSaturator2 *This);
void BMSaturator2_applyEQConfig(BMSaturator2 *This);
void BMSaturator2_setStereoCabWetMixDb(BMSaturator2 *This, float wetMixDb);
void BMSaturator2_applySideBandEarlyReflections(BMSaturator2 *This, enum BMMonoToStereoSideBand band);




void BMSaturator2_init(BMSaturator2 *This, float sampleRate, size_t oversampleFactor, bool isStereo){
    This->initCompleted = false;
    This->sampleRate = sampleRate;
    This->isStereo = This->requestedStereo = isStereo;
    
    // set the oversampling factor
    assert(powerof2(oversampleFactor));
    This->oversamplingFactor = This->requestedOversamplingFactor = oversampleFactor;
    
    // use a stereo speaker cabinet by default
    This->useStereoCab = true;
    
    // mirror the limiters' anti-aliasing filters on the lowpassed path by default
    This->useLimiterFilterMirror = true;
    This->aaFilterFcSetting = BM_SAT2_AA_FILTER_DEFAULT_FC;
    This->groupDelayScaleSetting = BM_SAT2_GROUP_DELAY_COMPENSATION_SCALE;
    
    // gain stage in; the crossfade step is set per oversampling rate in
    // BMSaturator2_initReconfigurableFilters. The external tone filters are
    // intentionally not touched here (see BMSaturator2_setExternalToneFilters).
    This->gainStageBypass = false;
    This->gainStageMix = 1.0f;
    This->gainStageBypassGain = 1.0f;
    
    // init the gain controllers
    BMSmoothGain_init(&This->inputGain, sampleRate);
    BMSmoothGain_init(&This->outputGain, sampleRate);
    
    // set EQ and gain controls to zero
    BMSaturator2_resetEQConfig(This);
    
    // init the default amp tone settings
    BMSaturator2_setAmpType(This, amp_blues1);
	
	// set the default wet mix for the stereo cabinet
	This->stereoCabWetMixDb = SAT2_STEREO_CABINET_WET_MIX_DB_DV;
    
	// early reflections: the decorrelator's own defaults until told otherwise.
	// (Before the filters' init below, which reapplies a custom model if these say there is one.)
	This->earlyReflectionsCustom = false;
	This->erPending = false;
	This->erConvolved = false;
	BMLock_init(&This->erConvolutionLock);
	for(size_t b=0; b<2; b++){
		This->erSide[b].custom = false;
		This->erSide[b].pending = false;
		This->erSide[b].bypassed = true;
		This->erSide[b].seed = 0;
	}
	
    // init the filters that can be reconfigured during runtime
    BMSaturator2_initReconfigurableFilters(This);
	
    // init reverb
#if BM_SAT2_INCLUDE_REVERB
    BMReverbInit(&This->reverb, sampleRate);
	BMSaturator2_setRoomType(This, s2room_small);
#else
    This->useReverb = false;
#endif
    
    // init the switch that prevents the app from clicking when it changes oversampling rate
    BMSmoothSwitch_init(&This->smoothSwitch, sampleRate);
	
	// init the peak limiter
	BMPeakLimiter_init(&This->peakLimiter, true, sampleRate);
	BMSaturator2_setPeakLimiter(This, true);

}





void BMSaturator2_initReconfigurableFilters(BMSaturator2 *This){

    // determine the resampler type appropriate for the input sample rate
    enum resamplerType rsType = BMRESAMPLER_FULL_SPECTRUM;
    if(This->sampleRate > 50000.0)
        rsType = BMRESAMPLER_INPUT_96KHZ;
    
    
    // initialize the oversampling filters
    if(This->requestedOversamplingFactor > 1){
        BMUpsampler_init(&This->upsampler,
                         This->requestedStereo,
                         This->requestedOversamplingFactor,
                         rsType);
        BMDownsampler_init(&This->downsampler,
                           This->requestedStereo,
                           This->requestedOversamplingFactor,
                           rsType);
    }
    
    // init the buffers
    //
    // we allocate all the buffers as a contiguous region in memory so
    // that we can process stateless functions on all of them with a single
    // function call.
    This->buffers[0]  = malloc(sizeof(float)*BM_BUFFER_CHUNK_SIZE*This->requestedOversamplingFactor*BM_SAT2_NUM_BUFFERS);
    for(size_t i=1; i<BM_SAT2_NUM_BUFFERS; i++)
        This->buffers[i] = This->buffers[0] + (i * BM_BUFFER_CHUNK_SIZE*This->requestedOversamplingFactor);
    This->dbuffers[0] = malloc(sizeof(double)*BM_BUFFER_CHUNK_SIZE*This->requestedOversamplingFactor*BM_SAT2_NUM_BUFFERS);
    for(size_t i=1; i<BM_SAT2_NUM_BUFFERS; i++)
        This->dbuffers[i] = This->dbuffers[0] + (i * BM_BUFFER_CHUNK_SIZE*This->requestedOversamplingFactor);
    
    // init EQ filters
    BMMultiLevelBiquad_init(&This->preEQ, BM_SAT2_EQ_NUM_LEVELS + BM_SAT2_PRE_AMP_FILTER_NUM_LEVELS, This->sampleRate, This->requestedStereo, true, false);
    BMMultiLevelBiquad_init(&This->postEQ, BM_SAT2_EQ_NUM_LEVELS + BM_SAT2_POST_AMP_FILTER_NUM_LEVELS, This->sampleRate, This->requestedStereo, true, false);
#if BM_SAT2_TONE_SVF
    BMMultiLevelSVF_init(&This->preTone, BM_SAT2_PRE_AMP_FILTER_NUM_LEVELS, This->sampleRate, This->requestedStereo);
    BMMultiLevelSVF_init(&This->postTone, BM_SAT2_POST_AMP_FILTER_NUM_LEVELS, This->sampleRate, This->requestedStereo);
#endif
    
    // apply the current EQ filters settings to the EQ filters
    BMSaturator2_applyEQConfig(This);
#if !BM_SAT2_INCLUDE_EQ
    BMSaturator2_setEQActive(This, false);
#endif
    
    // init the Gainstage
    BMGainstage_init(&This->gainStage, This->sampleRate*This->requestedOversamplingFactor, This->requestedStereo);
    // gain stage bypass crossfade: 20 ms at the oversampled rate
    This->gainStageMixStep = 1.0f / (0.020f * This->sampleRate * (float)This->requestedOversamplingFactor);
    This->gainStageMix = This->gainStageBypass ? 0.0f : 1.0f;
    BMGainstage_setMirrorLimiterFilters(&This->gainStage, This->useLimiterFilterMirror);
    This->gainStage.groupDelayScale = This->groupDelayScaleSetting;
    if(This->aaFilterFcSetting != This->gainStage.aaFilterFc || This->groupDelayScaleSetting != BM_SAT2_GROUP_DELAY_COMPENSATION_SCALE)
        BMGainstage_setAAFilterFc(&This->gainStage, This->aaFilterFcSetting);
	
    // init the stereo decorrelator, and reapply the early-reflection model if one was set
    BMMonoToStereo_init(&This->monoToStereo, This->sampleRate, This->requestedStereo);
    BMMonoToStereo_setBypass(&This->monoToStereo, !This->useStereoCab, false);
    if(This->earlyReflectionsCustom)
        BMMonoToStereo_setEarlyReflections(&This->monoToStereo, This->erMinDelayS, This->erMaxDelayS, This->erNumWetTaps,
                                           This->erRT60S, This->erDryGain, This->erWetTapGain, This->erBassCrossoverHz, This->erTrebleCrossoverHz);
    for(size_t b=0; b<2; b++)
        if(This->earlyReflectionsCustom && This->erSide[b].custom)
            BMSaturator2_applySideBandEarlyReflections(This, (enum BMMonoToStereoSideBand)b);
    if(This->earlyReflectionsCustom && This->erConvolved && !This->requestedStereo)
        BMMonoToStereo_bakeConvolution(&This->monoToStereo);
	
    // apply the current amp type to the amp tone filters
    BMSaturator2_applyAmpType(This);
    
    // mark the changes done
    This->isStereo = This->requestedStereo;
    This->oversamplingFactor = This->requestedOversamplingFactor;
    This->initCompleted = true;
}






/*!
 *BMSaturator2_freeReconfigurableFilters
 *
 * @abstract call this to free filter memory before switching between stereo and mono or changing oversampling rate
 */
void BMSaturator2_freeReconfigurableFilters(BMSaturator2 *This){
    This->initCompleted = false;
    
    if(This->oversamplingFactor > 1){
        BMUpsampler_free(&This->upsampler);
        BMDownsampler_free(&This->downsampler);
    }
    
    BMMultiLevelBiquad_free(&This->preEQ);
    BMMultiLevelBiquad_free(&This->postEQ);
#if BM_SAT2_TONE_SVF
    BMMultiLevelSVF_free(&This->preTone);
    BMMultiLevelSVF_free(&This->postTone);
#endif
    
    BMGainstage_free(&This->gainStage);
    
    // free the buffers
    // we allocated them all in a contiguous region starting at b1 so only
    // one call to free is needed
    free(This->buffers[0]);
    This->buffers[0] = NULL;
    free(This->dbuffers[0]);
    This->dbuffers[0] = NULL;
	
	BMMonoToStereo_free(&This->monoToStereo);
}




/*!
 *BMSaturator2_applyEQConfig
 *
 * @abstract this applies the EQ config and state saved in preEQConfig, preEQState, postEQConfig, and postEQState
 */
void BMSaturator2_applyEQConfig(BMSaturator2 *This){
    
    // pre EQ
    BMSmoothGain_setGainDbInstant(&This->inputGain, This->preEQState.gain);
	for(size_t i=0; i<5; i++){
		BMMultiLevelBiquad_setBellQ(&This->preEQ,
									This->preEQConfig.fc[i],
									This->preEQConfig.Q[i],
									This->preEQState.eqGain[i],
									i);
	}
	if(This->preEQConfig.oneIsShelf)
		BMMultiLevelBiquad_setLowShelfAdjustableSlope(&This->preEQ,
													  This->preEQConfig.fc[0],
													  This->preEQState.eqGain[0],
													  This->preEQConfig.oneSlope,
													  0);
	if(This->preEQConfig.fiveIsShelf)
	BMMultiLevelBiquad_setLowShelfAdjustableSlope(&This->preEQ,
												  This->preEQConfig.fc[4],
												  This->preEQState.eqGain[4],
												  This->preEQConfig.fiveSlope,
												  4);
	
	// post EQ
    BMSmoothGain_setGainDbInstant(&This->inputGain, This->postEQState.gain);
	for(size_t i=0; i<5; i++){
		BMMultiLevelBiquad_setBellQ(&This->postEQ,
									This->postEQConfig.fc[i],
									This->postEQConfig.Q[i],
									This->postEQState.eqGain[i],
									i);
	}
	if(This->postEQConfig.oneIsShelf)
		BMMultiLevelBiquad_setLowShelfAdjustableSlope(&This->postEQ,
													  This->postEQConfig.fc[0],
													  This->postEQState.eqGain[0],
													  This->postEQConfig.oneSlope,
													  0);
	if(This->postEQConfig.fiveIsShelf)
	BMMultiLevelBiquad_setLowShelfAdjustableSlope(&This->postEQ,
												  This->postEQConfig.fc[4],
												  This->postEQState.eqGain[4],
												  This->postEQConfig.fiveSlope,
												  4);
}





void BMSaturator2_resetEQConfig(BMSaturator2 *This){
	// The shelf flags and slopes were never written anywhere (latent since the
	// original plugin): with a fresh, zeroed allocation the flags read false
	// and nothing happened; on reused memory they read garbage and
	// BMSaturator2_applyEQConfig hit the slope assertion in
	// BMMultiLevelBiquad_setLowShelfAdjustableSlope. Bands 1 and 5 are bells
	// (the not-shelf branch), as they always effectively were.
	This->preEQConfig.oneIsShelf = This->preEQConfig.fiveIsShelf = false;
	This->postEQConfig.oneIsShelf = This->postEQConfig.fiveIsShelf = false;
	This->preEQConfig.oneSlope = This->preEQConfig.fiveSlope = 1.0f;
	This->postEQConfig.oneSlope = This->postEQConfig.fiveSlope = 1.0f;
	
	
	This->preEQConfig.fc[0] = SAT2_PRE_EQ_FC_1;
	This->preEQConfig.fc[1] = SAT2_PRE_EQ_FC_2;
	This->preEQConfig.fc[2] = SAT2_PRE_EQ_FC_3;
	This->preEQConfig.fc[3] = SAT2_PRE_EQ_FC_4;
	This->preEQConfig.fc[4] = SAT2_PRE_EQ_FC_5;
	
	This->preEQConfig.Q[0] = SAT2_PRE_EQ_Q_1;
	This->preEQConfig.Q[1] = SAT2_PRE_EQ_Q_2;
	This->preEQConfig.Q[2] = SAT2_PRE_EQ_Q_3;
	This->preEQConfig.Q[3] = SAT2_PRE_EQ_Q_4;
	This->preEQConfig.Q[4] = SAT2_PRE_EQ_Q_5;
	
	if(SAT2_PRE_EQ_ONE_IS_SHELF)
		This->preEQConfig.Q[0] = SAT2_PRE_EQ_ONE_SLOPE;
	if(SAT2_PRE_EQ_FIVE_IS_SHELF)
		This->preEQConfig.Q[4] = SAT2_PRE_EQ_FIVE_SLOPE;
	
	This->preEQState.gain = 0.0f;
	for(size_t i=0; i<5; i++)
		This->preEQState.eqGain[i] = 0.0f;
	
	This->postEQConfig.fc[0] = SAT2_POST_EQ_FC_1;
	This->postEQConfig.fc[1] = SAT2_POST_EQ_FC_2;
	This->postEQConfig.fc[2] = SAT2_POST_EQ_FC_3;
	This->postEQConfig.fc[3] = SAT2_POST_EQ_FC_4;
	This->postEQConfig.fc[4] = SAT2_POST_EQ_FC_5;
	
	This->postEQConfig.Q[0] = SAT2_POST_EQ_Q_1;
	This->postEQConfig.Q[1] = SAT2_POST_EQ_Q_2;
	This->postEQConfig.Q[2] = SAT2_POST_EQ_Q_3;
	This->postEQConfig.Q[3] = SAT2_POST_EQ_Q_4;
	This->postEQConfig.Q[4] = SAT2_POST_EQ_Q_5;
	
	This->postEQState.gain = 0.0f;
	for(size_t i=0; i<5; i++)
		This->postEQState.eqGain[i] = 0.0f;
	
	if(SAT2_POST_EQ_ONE_IS_SHELF)
		This->postEQConfig.Q[0] = SAT2_POST_EQ_ONE_SLOPE;
	if(SAT2_POST_EQ_FIVE_IS_SHELF)
		This->postEQConfig.Q[4] = SAT2_POST_EQ_FIVE_SLOPE;
}





void BMGainstage_init(BMGainstage *This, float sampleRate, bool isStereo){
    
    // init the rectifier
    float kneeWidth = 3.0f;
    BMQuadraticRectifier_init(&This->rectifier, kneeWidth);
    
    // init 2 delays for mono; 4 for stereo
    size_t numChannels = isStereo ? 4 : 2;
    BMShortSimpleDelay_init(&This->delay, numChannels, 10);
    
    // init the 6 dB AA filters: one 2-channel SVF per (pos, neg) pair in mono,
    // two in stereo ((posL, posR) and (negL, negR))
    This->numAAFilterPairs = isStereo ? 2 : 1;
    This->groupDelayScale = BM_SAT2_GROUP_DELAY_COMPENSATION_SCALE;
    BMMultiLevelBiquad_init(&This->aaFilterGroupDelayShadow, 1, sampleRate, false, false, false);
    for(size_t i = 0; i < This->numAAFilterPairs; i++){
        BMMultiLevelSVF_init(&This->AAFilter1[i],   1, sampleRate, true);
        BMMultiLevelSVF_init(&This->AAFilter2_1[i], 1, sampleRate, true);
        BMMultiLevelSVF_init(&This->AAFilter2_2[i], 1, sampleRate, true);
    }
    
    // init the hysteresis limiter that models sag from the power supply transformers
	float aaFilterFc = (sampleRate / 2.0f > 20000.0f) ? 20000.0f : sampleRate * 0.4f;
	BMHysteresisLimiter_init(&This->preAmpLimiter, sampleRate, 1, aaFilterFc, numChannels);
	BMHysteresisLimiter_init(&This->powerAmpLimiter, sampleRate, 1, aaFilterFc, numChannels);
	
	// engage the power amp limiter
	This->usePowerAmpLimiter = true;
    
    // mirror of the limiters' anti-aliasing filters for the lowpassed path:
    // one level per limiter section, same channel layout as the limiters'
    // filters (2 channels mono, 4 stereo). The coefficients are copied from
    // the limiters by BMGainstage_updateLimiterFilterMirror, which
    // BMGainstage_setAAFilterFc calls below.
    size_t mirrorLevels = This->preAmpLimiter.AAFilter.numLevels + This->powerAmpLimiter.AAFilter.numLevels;
    if(isStereo)
        BMMultiLevelBiquad_init4(&This->limiterFilterMirror, mirrorLevels, sampleRate, false);
    else
        BMMultiLevelBiquad_init(&This->limiterFilterMirror, mirrorLevels, sampleRate, true, true, false);
    This->limiterFilterMirrorD = NULL;
    This->mirrorLimiterFilters = true;
    
    BMGainstage_setAAFilterFc(This, BM_SAT2_AA_FILTER_DEFAULT_FC);
}






#if BM_SAT2_DOUBLE_GAINSTAGE

/*
 * Double-precision gain stage. Same structure as the float version below:
 * rectify (float, memoryless) -> anti-aliasing lowpass -> preamp hysteresis
 * limiter -> anti-aliasing lowpasses -> power amp hysteresis limiter -> bias
 * -> control signal = waveshaped / lowpassed -> multiply by the delayed
 * rectified input -> sum. Everything after the rectifier is in double; the
 * delay of the rectified input stays in float (memoryless precision is
 * enough there, see the measurements in Midi Tuning Synth's README).
 */
void BMGainstage_processMono(BMGainstage *This,
                             BMSaturator2 *saturator,
                             float *inL,
                             float *outL,
                             size_t numSamples){
    // rectify the input into positive and negative sides (float)
    float *rectPosL = saturator->buffers[0];
    float *rectNegL = saturator->buffers[2];
    BMQuadraticRectifier_processBufferMonoVDSP(&This->rectifier, inL, rectPosL, rectNegL, numSamples);
    
    // lowpass filter the rectified sides (double)
    double *lpPosL = saturator->dbuffers[4];
    double *lpNegL = saturator->dbuffers[6];
    vDSP_vspdp(rectPosL, 1, lpPosL, 1, numSamples);
    vDSP_vspdp(rectNegL, 1, lpNegL, 1, numSamples);
    const double *lp2in [2] = {lpPosL, lpNegL};
    double *lp2out [2] = {lpPosL, lpNegL};
    BMMultiLevelSVF_processBufferStereoD(&This->AAFilter1[0], lpPosL, lpNegL, lpPosL, lpNegL, numSamples);
    
    // preamp stage
    double *wsPosL = saturator->dbuffers[8];
    double *wsNegL = saturator->dbuffers[10];
    BMHysteresisLimiter_processMonoRectifiedD(&This->preAmpLimiter, lpPosL, lpNegL, wsPosL, wsNegL, numSamples);
    
    // second-stage anti-aliasing on both the waveshaped and the lowpassed signals
    BMMultiLevelSVF_processBufferStereoD(&This->AAFilter2_1[0], wsPosL, wsNegL, wsPosL, wsNegL, numSamples);
    BMMultiLevelSVF_processBufferStereoD(&This->AAFilter2_2[0], lpPosL, lpNegL, lpPosL, lpNegL, numSamples);
    
    // mirror of the limiters' internal anti-aliasing filters on the lowpassed
    // path (see BMGainstage_updateLimiterFilterMirror). The filter always runs,
    // into scratch buffers, so that its state is current whenever the switch
    // is turned on; the switch only selects which signal reaches the division.
    {
        double *mirPosL = saturator->dbuffers[12];
        double *mirNegL = saturator->dbuffers[14];
        double *mir2out [2] = {mirPosL, mirNegL};
        vDSP_biquadmD(This->limiterFilterMirrorD, lp2in, 1, mir2out, 1, numSamples);
        if(This->mirrorLimiterFilters){
            memcpy(lpPosL, mirPosL, sizeof(double) * numSamples);
            memcpy(lpNegL, mirNegL, sizeof(double) * numSamples);
        }
    }
    
    // power amp stage
    BMHysteresisLimiter_processMonoRectifiedD(&This->powerAmpLimiter, wsPosL, wsNegL, wsPosL, wsNegL, numSamples);
    
    // bias the signals to prevent divide by zero
    float bias = 0.01f, negBias = -bias;
    double biasD = 0.01, negBiasD = -biasD;
    vDSP_vsadd(rectPosL, 1, &bias, rectPosL, 1, numSamples);
    vDSP_vsadd(rectNegL, 1, &negBias, rectNegL, 1, numSamples);
    vDSP_vsaddD(lpPosL, 1, &biasD, lpPosL, 1, numSamples);
    vDSP_vsaddD(wsPosL, 1, &biasD, wsPosL, 1, numSamples);
    vDSP_vsaddD(lpNegL, 1, &negBiasD, lpNegL, 1, numSamples);
    vDSP_vsaddD(wsNegL, 1, &negBiasD, wsNegL, 1, numSamples);
    
    // control signals: waveshaped / lowpassed (vDSP_vdivD computes B / A)
    double *csPosL = lpPosL;
    double *csNegL = lpNegL;
    vDSP_vdivD(lpPosL, 1, wsPosL, 1, csPosL, 1, numSamples);
    vDSP_vdivD(lpNegL, 1, wsNegL, 1, csNegL, 1, numSamples);
    
    // delay the (biased) rectified input to compensate for the filter group delay
    float *delayedPosL = saturator->buffers[8];
    float *delayedNegL = saturator->buffers[10];
    float *rectPtrs [2] = {rectPosL, rectNegL};
    float *delayedOutputs [2] = {delayedPosL, delayedNegL};
    BMShortSimpleDelay_process(&This->delay, (const float**)rectPtrs, delayedOutputs, 2, numSamples);
    vDSP_vspdp(delayedPosL, 1, wsPosL, 1, numSamples);
    vDSP_vspdp(delayedNegL, 1, wsNegL, 1, numSamples);
    
    // apply the control signals and sum the two sides
    vDSP_vmulD(csPosL, 1, wsPosL, 1, csPosL, 1, numSamples);
    vDSP_vmulD(csNegL, 1, wsNegL, 1, csNegL, 1, numSamples);
    vDSP_vaddD(csPosL, 1, csNegL, 1, csPosL, 1, numSamples);
    vDSP_vdpsp(csPosL, 1, outL, 1, numSamples);
}



void BMGainstage_processStereo(BMGainstage *This,
                               BMSaturator2 *saturator,
                               float *inL, float *inR,
                               float *outL, float *outR,
                               size_t numSamples){
    // rectify (float)
    float *rectPosL = saturator->buffers[0];
    float *rectPosR = saturator->buffers[1];
    float *rectNegL = saturator->buffers[2];
    float *rectNegR = saturator->buffers[3];
    BMQuadraticRectifier_processBufferStereoVDSP(&This->rectifier, inL, inR, rectPosL, rectNegL, rectPosR, rectNegR, numSamples);
    
    // lowpass (double); channel order matches the float version's processBuffer4 calls
    double *lpPosL = saturator->dbuffers[4];
    double *lpPosR = saturator->dbuffers[5];
    double *lpNegL = saturator->dbuffers[6];
    double *lpNegR = saturator->dbuffers[7];
    vDSP_vspdp(rectPosL, 1, lpPosL, 1, numSamples);
    vDSP_vspdp(rectPosR, 1, lpPosR, 1, numSamples);
    vDSP_vspdp(rectNegL, 1, lpNegL, 1, numSamples);
    vDSP_vspdp(rectNegR, 1, lpNegR, 1, numSamples);
    const double *lp4in [4] = {lpPosL, lpPosR, lpNegL, lpNegR};
    double *lp4out [4] = {lpPosL, lpPosR, lpNegL, lpNegR};
    BMMultiLevelSVF_processBufferStereoD(&This->AAFilter1[0], lpPosL, lpPosR, lpPosL, lpPosR, numSamples);
    BMMultiLevelSVF_processBufferStereoD(&This->AAFilter1[1], lpNegL, lpNegR, lpNegL, lpNegR, numSamples);
    
    // preamp stage
    double *wsPosL = saturator->dbuffers[8];
    double *wsPosR = saturator->dbuffers[9];
    double *wsNegL = saturator->dbuffers[10];
    double *wsNegR = saturator->dbuffers[11];
    BMHysteresisLimiter_processStereoRectifiedD(&This->preAmpLimiter,
                                                lpPosL, lpPosR, lpNegL, lpNegR,
                                                wsPosL, wsPosR, wsNegL, wsNegR, numSamples);
    
    if(This->usePowerAmpLimiter){
        BMMultiLevelSVF_processBufferStereoD(&This->AAFilter2_1[0], wsPosL, wsPosR, wsPosL, wsPosR, numSamples);
        BMMultiLevelSVF_processBufferStereoD(&This->AAFilter2_1[1], wsNegL, wsNegR, wsNegL, wsNegR, numSamples);
        BMMultiLevelSVF_processBufferStereoD(&This->AAFilter2_2[0], lpPosL, lpPosR, lpPosL, lpPosR, numSamples);
        BMMultiLevelSVF_processBufferStereoD(&This->AAFilter2_2[1], lpNegL, lpNegR, lpNegL, lpNegR, numSamples);
        BMHysteresisLimiter_processStereoRectifiedD(&This->powerAmpLimiter,
                                                    wsPosL, wsPosR, wsNegL, wsNegR,
                                                    wsPosL, wsPosR, wsNegL, wsNegR, numSamples);
    }
    
    // mirror of the limiters' internal anti-aliasing filters on the lowpassed
    // path (see BMGainstage_updateLimiterFilterMirror). Always runs so its
    // state stays current; the switch only selects the signal for the division.
    {
        double *mirOut [4] = {saturator->dbuffers[12], saturator->dbuffers[13], saturator->dbuffers[14], saturator->dbuffers[15]};
        vDSP_biquadmD(This->limiterFilterMirrorD, lp4in, 1, mirOut, 1, numSamples);
        if(This->mirrorLimiterFilters)
            for(size_t ch = 0; ch < 4; ch++) memcpy(lp4out[ch], mirOut[ch], sizeof(double) * numSamples);
    }
    
    // bias
    float bias = 0.01f, negBias = -bias;
    double biasD = 0.01, negBiasD = -biasD;
    vDSP_vsadd(rectPosL, 1, &bias, rectPosL, 1, numSamples);
    vDSP_vsadd(rectPosR, 1, &bias, rectPosR, 1, numSamples);
    vDSP_vsadd(rectNegL, 1, &negBias, rectNegL, 1, numSamples);
    vDSP_vsadd(rectNegR, 1, &negBias, rectNegR, 1, numSamples);
    vDSP_vsaddD(lpPosL, 1, &biasD, lpPosL, 1, numSamples);
    vDSP_vsaddD(lpPosR, 1, &biasD, lpPosR, 1, numSamples);
    vDSP_vsaddD(wsPosL, 1, &biasD, wsPosL, 1, numSamples);
    vDSP_vsaddD(wsPosR, 1, &biasD, wsPosR, 1, numSamples);
    vDSP_vsaddD(lpNegL, 1, &negBiasD, lpNegL, 1, numSamples);
    vDSP_vsaddD(lpNegR, 1, &negBiasD, lpNegR, 1, numSamples);
    vDSP_vsaddD(wsNegL, 1, &negBiasD, wsNegL, 1, numSamples);
    vDSP_vsaddD(wsNegR, 1, &negBiasD, wsNegR, 1, numSamples);
    
    // control signals: waveshaped / lowpassed
    vDSP_vdivD(lpPosL, 1, wsPosL, 1, lpPosL, 1, numSamples);
    vDSP_vdivD(lpPosR, 1, wsPosR, 1, lpPosR, 1, numSamples);
    vDSP_vdivD(lpNegL, 1, wsNegL, 1, lpNegL, 1, numSamples);
    vDSP_vdivD(lpNegR, 1, wsNegR, 1, lpNegR, 1, numSamples);
    
    // delay the rectified input (float)
    float *delayedPosL = saturator->buffers[8];
    float *delayedPosR = saturator->buffers[9];
    float *delayedNegL = saturator->buffers[10];
    float *delayedNegR = saturator->buffers[11];
    float *rectPtrs [4] = {rectPosL, rectPosR, rectNegL, rectNegR};
    float *delayedOutputs [4] = {delayedPosL, delayedPosR, delayedNegL, delayedNegR};
    BMShortSimpleDelay_process(&This->delay, (const float**)rectPtrs, delayedOutputs, 4, numSamples);
    vDSP_vspdp(delayedPosL, 1, wsPosL, 1, numSamples);
    vDSP_vspdp(delayedPosR, 1, wsPosR, 1, numSamples);
    vDSP_vspdp(delayedNegL, 1, wsNegL, 1, numSamples);
    vDSP_vspdp(delayedNegR, 1, wsNegR, 1, numSamples);
    
    // apply and sum
    vDSP_vmulD(lpPosL, 1, wsPosL, 1, lpPosL, 1, numSamples);
    vDSP_vmulD(lpPosR, 1, wsPosR, 1, lpPosR, 1, numSamples);
    vDSP_vmulD(lpNegL, 1, wsNegL, 1, lpNegL, 1, numSamples);
    vDSP_vmulD(lpNegR, 1, wsNegR, 1, lpNegR, 1, numSamples);
    vDSP_vaddD(lpPosL, 1, lpNegL, 1, lpPosL, 1, numSamples);
    vDSP_vaddD(lpPosR, 1, lpNegR, 1, lpPosR, 1, numSamples);
    vDSP_vdpsp(lpPosL, 1, outL, 1, numSamples);
    vDSP_vdpsp(lpPosR, 1, outR, 1, numSamples);
}

#else /* float32 gain stage */

void BMGainstage_processStereo(BMGainstage *This,
                               BMSaturator2 *saturator,
                               float *inL, float *inR,
                               float *outL, float *outR,
                               size_t numSamples){
    
    
    // rectify the input and split into positive and negative side signals
    //
    // output 4 signals:
    //   (pos,neg),(left,right)
    float *rectPosL = saturator->buffers[0];
    float *rectPosR = saturator->buffers[1];
    float *rectNegL = saturator->buffers[2];
    float *rectNegR = saturator->buffers[3];
    BMQuadraticRectifier_processBufferStereoVDSP(&This->rectifier,
                                                 inL, inR,
                                                 rectPosL, rectNegL,
                                                 rectPosR, rectNegR, numSamples);
    
    
    // lowpass filter the positive and negative signals
    // output to a new location to preserve the rectified input
    //
    // output 4 signals:
    //   (pos,neg),(left,right)
    float *lpPosL = saturator->buffers[4];
    float *lpPosR = saturator->buffers[5];
    float *lpNegL = saturator->buffers[6];
    float *lpNegR = saturator->buffers[7];
    //
    // antialiasing filters to prevent high frequencies from aliasing into the control signal
    BMMultiLevelSVF_processBufferStereo(&This->AAFilter1[0], rectPosL, rectPosR, rectPosL, rectPosR, numSamples);
		BMMultiLevelSVF_processBufferStereo(&This->AAFilter1[1], rectNegL, rectNegR, rectNegL, rectNegR, numSamples);
    
    // apply waveshaping to the positive and negative signals, buffering to a new
    // location to preserve both the waveshaped and the non-waveshaped signals
    //
    // output 4 signals:
    //   (pos,neg),(left,right)
    // ws: 8-11   lp: 4-7
    float *wsPosL = saturator->buffers[8];
    float *wsPosR = saturator->buffers[9];
    float *wsNegL = saturator->buffers[10];
    float *wsNegR = saturator->buffers[11];
    BMHysteresisLimiter_processStereoRectified(&This->preAmpLimiter,
                                               lpPosL, lpPosR, lpNegL, lpNegR,
                                               wsPosL, wsPosR, wsNegL, wsNegR,
                                               numSamples);
    
	if(This->usePowerAmpLimiter){
		// second-stage AA
		// antialiasing filters to prevent high frequencies from aliasing into the control signal. This must be done twice. Once on the ws signal and again on the lp signal.
		BMMultiLevelSVF_processBufferStereo(&This->AAFilter2_1[0], wsPosL, wsPosR, wsPosL, wsPosR, numSamples);
		BMMultiLevelSVF_processBufferStereo(&This->AAFilter2_1[1], wsNegL, wsNegR, wsNegL, wsNegR, numSamples);
		BMMultiLevelSVF_processBufferStereo(&This->AAFilter2_2[0], lpPosL, lpPosR, lpPosL, lpPosR, numSamples);
		BMMultiLevelSVF_processBufferStereo(&This->AAFilter2_2[1], lpNegL, lpNegR, lpNegL, lpNegR, numSamples);
    
		// power amp limiter
		BMHysteresisLimiter_processStereoRectified(&This->powerAmpLimiter,
                                               wsPosL, wsPosR, wsNegL, wsNegR,
                                               wsPosL, wsPosR, wsNegL, wsNegR,
                                               numSamples);
	}
    
    
    // mirror of the limiters' internal anti-aliasing filters on the lowpassed
    // path (see BMGainstage_updateLimiterFilterMirror). Always runs so its
    // state stays current; the switch only selects the signal for the division.
    {
        float *mirPosL = saturator->buffers[12], *mirPosR = saturator->buffers[13];
        float *mirNegL = saturator->buffers[14], *mirNegR = saturator->buffers[15];
        BMMultiLevelBiquad_processBuffer4(&This->limiterFilterMirror,
                                          lpPosL, lpPosR, lpNegL, lpNegR,
                                          mirPosL, mirPosR, mirNegL, mirNegR,
                                          numSamples);
        if(This->mirrorLimiterFilters){
            memcpy(lpPosL, mirPosL, sizeof(float) * numSamples);
            memcpy(lpPosR, mirPosR, sizeof(float) * numSamples);
            memcpy(lpNegL, mirNegL, sizeof(float) * numSamples);
            memcpy(lpNegR, mirNegR, sizeof(float) * numSamples);
        }
    }
    
    // bias the signals to prevent divide by zero errors in the next step.
    //
    // output 12 signals:
    //   (waveshaped, non-waveshaped, rectifiedInput),(pos,neg),(left,right)
    // ws: 8-11   in: 0-3    lp: 4-7
    float bias = 0.01f;
    float negBias = -bias;
    //
    /* POSITIVE SIDE */
    // rectified
    vDSP_vsadd(rectPosL, 1, &bias, rectPosL, 1, numSamples);
    vDSP_vsadd(rectPosR, 1, &bias, rectPosR, 1, numSamples);
    // low passed
    vDSP_vsadd(lpPosL, 1, &bias, lpPosL, 1, numSamples);
    vDSP_vsadd(lpPosR, 1, &bias, lpPosR, 1, numSamples);
    // waveshaped
    vDSP_vsadd(wsPosL, 1, &bias, wsPosL, 1, numSamples);
    vDSP_vsadd(wsPosR, 1, &bias, wsPosR, 1, numSamples);
    
    /* NEGATIVE SIDE */
    // rectified
    vDSP_vsadd(rectNegL, 1, &negBias, rectNegL, 1, numSamples);
    vDSP_vsadd(rectNegR, 1, &negBias, rectNegR, 1, numSamples);
    // low passed
    vDSP_vsadd(lpNegL, 1, &negBias, lpNegL, 1, numSamples);
    vDSP_vsadd(lpNegR, 1, &negBias, lpNegR, 1, numSamples);
    // waveshaped
    vDSP_vsadd(wsNegL, 1, &negBias, wsNegL, 1, numSamples);
    vDSP_vsadd(wsNegR, 1, &negBias, wsNegR, 1, numSamples);
    
    
    // divide the waveshaped signal by the non-waveshaped signal to get control
    // signals for saturating the input
    //
    // output 4 signals:
    //   (pos,neg),(left,right)
    float *csPosL = saturator->buffers[4];
    float *csPosR = saturator->buffers[5];
    float *csNegL = saturator->buffers[6];
    float *csNegR = saturator->buffers[7];
    // ws: buffers 8-11   lp: 4-7   cs: 4-7   rect: 0-3
    vDSP_vdiv(lpPosL, 1, wsPosL, 1, csPosL, 1, numSamples);
    vDSP_vdiv(lpPosR, 1, wsPosR, 1, csPosR, 1, numSamples);
    vDSP_vdiv(lpNegL, 1, wsNegL, 1, csNegL, 1, numSamples);
    vDSP_vdiv(lpNegR, 1, wsNegR, 1, csNegR, 1, numSamples);
    
    
    // delay to compensate for AAFilter group delay
    float *delayedPosL = saturator->buffers[8];
    float *delayedPosR = saturator->buffers[9];
    float *delayedNegL = saturator->buffers[10];
    float *delayedNegR = saturator->buffers[11];
    float *rectPtrs [4] = {rectPosL, rectPosR, rectNegL, rectNegR};
    float *delayedOutputs [4] = {delayedPosL, delayedPosR, delayedNegL, delayedNegR};
    BMShortSimpleDelay_process(&This->delay, (const float**)rectPtrs, delayedOutputs, 4, numSamples);
    
    
    // multiply the control signals by the biased rectifiedInputs
    // the nonlinear stage will output 4 signals:
    //   (pos,neg),(left,right)
    // cs: 4-7   rect: 0-3   fnl: 4-7  delay: 8-11
    float *fnlPosL = saturator->buffers[4];
    float *fnlPosR = saturator->buffers[5];
    float *fnlNegL = saturator->buffers[6];
    float *fnlNegR = saturator->buffers[7];
    vDSP_vmul(csPosL, 1, delayedPosL, 1, fnlPosL, 1, numSamples);
    vDSP_vmul(csPosR, 1, delayedPosR, 1, fnlPosR, 1, numSamples);
    vDSP_vmul(csNegL, 1, delayedNegL, 1, fnlNegL, 1, numSamples);
    vDSP_vmul(csNegR, 1, delayedNegR, 1, fnlNegR, 1, numSamples);
    
    // sum the positive and negative rectified channels to get the final result
    //
    // output 2 signals:
    //  (left,right)
    vDSP_vadd(fnlPosL, 1, fnlNegL, 1, outL, 1, numSamples);
    vDSP_vadd(fnlPosR, 1, fnlNegR, 1, outR, 1, numSamples);
}




void BMGainstage_processMono(BMGainstage *This,
                             BMSaturator2 *saturator,
                             float *inL,
                             float *outL,
                             size_t numSamples){
    
    
    // rectify the input and split into positive and negative side signals
    //
    // output 4 signals:
    //   (pos,neg),(left,right)
    float *rectPosL = saturator->buffers[0];
    float *rectNegL = saturator->buffers[2];
    BMQuadraticRectifier_processBufferMonoVDSP(&This->rectifier,
                                               inL,
                                               rectPosL, rectNegL,
                                               numSamples);
    
    
    // lowpass filter the positive and negative signals
    // output to a new location to preserve the rectified input
    //
    // output 4 signals:
    //   (pos,neg),(left,right)
    float *lpPosL = saturator->buffers[4];
    float *lpNegL = saturator->buffers[6];
    //
    // antialiasing filters to prevent high frequencies from aliasing into the control signal
    BMMultiLevelSVF_processBufferStereo(&This->AAFilter1[0], rectPosL, rectNegL, rectPosL, rectNegL, numSamples);
    
    // apply waveshaping to the positive and negative signals, buffering to a new
    // location to preserve both the waveshaped and the non-waveshaped signals
    //
    // output 4 signals:
    //   (pos,neg),(left,right)
    // ws: 8-11   lp: 4-7
    float *wsPosL = saturator->buffers[8];
    float *wsNegL = saturator->buffers[10];
    BMHysteresisLimiter_processMonoRectified(&This->preAmpLimiter,
                                             lpPosL, lpNegL,
                                             wsPosL, wsNegL,
                                             numSamples);
    
    
    // second-stage
    // antialiasing filters to prevent high frequencies from aliasing into the
    // control signal. This must be done separately on both the ws and lp signals
    BMMultiLevelSVF_processBufferStereo(&This->AAFilter2_1[0], wsPosL, wsNegL, wsPosL, wsNegL, numSamples);
    BMMultiLevelSVF_processBufferStereo(&This->AAFilter2_2[0], lpPosL, lpNegL, lpPosL, lpNegL, numSamples);
    
    // power amp stage
    BMHysteresisLimiter_processMonoRectified(&This->powerAmpLimiter,
                                             wsPosL, wsNegL,
                                             wsPosL, wsNegL,
                                             numSamples);
    
    
    // mirror of the limiters' internal anti-aliasing filters on the lowpassed
    // path (see BMGainstage_updateLimiterFilterMirror). Always runs so its
    // state stays current; the switch only selects the signal for the division.
    {
        float *mirPosL = saturator->buffers[12], *mirNegL = saturator->buffers[14];
        BMMultiLevelBiquad_processBufferStereo(&This->limiterFilterMirror, lpPosL, lpNegL, mirPosL, mirNegL, numSamples);
        if(This->mirrorLimiterFilters){
            memcpy(lpPosL, mirPosL, sizeof(float) * numSamples);
            memcpy(lpNegL, mirNegL, sizeof(float) * numSamples);
        }
    }
    
    // bias the signals to prevent divide by zero errors in the next step.
    //
    // output 12 signals:
    //   (waveshaped, non-waveshaped, rectifiedInput),(pos,neg),(left,right)
    // ws: 8-11   in: 0-3    lp: 4-7
    float bias = 0.01f;
    float negBias = -bias;
    //
    /* POSITIVE SIDE */
    // rectified
    vDSP_vsadd(rectPosL, 1, &bias, rectPosL, 1, numSamples);
    // low passed
    vDSP_vsadd(lpPosL, 1, &bias, lpPosL, 1, numSamples);
    // waveshaped
    vDSP_vsadd(wsPosL, 1, &bias, wsPosL, 1, numSamples);
    
    /* NEGATIVE SIDE */
    // rectified
    vDSP_vsadd(rectNegL, 1, &negBias, rectNegL, 1, numSamples);
    // low passed
    vDSP_vsadd(lpNegL, 1, &negBias, lpNegL, 1, numSamples);
    // waveshaped
    vDSP_vsadd(wsNegL, 1, &negBias, wsNegL, 1, numSamples);
    
    
    // divide the waveshaped signal by the non-waveshaped signal to get control
    // signals for saturating the input
    //
    // output 4 signals:
    //   (pos,neg),(left,right)
    float *csPosL = saturator->buffers[4];
    float *csNegL = saturator->buffers[6];
    // ws: buffers 8-11   lp: 4-7   cs: 4-7   rect: 0-3
    vDSP_vdiv(lpPosL, 1, wsPosL, 1, csPosL, 1, numSamples);
    vDSP_vdiv(lpNegL, 1, wsNegL, 1, csNegL, 1, numSamples);
    
    
    // delay to compensate for AAFilter group delay
    float *delayedPosL = saturator->buffers[8];
    float *delayedNegL = saturator->buffers[10];
    float *rectPtrs [2] = {rectPosL, rectNegL};
    float *delayedOutputs [2] = {delayedPosL, delayedNegL};
    BMShortSimpleDelay_process(&This->delay, (const float**)rectPtrs, delayedOutputs, 2, numSamples);
    
    
    // multiply the control signals by the biased rectifiedInputs
    // the nonlinear stage will output 4 signals:
    //   (pos,neg),(left,right)
    // cs: 4-7   rect: 0-3   fnl: 4-7  delay: 8-11
    float *fnlPosL = saturator->buffers[4];
    float *fnlNegL = saturator->buffers[6];
    vDSP_vmul(csPosL, 1, delayedPosL, 1, fnlPosL, 1, numSamples);
    vDSP_vmul(csNegL, 1, delayedNegL, 1, fnlNegL, 1, numSamples);
    
    // sum the positive and negative rectified channels to get the final result
    vDSP_vadd(fnlPosL, 1, fnlNegL, 1, outL, 1, numSamples);
}

#endif /* BM_SAT2_DOUBLE_GAINSTAGE */





void BMGainstage_setPowerAmpLimiter(BMGainstage *This, bool paLimiterOn){
	This->usePowerAmpLimiter = paLimiterOn;
	
	// the group delay will change as a result of this, so we need to update
	// the delay by calling the following function:
	BMGainstage_setAAFilterFc(This, This->aaFilterFc);
}





/*
 * Shared by the configuration-time and realtime cutoff setters: compute the
 * three 6 dB lowpass coefficients (the float filters pick them up at their
 * next process call, state preserved) and retarget the audio-path delay to
 * the new group delay (the delay moves to its target itself).
 *
 * There is an AA filter inside each hysteresis limiter but it is not
 * adjusted here: the limiters rely on it to keep the spectrum balanced, and
 * filtering at too low a cutoff there messes that up. Use
 * BMGainstage_setLimiterAAFilterFc for those.
 */
float BMGainstage_lowpass6dBGroupDelay(float fc, float sampleRate, float f){
    // H(z) = b0 (1 + z^-1) / (1 - p z^-1) with p = (1 - g) / (1 + g), g = tan(pi fc / fs).
    // The zero at Nyquist contributes 1/2 sample; the pole contributes
    // (p cos w - p^2) / (1 - 2 p cos w + p^2), w = 2 pi f / fs.
    double g = tan(M_PI * (double)fc / (double)sampleRate);
    double p = (1.0 - g) / (1.0 + g);
    double cw = cos(2.0 * M_PI * (double)f / (double)sampleRate);
    return (float)(0.5 + (p * cw - p * p) / (1.0 - 2.0 * p * cw + p * p));
}



static void BMGainstage_computeAAFilterFc(BMGainstage *This, float fc, bool retargetDelay){
	This->aaFilterFc = fc;
    // the SVF setters are thread-safe: the new coefficients are picked up at
    // the start of the next buffer with the state preserved
    for(size_t i = 0; i < This->numAAFilterPairs; i++){
        BMMultiLevelSVF_setLowpass6dB(&This->AAFilter1[i],   fc, 0);
        BMMultiLevelSVF_setLowpass6dB(&This->AAFilter2_1[i], fc, 0);
        BMMultiLevelSVF_setLowpass6dB(&This->AAFilter2_2[i], fc, 0);
    }
    
    // group delay of everything on the control-signal path, measured at the
    // AA filters' cutoff, by BMMultiLevelBiquad_groupDelay throughout (see
    // aaFilterGroupDelayShadow and BM_SAT2_GROUP_DELAY_COMPENSATION_SCALE)
    BMMultiLevelBiquad_setLowPass6db(&This->aaFilterGroupDelayShadow, fc, 0);
    float f = fc;
    float groupDelayF = BMMultiLevelBiquad_groupDelay(&This->aaFilterGroupDelayShadow, f);
	groupDelayF += BMMultiLevelBiquad_groupDelay(&This->preAmpLimiter.AAFilter, f);
	if(This->usePowerAmpLimiter){
		groupDelayF += BMMultiLevelBiquad_groupDelay(&This->aaFilterGroupDelayShadow, f);
		groupDelayF += BMMultiLevelBiquad_groupDelay(&This->powerAmpLimiter.AAFilter, f);
	}
    // scaled so that the delay is what the amp was voiced with, see
    // BM_SAT2_GROUP_DELAY_COMPENSATION_SCALE
    groupDelayF *= This->groupDelayScale;
    if(retargetDelay) BMShortSimpleDelay_changeLength(&This->delay, (size_t)round(groupDelayF));
}



/*
 * Configuration time: sets the cutoff, retargets the group-delay
 * compensation and refreshes the limiter filter mirror (which rebuilds a
 * vDSP setup, so not while audio runs).
 */
void BMGainstage_setAAFilterFc(BMGainstage *This, float fc){
    BMGainstage_computeAAFilterFc(This, fc, true);
    
    // the limiters' filters are part of the group delay above; keep their
    // mirror on the lowpassed path in step with them
    BMGainstage_updateLimiterFilterMirror(This);
}



/*
 * Live (any thread, audio running): the same cutoff change through the SVFs'
 * thread-safe setters, state preserved, so the filters themselves are
 * click-free. The mirror does not depend on this cutoff and is left alone.
 * The audio-path compensation delay is retargeted as well; when its integer
 * length changes, BMShortSimpleDelay re-initialises it with zeros on the
 * audio thread, a dropout of about the delay length (39 samples at the
 * default, up to about 200 at 500 Hz; 50 to 250 us at 16 x 48 kHz). That is
 * accepted: this slider is a testing control. The delay is measured at the
 * cutoff, so it changes with every slider step.
 */
void BMGainstage_setAAFilterFcRealtime(BMGainstage *This, float fc){
    BMGainstage_computeAAFilterFc(This, fc, true);
}



/*
 * Live: set the group-delay multiplier and retarget the compensation delay
 * (same dropout caveat as above).
 */
void BMGainstage_setGroupDelayScale(BMGainstage *This, float scale){
    This->groupDelayScale = scale;
    BMGainstage_computeAAFilterFc(This, This->aaFilterFc, true);
}





/*
 * Copy the coefficients of the two hysteresis limiters' anti-aliasing filters
 * into the mirror filter on the lowpassed path: the first level(s) from the
 * preamp limiter, the following level(s) from the power amp limiter. This is
 * the only place the mirror's coefficients are written, so it always matches
 * whatever the limiters' filters are set to. It is called from
 * BMGainstage_setAAFilterFc, which runs at init, on
 * BMGainstage_setLimiterAAFilterFc and on the power amp limiter switch.
 * Configuration time only: it rebuilds the double-precision setup.
 */
void BMGainstage_updateLimiterFilterMirror(BMGainstage *This){
    BMMultiLevelBiquad *mirror = &This->limiterFilterMirror;
    BMMultiLevelBiquad *sources [2] = {&This->preAmpLimiter.AAFilter, &This->powerAmpLimiter.AAFilter};
    size_t numChannels = mirror->numChannels;
    size_t level = 0;
    for(size_t s = 0; s < 2; s++){
        assert(sources[s]->numChannels == numChannels);
        for(size_t i = 0; i < sources[s]->numLevels; i++){
            assert(level < mirror->numLevels);
            memcpy(mirror->coefficients_d + level * numChannels * 5,
                   sources[s]->coefficients_d + i * numChannels * 5,
                   sizeof(double) * numChannels * 5);
            // the power amp limiter's sections are only in the waveshaped path when that limiter is
            if(s == 1) BMMultiLevelBiquad_setActiveOnLevel(mirror, This->usePowerAmpLimiter, level);
            level++;
        }
    }
    BMMultiLevelBiquad_queueUpdate(mirror);
    
    // double-precision copy for the double gain stage. The preamp levels come
    // first, so leaving the power amp levels out is a truncation.
    size_t activeLevels = This->preAmpLimiter.AAFilter.numLevels
                        + (This->usePowerAmpLimiter ? This->powerAmpLimiter.AAFilter.numLevels : 0);
    if(This->limiterFilterMirrorD) vDSP_biquadm_DestroySetupD(This->limiterFilterMirrorD);
    This->limiterFilterMirrorD = vDSP_biquadm_CreateSetupD(mirror->coefficients_d, activeLevels, numChannels);
}



/*
 * Set the cutoff of the anti-aliasing filters inside both hysteresis limiters
 * and keep their mirror on the lowpassed path in step. This is the supported
 * way to change those filters from the gain stage: setting them on the
 * limiters directly would leave the mirror stale. Recomputes the group-delay
 * compensation as well. Configuration time only.
 */
void BMGainstage_setLimiterAAFilterFc(BMGainstage *This, float fc){
    BMHysteresisLimiter_setAAFilterFC(&This->preAmpLimiter, fc);
    BMHysteresisLimiter_setAAFilterFC(&This->powerAmpLimiter, fc);
    // recomputes the delay and refreshes the mirror
    BMGainstage_setAAFilterFc(This, This->aaFilterFc);
}



/*
 * Switch the mirror filter into or out of the lowpassed path. The filter
 * keeps running either way, so this is a plain selection and is click-free.
 */
void BMGainstage_setMirrorLimiterFilters(BMGainstage *This, bool on){
    This->mirrorLimiterFilters = on;
}




bool BMGainstage_limiterFilterMirrorIsInStep(BMGainstage *This){
    BMMultiLevelBiquad *mirror = &This->limiterFilterMirror;
    BMMultiLevelBiquad *sources [2] = {&This->preAmpLimiter.AAFilter, &This->powerAmpLimiter.AAFilter};
    size_t numChannels = mirror->numChannels;
    size_t level = 0;
    for(size_t s = 0; s < 2; s++){
        if(sources[s]->numChannels != numChannels) return false;
        for(size_t i = 0; i < sources[s]->numLevels; i++){
            if(level >= mirror->numLevels) return false;
            if(memcmp(mirror->coefficients_d + level * numChannels * 5,
                      sources[s]->coefficients_d + i * numChannels * 5,
                      sizeof(double) * numChannels * 5) != 0) return false;
            level++;
        }
    }
    return level == mirror->numLevels;
}




void BMGainstage_free(BMGainstage *This){
    BMMultiLevelBiquad_free(&This->aaFilterGroupDelayShadow);
    for(size_t i = 0; i < This->numAAFilterPairs; i++){
        BMMultiLevelSVF_free(&This->AAFilter1[i]);
        BMMultiLevelSVF_free(&This->AAFilter2_1[i]);
        BMMultiLevelSVF_free(&This->AAFilter2_2[i]);
    }
    BMMultiLevelBiquad_free(&This->limiterFilterMirror);
    if(This->limiterFilterMirrorD) vDSP_biquadm_DestroySetupD(This->limiterFilterMirrorD);
    This->limiterFilterMirrorD = NULL;
    BMShortSimpleDelay_free(&This->delay);
    BMHysteresisLimiter_free(&This->preAmpLimiter);
    BMHysteresisLimiter_free(&This->powerAmpLimiter);
}





//void BMSaturator2_setAAFilterFc(BMSaturator2 *This, float fc){
//	BMGainstage_setAAFilterFc(&This->gainStage, fc);
//}





void BMSaturator2_free(BMSaturator2 *This){
    // free the reconfigurable filters
    BMSaturator2_freeReconfigurableFilters(This);
    
    // free the other filters
#if BM_SAT2_INCLUDE_REVERB
    BMReverbFree(&This->reverb);
#endif
	BMPeakLimiter_free(&This->peakLimiter);
}





void arrayToFileWithName2(const float *array, char *name, size_t length){
    // open a file for writing
    FILE *audioFile;
    audioFile = fopen(name, "w+");
    
    // print out the entire frame in .csv format
    //fprintf(audioFile,"{");
    for (size_t i=0; i<length; i++) {
        float arrayX = (float)i / (float)length;
        fprintf(audioFile, "%f,%f\n",arrayX,array[i]);
    }
    
    fclose(audioFile);
    
    //system("pwd");
    printf("/");
    printf("%s", name);
    printf("\n");
}





void arrayToFile2(const float *array, size_t length){
    char *name = "arrayOut.csv";
    arrayToFileWithName2(array, name, length);
}







/************************************************
 *   Pre / post filter cascades (EQ + tone)     *
 ************************************************/

/*
 * The 5-band EQ lives on biquad levels 0..4 of preEQ / postEQ. The amp tone
 * filters are either biquad levels 5.. of the same cascades (BM_SAT2_TONE_SVF
 * = 0) or the separate preTone / postTone SVFs. When the EQ is compiled out
 * and the tone filters are SVFs, the biquads are not run at all.
 */
/*
 * External tone filters (BMSaturator2_setExternalToneFilters) run in chunks
 * of BM_BUFFER_CHUNK_SIZE so that their filter sweep mode, which limits the
 * buffer length, may be on.
 */
static inline void BMSaturator2_externalMono(BMMultiLevelSVF *const *f, size_t numFilters, const float *in, float *out, size_t n){
    while(n > 0){
        size_t chunk = BM_MIN(n, (size_t)BM_BUFFER_CHUNK_SIZE);
        BMMultiLevelSVF_processBufferMono(f[0], in, out, chunk);
        for(size_t i = 1; i < numFilters; i++)
            BMMultiLevelSVF_processBufferMono(f[i], out, out, chunk);
        in += chunk; out += chunk; n -= chunk;
    }
}

static inline void BMSaturator2_externalStereo(BMMultiLevelSVF *const *f, size_t numFilters, const float *inL, const float *inR, float *outL, float *outR, size_t n){
    while(n > 0){
        size_t chunk = BM_MIN(n, (size_t)BM_BUFFER_CHUNK_SIZE);
        BMMultiLevelSVF_processBufferStereo(f[0], inL, inR, outL, outR, chunk);
        for(size_t i = 1; i < numFilters; i++)
            BMMultiLevelSVF_processBufferStereo(f[i], outL, outR, outL, outR, chunk);
        inL += chunk; inR += chunk; outL += chunk; outR += chunk; n -= chunk;
    }
}

static inline void BMSaturator2_preFilterMono(BMSaturator2 *This, const float *in, float *out, size_t n){
    if(This->numExternalPreTone > 0){
        BMSaturator2_externalMono(This->externalPreTone, This->numExternalPreTone, in, out, n);
        return;
    }
#if BM_SAT2_INCLUDE_EQ || !BM_SAT2_TONE_SVF
    BMMultiLevelBiquad_processBufferMono(&This->preEQ, in, out, n);
    in = out;
#endif
#if BM_SAT2_TONE_SVF
    BMMultiLevelSVF_processBufferMono(&This->preTone, in, out, n);
#endif
}

static inline void BMSaturator2_postFilterMono(BMSaturator2 *This, float *buf, size_t n){
    if(This->numExternalPostTone > 0){
        BMSaturator2_externalMono(This->externalPostTone, This->numExternalPostTone, buf, buf, n);
        return;
    }
#if BM_SAT2_INCLUDE_EQ || !BM_SAT2_TONE_SVF
    BMMultiLevelBiquad_processBufferMono(&This->postEQ, buf, buf, n);
#endif
#if BM_SAT2_TONE_SVF
    BMMultiLevelSVF_processBufferMono(&This->postTone, buf, buf, n);
#endif
}

static inline void BMSaturator2_preFilterStereo(BMSaturator2 *This, const float *inL, const float *inR, float *outL, float *outR, size_t n){
    if(This->numExternalPreTone > 0){
        BMSaturator2_externalStereo(This->externalPreTone, This->numExternalPreTone, inL, inR, outL, outR, n);
        return;
    }
#if BM_SAT2_INCLUDE_EQ || !BM_SAT2_TONE_SVF
    BMMultiLevelBiquad_processBufferStereo(&This->preEQ, inL, inR, outL, outR, n);
    inL = outL; inR = outR;
#endif
#if BM_SAT2_TONE_SVF
    BMMultiLevelSVF_processBufferStereo(&This->preTone, inL, inR, outL, outR, n);
#endif
}

static inline void BMSaturator2_postFilterStereo(BMSaturator2 *This, float *L, float *R, size_t n){
    if(This->numExternalPostTone > 0){
        BMSaturator2_externalStereo(This->externalPostTone, This->numExternalPostTone, L, R, L, R, n);
        return;
    }
#if BM_SAT2_INCLUDE_EQ || !BM_SAT2_TONE_SVF
    BMMultiLevelBiquad_processBufferStereo(&This->postEQ, L, R, L, R, n);
#endif
#if BM_SAT2_TONE_SVF
    BMMultiLevelSVF_processBufferStereo(&This->postTone, L, R, L, R, n);
#endif
}


/*
 * Gain stage bypass (BMSaturator2_setGainStageBypass): out, the gain
 * stage's output, is crossfaded in place against in, its input, at the
 * oversampled rate. gainStageMix ramps linearly toward 1 (gain stage) or 0
 * (bypass) by gainStageMixStep per sample; once it has arrived the mix is a
 * scaled copy or nothing at all. The input is scaled by gainStageBypassGain
 * (BMSaturator2_setGainStageBypassGainDb).
 */
static inline void BMSaturator2_gainStageCrossfadeMono(BMSaturator2 *This, const float *in, float *out, size_t n){
    const float target = This->gainStageBypass ? 0.0f : 1.0f;
    const float bypassGain = This->gainStageBypassGain;
    float mix = This->gainStageMix;
    if(mix == target){
        if(target == 0.0f) vDSP_vsmul(in, 1, &bypassGain, out, 1, n);
        return;
    }
    const float step = target > mix ? This->gainStageMixStep : -This->gainStageMixStep;
    for(size_t i = 0; i < n; i++){
        mix += step;
        if((step > 0.0f && mix >= target) || (step < 0.0f && mix <= target)) mix = target;
        out[i] = bypassGain * in[i] + mix * (out[i] - bypassGain * in[i]);
    }
    This->gainStageMix = mix;
}

static inline void BMSaturator2_gainStageCrossfadeStereo(BMSaturator2 *This, const float *inL, const float *inR, float *outL, float *outR, size_t n){
    const float target = This->gainStageBypass ? 0.0f : 1.0f;
    const float bypassGain = This->gainStageBypassGain;
    float mix = This->gainStageMix;
    if(mix == target){
        if(target == 0.0f){
            vDSP_vsmul(inL, 1, &bypassGain, outL, 1, n);
            vDSP_vsmul(inR, 1, &bypassGain, outR, 1, n);
        }
        return;
    }
    const float step = target > mix ? This->gainStageMixStep : -This->gainStageMixStep;
    for(size_t i = 0; i < n; i++){
        mix += step;
        if((step > 0.0f && mix >= target) || (step < 0.0f && mix <= target)) mix = target;
        outL[i] = bypassGain * inL[i] + mix * (outL[i] - bypassGain * inL[i]);
        outR[i] = bypassGain * inR[i] + mix * (outR[i] - bypassGain * inR[i]);
    }
    This->gainStageMix = mix;
}



/*!
 *BMSaturator2_processBufferStereo
 *
 * @abstract This is the main stereo processing function for the saturator
 */
void BMSaturator2_processBufferStereo(BMSaturator2 *This,
                                      const float *inL, const float *inR,
                                      float *outL, float *outR,
                                      size_t numSamples){
    
    // save a backup of the output pointers
    float *outLb = outL;
    float *outRb = outR;
    size_t numSamplesB = numSamples;
    
    // chunked processing
    while(numSamples > 0){
        
        // find the number of samples we can process in a single iteration
        size_t samplesProcessing = BM_MIN(numSamples, BM_BUFFER_CHUNK_SIZE);
        
        
        // find the number of samples we will process in the oversampled sections of the code
        size_t samplesProcessingOS = samplesProcessing  * This->oversamplingFactor;
        
        
        // set up pointers to buffer memory
        float *inLEmphasized  = This->buffers[4];
        float *inREmphasized  = This->buffers[5];
        float *inLOversampled = This->buffers[6];
        float *inROversampled = This->buffers[7];
        
        
        // run the preamp filters
        BMSaturator2_preFilterStereo(This, inL, inR, inLEmphasized, inREmphasized, samplesProcessing);
        
        // process input gain
        BMSmoothGain_processBuffer(&This->inputGain,
                                   inLEmphasized, inREmphasized,
                                   inLEmphasized, inREmphasized,
                                   samplesProcessing);
        
        // upsample
        if(This->oversamplingFactor > 1)
            BMUpsampler_processBufferStereo(&This->upsampler, inLEmphasized, inREmphasized, inLOversampled, inROversampled, samplesProcessing);
        else {
            memcpy(inLOversampled,inLEmphasized,sizeof(float)*samplesProcessing);
            memcpy(inROversampled,inREmphasized,sizeof(float)*samplesProcessing);
        }
        
        // process the gain stage
        float *outLOS = This->buffers[0];
        float *outROS = This->buffers[1];
        BMGainstage_processStereo(&This->gainStage,
                                  This,
                                  inLOversampled, inROversampled,
                                  outLOS, outROS,
                                  samplesProcessingOS);
        BMSaturator2_gainStageCrossfadeStereo(This, inLOversampled, inROversampled, outLOS, outROS, samplesProcessingOS);
        
        // downsample
        if(This->oversamplingFactor > 1)
            BMDownsampler_processBufferStereo(&This->downsampler, outLOS, outROS, outL, outR, samplesProcessingOS);
        else {
            memcpy(outL,outLOS,sizeof(float)*samplesProcessing);
            memcpy(outR,outROS,sizeof(float)*samplesProcessing);
        }
        
        // advance pointers
        inL += samplesProcessing;
        inR += samplesProcessing;
        outL += samplesProcessing;
        outR += samplesProcessing;
        numSamples -= samplesProcessing;
    }
    
    // post-amp filters to model speaker cabinet response
    BMSaturator2_postFilterStereo(This, outLb, outRb, numSamplesB);
	
	// decorrelate the L and R channels
	BMMonoToStereo_processBuffer(&This->monoToStereo, outLb, outRb, outLb, outRb, numSamplesB);
    
    // reverb
#if BM_SAT2_INCLUDE_REVERB
    if(This->useReverb)
        BMReverbProcessBuffer(&This->reverb, outLb, outRb, outLb, outRb, numSamplesB);
#endif
    
    // adjust the output gain
    BMSmoothGain_processBuffer(&This->outputGain, outLb, outRb, outLb, outRb, numSamplesB);
}





/*!
 *BMSaturator2_processBufferMono
 *
 * @abstract This is the main mono processing function for the saturator
 */
void BMSaturator2_processBufferMono(BMSaturator2 *This,
                                    const float *inL, const float *inR,
                                    float *outL, float *outR,
                                    size_t numSamples){
    
    // save a backup of the output pointers
    float *outLb = outL;
    float *outRb = outR;
    size_t numSamplesB = numSamples;
    
    // chunked processing
    while(numSamples > 0){
        
        // find the number of samples we can process in a single iteration
        size_t samplesProcessing = BM_MIN(numSamples, BM_BUFFER_CHUNK_SIZE);
        
        
        // find the number of samples we will process in the oversampled sections of the code
        size_t samplesProcessingOS = samplesProcessing * This->oversamplingFactor;
        
        
        // set up pointers to buffer memory
        float *inMixed  = This->buffers[4];
        float *inOversampled = This->buffers[6];
        
        // mix to mono
        float half = 0.5;
        vDSP_vasm(inL, 1, inR, 1, &half, inMixed, 1, samplesProcessing);
        
        // run the preamp filters
		BMSaturator2_preFilterMono(This, inMixed, inMixed, samplesProcessing);
        
        // apply the input gain
        BMSmoothGain_processBufferMono(&This->inputGain, inMixed, inMixed, samplesProcessing);
        
        // upsample
        if(This->oversamplingFactor > 1)
            BMUpsampler_processBufferMono(&This->upsampler, inMixed, inOversampled, samplesProcessing);
        else {
            memcpy(inOversampled,inMixed,sizeof(float)*samplesProcessing);
        }
        
        // process the gain stage
        float *outOversampled = This->buffers[0];
        BMGainstage_processMono(&This->gainStage,
                                This,
                                inOversampled,
                                outOversampled,
                                samplesProcessingOS);
        BMSaturator2_gainStageCrossfadeMono(This, inOversampled, outOversampled, samplesProcessingOS);
        
        // downsample
        if(This->oversamplingFactor > 1)
            BMDownsampler_processBufferMono(&This->downsampler, outOversampled, outL, samplesProcessingOS);
        else {
            memcpy(outL,outOversampled,sizeof(float)*samplesProcessing);
        }
        
        // advance pointers
        inL += samplesProcessing;
        inR += samplesProcessing;
        outL += samplesProcessing;
        numSamples -= samplesProcessing;
    }
    
    // post-amp filter to model speaker cabinet response
    BMSaturator2_postFilterMono(This, outLb, numSamplesB);
    
    // adjust the output gain
    BMSmoothGain_processBufferMono(&This->outputGain, outLb, outLb, numSamplesB);
    
    // copy from the L output to the R output
    memcpy(outRb,outLb,sizeof(float)*numSamplesB);
    
    // decorrelate the L and R channels
    BMMonoToStereo_processBuffer(&This->monoToStereo, outLb, outRb, outLb, outRb, numSamplesB);
    
    // reverb
#if BM_SAT2_INCLUDE_REVERB
    if(This->useReverb)
        BMReverbProcessBuffer(&This->reverb, outLb, outRb, outLb, outRb, numSamplesB);
#endif
}





void BMSaturator2_processBuffer(BMSaturator2 *This,
                                const float *inL, const float *inR,
                                float *outL, float *outR,
                                size_t numSamples){
	
    // check if the saturator has finished initialization
    if(This->initCompleted){
		
        // call the apropriate process function for the current stereo / mono setting
        if(This->isStereo)
            BMSaturator2_processBufferStereo(This, inL, inR, outL, outR, numSamples);
        else
            BMSaturator2_processBufferMono(This, inL, inR, outL, outR, numSamples);
		
		// apply the peak limiter
		if(This->usePeakLimiter)
			BMPeakLimiter_processStereo(&This->peakLimiter, outL, outR, outL, outR, numSamples);
        
        // apply a smooth switch to prevent clicks during settings transitions
        BMSmoothSwitch_processBufferStereo(&This->smoothSwitch, outL, outR, outL, outR, numSamples);
        
        // change between stereo and mono or change oversampling factor
		// of change amp type if requested
        bool structuralChange = (This->isStereo != This->requestedStereo ||
								 This->oversamplingFactor != This->requestedOversamplingFactor ||
								 This->requestedAmpType != This->ampType);
        if(structuralChange || This->erPending || This->erSide[0].pending || This->erSide[1].pending){
            
            // if the audio is switched off, apply the change now
            if(BMSmoothSwitch_getState(&This->smoothSwitch) == BMSwitchOff) {
                if(structuralChange){
                    // free and re-init the reconfigurable filters (the early-
                    // reflection model is reapplied from the stored settings there)
                    // (not while a control thread puts a new convolution into the cabinet that is freed here)
                    BMLock_lock(&This->erConvolutionLock);
                    BMSaturator2_freeReconfigurableFilters(This);
                    BMSaturator2_initReconfigurableFilters(This);
                    BMLock_unlock(&This->erConvolutionLock);
                    BMSaturator2_applyAmpType(This);
                }
                // a new early-reflection model alone: rebuild just the decorrelator that changed
                else {
                    if(This->erPending)
                        BMMonoToStereo_setEarlyReflections(&This->monoToStereo, This->erMinDelayS, This->erMaxDelayS, This->erNumWetTaps,
                                                           This->erRT60S, This->erDryGain, This->erWetTapGain, This->erBassCrossoverHz, This->erTrebleCrossoverHz);
                    for(size_t b=0; b<2; b++)
                        if(This->erSide[b].pending)
                            BMSaturator2_applySideBandEarlyReflections(This, (enum BMMonoToStereoSideBand)b);
                }
                This->erPending = false;
                This->erSide[0].pending = This->erSide[1].pending = false;
            }
            
            // otherwise turn off the output switch first
            BMSmoothSwitch_setState(&This->smoothSwitch, false);
        }
        // if we're not in the process of changing audio system settings,
        // then the output switch should be on
        else
            BMSmoothSwitch_setState(&This->smoothSwitch, true);
    }
    
    // if not initialized yet, output zeros
    else {
        memset(outL,0,sizeof(float)*numSamples);
        memset(outR,0,sizeof(float)*numSamples);
    }
}






void BMSaturator2_setInputGain(BMSaturator2 *This, float gainDb){
    // save the input gain setting
    This->preEQState.gain = gainDb;
    
    // offset the gain by the base gain value
    gainDb += This->baseInputGain;
    
    // set the gain
    BMSmoothGain_setGainDb(&This->inputGain,gainDb);
}



/*!
 *BMSaturator2_setEQControl
 */
void BMSaturator2_setEQControl(BMSaturator2 *This, float gainDb, size_t level, bool isPreEQ){
	BMMultiLevelBiquad *filter = &This->postEQ;
	BMSat2EQState *state = &This->postEQState;
	BMSat2EQConfig *config = &This->postEQConfig;
	state->eqGain[level] = gainDb;
	if(isPreEQ){
		filter = &This->preEQ;
		state = &This->postEQState;
		config = &This->postEQConfig;
	}
	BMMultiLevelBiquad_setBellQ(filter,
								config->fc[level],
								config->Q[level],
								gainDb,
								level);
}




/*!
 *BMSaturator2_setInputEQ1
 */
void BMSaturator2_setEQActive(BMSaturator2 *This, bool active){
    for(size_t i=0; i<BM_SAT2_EQ_NUM_LEVELS; i++){
        BMMultiLevelBiquad_setActiveOnLevel(&This->preEQ, active, i);
        BMMultiLevelBiquad_setActiveOnLevel(&This->postEQ, active, i);
    }
}

void BMSaturator2_setInputEQ1(BMSaturator2 *This, float gainDb){
	size_t level = 0;
	if(SAT2_POST_EQ_ONE_IS_SHELF){
		This->preEQState.eqGain[level] = gainDb;
		BMMultiLevelBiquad_setHighShelfAdjustableSlope(&This->preEQ,
													   This->preEQConfig.fc[level],
													   gainDb,
													   This->preEQConfig.Q[level],
													   level);
	}
	else
		BMSaturator2_setEQControl(This,gainDb,level,true);
}

/*!
 *BMSaturator2_setInputEQ2
 */
void BMSaturator2_setInputEQ2(BMSaturator2 *This, float gainDb){
	BMSaturator2_setEQControl(This,gainDb,1,true);
}

/*!
 *BMSaturator2_setInputEQ3
 */
void BMSaturator2_setInputEQ3(BMSaturator2 *This, float gainDb){
	BMSaturator2_setEQControl(This,gainDb,2,true);
}

/*!
 *BMSaturator2_setInputEQ4
 */
void BMSaturator2_setInputEQ4(BMSaturator2 *This, float gainDb){
	BMSaturator2_setEQControl(This,gainDb,3,true);
}

/*!
 *BMSaturator2_setInputEQ5
 */
void BMSaturator2_setInputEQ5(BMSaturator2 *This, float gainDb){
	if(SAT2_POST_EQ_FIVE_IS_SHELF){
		This->preEQState.eqGain[4] = gainDb;
		BMMultiLevelBiquad_setHighShelfAdjustableSlope(&This->preEQ,
													   This->preEQConfig.fc[4],
													   gainDb,
													   This->preEQConfig.Q[4],
													   4);
	}
	else
		BMSaturator2_setEQControl(This,gainDb,4,true);
}




/*!
 *BMSaturator2_setOutputEQ1
 */
void BMSaturator2_setOutputEQ1(BMSaturator2 *This, float gainDb){
	size_t level = 0;
	if(SAT2_POST_EQ_ONE_IS_SHELF){
		This->postEQState.eqGain[level] = gainDb;
		BMMultiLevelBiquad_setHighShelfAdjustableSlope(&This->postEQ,
													   This->postEQConfig.fc[level],
													   gainDb,
													   This->postEQConfig.Q[level],
													   level);
	}
	else
		BMSaturator2_setEQControl(This,gainDb,level,false);
}

/*!
 *BMSaturator2_setOutputEQ2
 */
void BMSaturator2_setOutputEQ2(BMSaturator2 *This, float gainDb){
	BMSaturator2_setEQControl(This,gainDb,1,false);
}

/*!
 *BMSaturator2_setOutputEQ3
 */
void BMSaturator2_setOutputEQ3(BMSaturator2 *This, float gainDb){
	BMSaturator2_setEQControl(This,gainDb,2,false);
}

/*!
 *BMSaturator2_setOutputEQ4
 */
void BMSaturator2_setOutputEQ4(BMSaturator2 *This, float gainDb){
	BMSaturator2_setEQControl(This,gainDb,3,false);
}

/*!
 *BMSaturator2_setOutputEQ5
 */
void BMSaturator2_setOutputEQ5(BMSaturator2 *This, float gainDb){
	if(SAT2_POST_EQ_FIVE_IS_SHELF){
		This->postEQState.eqGain[4] = gainDb;
		BMMultiLevelBiquad_setHighShelfAdjustableSlope(&This->postEQ,
													   This->postEQConfig.fc[4],
													   gainDb,
													   This->postEQConfig.Q[4],
													   4);
	}
	else
		BMSaturator2_setEQControl(This,gainDb,4,false);
}



void BMSaturator2_setOutputGain(BMSaturator2 *This, float gainDb){
    // save the input gain setting
    This->postEQState.gain = gainDb;
    
    // offset the gain by the base level
    gainDb += This->baseOutputGain;
    
    BMSmoothGain_setGainDb(&This->outputGain, gainDb);
}







void BMSaturator2_setEarlyReflections(BMSaturator2 *This,
									  float minDelayMs, float maxDelayMs, size_t numWetTaps,
									  float rt60Seconds, float dryGainDb, float wetTapGainDb,
									  float bassCrossoverHz, float trebleCrossoverHz){
	This->earlyReflectionsCustom = true;
	This->erMinDelayS = minDelayMs * 0.001f;
	This->erMaxDelayS = maxDelayMs * 0.001f;
	This->erNumWetTaps = numWetTaps;
	This->erRT60S = rt60Seconds;
	This->erDryGain = BM_DB_TO_GAIN(dryGainDb);
	This->erWetTapGain = BM_DB_TO_GAIN(wetTapGainDb);
	This->erBassCrossoverHz = bassCrossoverHz;
	This->erTrebleCrossoverHz = trebleCrossoverHz;
	BMMonoToStereo_setEarlyReflections(&This->monoToStereo, This->erMinDelayS, This->erMaxDelayS, numWetTaps,
									   rt60Seconds, This->erDryGain, This->erWetTapGain, bassCrossoverHz, trebleCrossoverHz);
}


// builds a side band's reflections from the stored settings
void BMSaturator2_setSideBandEarlyReflectionsSeed(BMSaturator2 *This, enum BMMonoToStereoSideBand band, uint32_t seed){
	This->erSide[band].seed = seed;
}


void BMSaturator2_applySideBandEarlyReflections(BMSaturator2 *This, enum BMMonoToStereoSideBand band){
	BMMonoToStereo_setSideBandSeed(&This->monoToStereo, band,
								   This->erSide[band].seed != 0 ? This->erSide[band].seed : BM_MTS_SIDE_BAND_SEED(band));
	BMMonoToStereo_setSideBandEarlyReflections(&This->monoToStereo, band, This->erSide[band].minDelayS, This->erSide[band].maxDelayS,
											   This->erSide[band].numWetTaps, This->erSide[band].rt60S,
											   This->erDryGain, This->erSide[band].wetTapGain);
	BMMonoToStereo_setSideBandBypass(&This->monoToStereo, band, This->erSide[band].bypassed);
}


void BMSaturator2_setSideBandEarlyReflections(BMSaturator2 *This, enum BMMonoToStereoSideBand band,
											  float minDelayMs, float maxDelayMs, size_t numWetTaps,
											  float rt60Seconds, float wetTapGainDb, bool bypassed){
	assert(This->earlyReflectionsCustom && This->erBassCrossoverHz > 0.0f && This->erTrebleCrossoverHz > This->erBassCrossoverHz);
	This->erSide[band].custom = true;
	This->erSide[band].minDelayS = minDelayMs * 0.001f;
	This->erSide[band].maxDelayS = maxDelayMs * 0.001f;
	This->erSide[band].numWetTaps = numWetTaps;
	This->erSide[band].rt60S = rt60Seconds;
	This->erSide[band].wetTapGain = BM_DB_TO_GAIN(wetTapGainDb);
	This->erSide[band].bypassed = bypassed;
	BMSaturator2_applySideBandEarlyReflections(This, band);
}


void BMSaturator2_requestSideBandEarlyReflections(BMSaturator2 *This, enum BMMonoToStereoSideBand band,
												  float minDelayMs, float maxDelayMs, size_t numWetTaps,
												  float rt60Seconds, float wetTapGainDb){
	assert(This->erSide[band].custom);   // BMSaturator2_setSideBandEarlyReflections comes first, before processing
	// the settings first, the flag last: the audio thread reads them once it sees the flag
	This->erSide[band].minDelayS = minDelayMs * 0.001f;
	This->erSide[band].maxDelayS = maxDelayMs * 0.001f;
	This->erSide[band].numWetTaps = numWetTaps;
	This->erSide[band].rt60S = rt60Seconds;
	This->erSide[band].wetTapGain = BM_DB_TO_GAIN(wetTapGainDb);
	if(This->erConvolved) return;   // stored for the amp's re-initialisations; the convolution is the caller's to renew
	This->erSide[band].pending = true;
}


void BMSaturator2_setSideBandEarlyReflectionsBypass(BMSaturator2 *This, enum BMMonoToStereoSideBand band, bool bypassed){
	assert(This->erSide[band].custom);
	This->erSide[band].bypassed = bypassed;
	if(This->erConvolved) return;
	BMMonoToStereo_setSideBandBypass(&This->monoToStereo, band, bypassed);
}


void BMSaturator2_setSideBandEarlyReflectionsWetTapGainDb(BMSaturator2 *This, enum BMMonoToStereoSideBand band, float wetTapGainDb){
	assert(This->erSide[band].custom);
	This->erSide[band].wetTapGain = BM_DB_TO_GAIN(wetTapGainDb);
	// reflections that are waiting to be built take the new gain with them
	if(This->erSide[band].pending || This->erConvolved) return;
	BMMonoToStereo_setSideBandWetTapGain(&This->monoToStereo, band, This->erSide[band].wetTapGain);
}


void BMSaturator2_setEarlyReflectionsCrossoversHz(BMSaturator2 *This, float bassCrossoverHz, float trebleCrossoverHz){
	assert(This->earlyReflectionsCustom && This->erBassCrossoverHz > 0.0f && This->erTrebleCrossoverHz > This->erBassCrossoverHz);
	assert(bassCrossoverHz > 0.0f && trebleCrossoverHz > bassCrossoverHz);
	This->erBassCrossoverHz = bassCrossoverHz;
	This->erTrebleCrossoverHz = trebleCrossoverHz;
	// a model that is waiting to be built takes the new crossovers with it
	if(This->erPending || This->erConvolved) return;
	BMMonoToStereo_setCrossoversHz(&This->monoToStereo, bassCrossoverHz, trebleCrossoverHz);
}


void BMSaturator2_requestEarlyReflections(BMSaturator2 *This,
										  float minDelayMs, float maxDelayMs, size_t numWetTaps,
										  float rt60Seconds, float dryGainDb, float wetTapGainDb,
									  float bassCrossoverHz, float trebleCrossoverHz){
	assert(This->earlyReflectionsCustom);   // BMSaturator2_setEarlyReflections comes first, before processing
	// the settings first, the flag last: the audio thread reads them once it sees the flag
	This->erMinDelayS = minDelayMs * 0.001f;
	This->erMaxDelayS = maxDelayMs * 0.001f;
	This->erNumWetTaps = numWetTaps;
	This->erRT60S = rt60Seconds;
	This->erDryGain = BM_DB_TO_GAIN(dryGainDb);
	This->erWetTapGain = BM_DB_TO_GAIN(wetTapGainDb);
	This->erBassCrossoverHz = bassCrossoverHz;
	This->erTrebleCrossoverHz = trebleCrossoverHz;
	if(This->erConvolved) return;   // stored for the amp's re-initialisations; the convolution is the caller's to renew
	This->erPending = true;
}


void BMSaturator2_setEarlyReflectionsWetTapGainDb(BMSaturator2 *This, float wetTapGainDb){
	assert(This->earlyReflectionsCustom);
	This->erWetTapGain = BM_DB_TO_GAIN(wetTapGainDb);
	// a model that is waiting to be built takes the new gain with it
	if(This->erPending || This->erConvolved) return;
	BMMonoToStereo_setEarlyReflectionsWetTapGain(&This->monoToStereo, This->erWetTapGain);
}


void BMSaturator2_setEarlyReflectionsConvolved(BMSaturator2 *This, bool convolved){
	assert(This->earlyReflectionsCustom);
	This->erConvolved = convolved;
	if(convolved && !This->isStereo) BMMonoToStereo_bakeConvolution(&This->monoToStereo);
	else BMMonoToStereo_clearConvolution(&This->monoToStereo);
}


bool BMSaturator2_installEarlyReflectionsConvolution(BMSaturator2 *This, const float *left, const float *right, size_t length, bool immediate){
	assert(This->erConvolved);
	BMLock_lock(&This->erConvolutionLock);
	bool installed = This->isStereo ? true : BMMonoToStereo_installConvolution(&This->monoToStereo, left, right, length, immediate);
	BMLock_unlock(&This->erConvolutionLock);
	return installed;
}


void BMSaturator2_setStereoCabinet(BMSaturator2 *This, bool useStereo){
    This->useStereoCab = useStereo;
    // click-free: the cabinet keeps running and fades against its input
    BMMonoToStereo_setBypass(&This->monoToStereo, !useStereo, true);
}





/*!
 *BMSaturator2_setStereoCabWetMixDb
 *
 * @param This struct
 * @param wetMixDb the wet gain in decibels in [FLT_MIN,0]
 */
void BMSaturator2_setStereoCabWetMixDb(BMSaturator2 *This, float wetMixDb){
	assert(wetMixDb <= 0.0f);
	
	This->stereoCabWetMixDb = wetMixDb;
	
	// An early-reflection model overrides the wet mix (see
	// BMSaturator2_setEarlyReflections): its tap gains are explicit, and
	// setting a wet mix would replace them with normalised ones. This function
	// is also called from inside the amp (BMSaturator2_applyAmpType, on the
	// audio thread whenever the amp reconfigures itself, right after the model
	// has been reapplied), so without this check the model's gains were lost
	// as soon as audio ran.
	if(This->earlyReflectionsCustom) return;
	
	BMMonoToStereo_setWetMix(&This->monoToStereo, BM_DB_TO_GAIN(wetMixDb));
}




void BMSaturator2_setRoomType(BMSaturator2 *This, enum Sat2RoomType roomType){
#if !BM_SAT2_INCLUDE_REVERB
    (void)roomType;
    This->useReverb = false;
    return;
#endif
    
    if(roomType != s2room_none){

		// large room
		if(roomType == s2room_large){
			BMReverbSetRT60DecayTime(&This->reverb, 1.3f);
			BMReverbSetWetMix(&This->reverb, 0.55f);
			BMReverbSetHFDecayMultiplier(&This->reverb, SAT2_REVERB_HF_DECAY_MULTIPLIER_DV);
            BMReverbSetHFDecayFC(&This->reverb, 250.0f);
            BMReverbSetHighPassFC(&This->reverb, 200.0f);
			BMReverbSetRoomSize(&This->reverb, 0.015f, 0.100f);
			BMReverbSetLowPassFC(&This->reverb, 6000.0f);
			BMSaturator2_setStereoCabWetMixDb(This, SAT2_STEREO_CABINET_WET_MIX_DB_DV);
		}
		
		// medium room
		if(roomType == s2room_medium){
			BMReverbSetRT60DecayTime(&This->reverb, 0.60f);
			BMReverbSetWetMix(&This->reverb, 0.5f);
			BMReverbSetHFDecayMultiplier(&This->reverb, SAT2_REVERB_HF_DECAY_MULTIPLIER_DV);
            BMReverbSetHFDecayFC(&This->reverb, 750.0f);
			BMReverbSetRoomSize(&This->reverb, 100.0f/48000.0f, 0.050f);
			BMReverbSetLowPassFC(&This->reverb, 8000.0f);
            BMReverbSetHighPassFC(&This->reverb, 80.0f);
			BMSaturator2_setStereoCabWetMixDb(This, SAT2_STEREO_CABINET_WET_MIX_DB_DV);
		}
		
		// small room
		if(roomType == s2room_small){
			BMReverbSetRT60DecayTime(&This->reverb, 0.235f);
			BMReverbSetWetMix(&This->reverb, 0.5f);
			BMReverbSetHFDecayMultiplier(&This->reverb, SAT2_REVERB_HF_DECAY_MULTIPLIER_DV);
            BMReverbSetHFDecayFC(&This->reverb, 2000.0f);
			BMReverbSetRoomSize(&This->reverb, 50.0f/48000.0f, 0.020f);
			BMReverbSetLowPassFC(&This->reverb, 11000.0f);
            BMReverbSetHighPassFC(&This->reverb, 80.0f);
			BMSaturator2_setStereoCabWetMixDb(This, SAT2_STEREO_CABINET_WET_MIX_DB_DV);
		}
		
        This->useReverb = true;
    }
	
	// room reverb off
	else{
		This->useReverb = false;
		BMSaturator2_setStereoCabWetMixDb(This, SAT2_STEREO_CABINET_WET_MIX_DB_DV);
	}
}





void BMSaturator2_setAAFilterFc(BMSaturator2 *This, float fc){
    if(fc < 200.0f) fc = 200.0f;
    if(fc > 20000.0f) fc = 20000.0f;
    This->aaFilterFcSetting = fc;      // reapplied after any gain stage re-init
    if(This->initCompleted)
        BMGainstage_setAAFilterFcRealtime(&This->gainStage, fc);   // thread-safe, click-free
}



void BMSaturator2_setGroupDelayCompensationScale(BMSaturator2 *This, float scale){
    if(scale < 0.01f) scale = 0.01f;
    This->groupDelayScaleSetting = scale;
    if(This->initCompleted)
        BMGainstage_setGroupDelayScale(&This->gainStage, scale);
}



void BMSaturator2_setExternalToneFilters(BMSaturator2 *This,
                                         BMMultiLevelSVF *const *pre, size_t numPre,
                                         BMMultiLevelSVF *const *post, size_t numPost){
    assert(numPre <= BM_SAT2_MAX_EXTERNAL_TONE_FILTERS && numPost <= BM_SAT2_MAX_EXTERNAL_TONE_FILTERS);
    for(size_t i = 0; i < numPre; i++) This->externalPreTone[i] = pre[i];
    for(size_t i = 0; i < numPost; i++) This->externalPostTone[i] = post[i];
    This->numExternalPreTone = numPre;
    This->numExternalPostTone = numPost;
}



void BMSaturator2_setGainStageBypass(BMSaturator2 *This, bool bypassed){
    This->gainStageBypass = bypassed;
}

bool BMSaturator2_gainStageBypassed(const BMSaturator2 *This){
    return This->gainStageBypass;
}

void BMSaturator2_setGainStageBypassGainDb(BMSaturator2 *This, float gainDb){
    This->gainStageBypassGain = powf(10.0f, gainDb / 20.0f);
}



void BMSaturator2_setLimiterFilterMirror(BMSaturator2 *This, bool on){
    This->useLimiterFilterMirror = on;
    BMGainstage_setMirrorLimiterFilters(&This->gainStage, on);
}




void BMSaturator2_setPeakLimiter(BMSaturator2 *This, bool limiterOn){
	This->usePeakLimiter = limiterOn;
}




bool BMSaturator2_getLimiterClipState(BMSaturator2 *This){
	return BMPeakLimiter_isLimiting(&This->peakLimiter);
}




void BMSaturator2_setStereo(BMSaturator2 *This, bool isStereo){
    This->requestedStereo = isStereo;
}




void BMSaturator2_setOversample(BMSaturator2 *This, size_t oversampleFactor){
    assert(powerof2(oversampleFactor));
    printf("oversampleFactor %zu\n",oversampleFactor);
    This->requestedOversamplingFactor = oversampleFactor;
}




float BMSaturator2_getLatencyInSeconds(BMSaturator2 *This){
	// find the latency of the phase compensation delays
	float delayLatency = (float)This->gainStage.delay.delayLength / (float)(This->oversamplingFactor*This->sampleRate);
	
	// find the latency of the oversampler
	float oversamplerLatency = (BMUpsampler_getLatencyInSamples(&This->upsampler) + BMDownsampler_getLatencyInSamples(&This->downsampler)) / This->sampleRate;
	
	// find the total latency
	float totalLatency = delayLatency + oversamplerLatency;
	printf("\nSaturator latency: %f\n", totalLatency);
	return totalLatency;
}





/************************************************
 *            Amp tone filter backend           *
 ************************************************/

#define SAT2_TONE_PRE  0
#define SAT2_TONE_POST 1

#if BM_SAT2_TONE_SVF

static inline BMMultiLevelSVF *BMSaturator2_tone(BMSaturator2 *This, int which){
    return which == SAT2_TONE_POST ? &This->postTone : &This->preTone;
}


static void sat2Tone_setHighPass6db(BMSaturator2 *This, int which, float fc, size_t level){
    BMMultiLevelSVF_setHighpass6dB(BMSaturator2_tone(This, which), fc, level);
}
static void sat2Tone_setHighPassQ12db(BMSaturator2 *This, int which, float fc, float Q, size_t level){
    BMMultiLevelSVF_setHighpass12dBwithQ(BMSaturator2_tone(This, which), fc, Q, level);
}
static void sat2Tone_setBellWithSkirt(BMSaturator2 *This, int which, float fc, float Q, float bellDb, float skirtDb, size_t level){
    // biquad-compatible Q: same width as BMMultiLevelBiquad_setBellWithSkirt
    BMMultiLevelSVF_setBellWithSkirtBiquadQ(BMSaturator2_tone(This, which), fc, bellDb, skirtDb, Q, level);
}
static void sat2Tone_setBellQ(BMSaturator2 *This, int which, float fc, float Q, float gainDb, size_t level){
    BMMultiLevelSVF_setBellBiquadQ(BMSaturator2_tone(This, which), fc, gainDb, Q, level);
}
static void sat2Tone_setHighShelfAdjustableSlope(BMSaturator2 *This, int which, float fc, float gainDb, float slope, size_t level){
    BMMultiLevelSVF_setHighShelfAdjustableSlope(BMSaturator2_tone(This, which), fc, gainDb, slope, level);
}
static void sat2Tone_setLowShelfAdjustableSlope(BMSaturator2 *This, int which, float fc, float gainDb, float slope, size_t level){
    BMMultiLevelSVF_setLowShelfAdjustableSlope(BMSaturator2_tone(This, which), fc, gainDb, slope, level);
}
static void sat2Tone_setBypass(BMSaturator2 *This, int which, size_t level){
    BMMultiLevelSVF_setBypass(BMSaturator2_tone(This, which), level);
}
static void sat2Tone_setActive(BMSaturator2 *This, int which, bool active, size_t level){
    // unused SVF levels are left at unity gain (see sat2Tone_setBypass)
    (void)This; (void)which; (void)active; (void)level;
}

#else /* biquad tone filters: levels BM_SAT2_EQ_NUM_LEVELS.. of preEQ / postEQ */

static inline BMMultiLevelBiquad *BMSaturator2_tone(BMSaturator2 *This, int which){
    return which == SAT2_TONE_POST ? &This->postEQ : &This->preEQ;
}
static void sat2Tone_setHighPass6db(BMSaturator2 *This, int which, float fc, size_t level){
    BMMultiLevelBiquad_setHighPass6db(BMSaturator2_tone(This, which), fc, BM_SAT2_EQ_NUM_LEVELS + level);
}
static void sat2Tone_setHighPassQ12db(BMSaturator2 *This, int which, float fc, float Q, size_t level){
    BMMultiLevelBiquad_setHighPassQ12db(BMSaturator2_tone(This, which), fc, Q, BM_SAT2_EQ_NUM_LEVELS + level);
}
static void sat2Tone_setBellWithSkirt(BMSaturator2 *This, int which, float fc, float Q, float bellDb, float skirtDb, size_t level){
    BMMultiLevelBiquad_setBellWithSkirt(BMSaturator2_tone(This, which), fc, Q, bellDb, skirtDb, BM_SAT2_EQ_NUM_LEVELS + level);
}
static void sat2Tone_setBellQ(BMSaturator2 *This, int which, float fc, float Q, float gainDb, size_t level){
    BMMultiLevelBiquad_setBellQ(BMSaturator2_tone(This, which), fc, Q, gainDb, BM_SAT2_EQ_NUM_LEVELS + level);
}
static void sat2Tone_setHighShelfAdjustableSlope(BMSaturator2 *This, int which, float fc, float gainDb, float slope, size_t level){
    BMMultiLevelBiquad_setHighShelfAdjustableSlope(BMSaturator2_tone(This, which), fc, gainDb, slope, BM_SAT2_EQ_NUM_LEVELS + level);
}
static void sat2Tone_setLowShelfAdjustableSlope(BMSaturator2 *This, int which, float fc, float gainDb, float slope, size_t level){
    BMMultiLevelBiquad_setLowShelfAdjustableSlope(BMSaturator2_tone(This, which), fc, gainDb, slope, BM_SAT2_EQ_NUM_LEVELS + level);
}
static void sat2Tone_setBypass(BMSaturator2 *This, int which, size_t level){
    BMMultiLevelBiquad_setBypass(BMSaturator2_tone(This, which), BM_SAT2_EQ_NUM_LEVELS + level);
}
static void sat2Tone_setActive(BMSaturator2 *This, int which, bool active, size_t level){
    BMMultiLevelBiquad_setActiveOnLevel(BMSaturator2_tone(This, which), active, BM_SAT2_EQ_NUM_LEVELS + level);
}

#endif



/************************************************
 *       Definitions of amplifier types         *
 ************************************************/

void BMSaturator2_setAmpType(BMSaturator2 *This, enum Sat2AmpType type){
    This->requestedAmpType = type;
}


void BMSaturator2_applyAmpType(BMSaturator2 *This){
    
    // bypass and disable all filters at start
    for(size_t i=0; i < BM_SAT2_PRE_AMP_FILTER_NUM_LEVELS; i++){
        sat2Tone_setBypass(This, SAT2_TONE_PRE, i);
        sat2Tone_setActive(This, SAT2_TONE_PRE, false, i);
    }
    for(size_t i=0; i < BM_SAT2_POST_AMP_FILTER_NUM_LEVELS; i++){
        sat2Tone_setBypass(This, SAT2_TONE_POST, i);
        sat2Tone_setActive(This, SAT2_TONE_POST, false, i);
    }
    
    
    // set the base input and output gain to zero dB at start
    This->baseInputGain = 0.0f;
    This->baseOutputGain = 0.0f;
    
    // set the standard sag configuration
	BMHysteresisLimiter_setSag(&This->gainStage.preAmpLimiter, SAT2_PREAMP_SAG_STANDARD);
	BMHysteresisLimiter_setPowerLimit(&This->gainStage.preAmpLimiter, SAT2_PREAMP_POWER_LIMIT_DB_STANDARD);
	BMHysteresisLimiter_setSag(&This->gainStage.powerAmpLimiter, SAT2_POWERAMP_SAG_STANDARD);
	BMHysteresisLimiter_setPowerLimit(&This->gainStage.powerAmpLimiter, SAT2_POWERAMP_POWER_LIMIT_DB_STANDARD);
	
	// enable the power amp limiter
	BMGainstage_setPowerAmpLimiter(&This->gainStage, true);
   
    
    if(This->ampType == amp_crunch1) {
        // transformer settings
        BMHysteresisLimiter_setPowerLimit(&This->gainStage.preAmpLimiter, -60.0f);
        BMHysteresisLimiter_setPowerLimit(&This->gainStage.powerAmpLimiter, -60.0f);
        BMHysteresisLimiter_setSag(&This->gainStage.preAmpLimiter, 1.58f/4000.0f);
        BMHysteresisLimiter_setSag(&This->gainStage.powerAmpLimiter, 2.0f/4000.0f);
        
        // Preamp EQ settings
        sat2Tone_setHighPass6db(This, SAT2_TONE_PRE, 500.0f, 0);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 300.0f, 1.4f, 5.0f, -30.0f, 1);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 1000.0f, 1.0f, 7.0f, -30.0f, 2);
        sat2Tone_setHighShelfAdjustableSlope(This, SAT2_TONE_PRE, 4100.0f, -17.0f, 0.75f, 3);
        for(size_t i=0; i<4; i++)
            sat2Tone_setActive(This, SAT2_TONE_PRE, true, i);
        
        // Saturator settings
        This->baseInputGain = 54.0f;
        This->baseOutputGain = -18.0f;
        
        // post-amp EQ settings
        sat2Tone_setHighPassQ12db(This, SAT2_TONE_POST, 100.0f, 1.4f, 0);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 140.0f, 0.35f, 12.0f, -15.0f, 1);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 930.0f, 2.0f, -8.0f, 0.0f, 2);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 3600.0f, 0.71f, 9.0f, 0.0f, 3);
        sat2Tone_setHighShelfAdjustableSlope(This, SAT2_TONE_POST, 10000.0f, -10.0f, 1.0f, 4);
        for(size_t i=0; i<5; i++)
            sat2Tone_setActive(This, SAT2_TONE_POST, true, i);
    }
    
    
	
	// fender american standard clean
	if(This->ampType == amp_clean1){
		// transformer settings
        BMHysteresisLimiter_setPowerLimit(&This->gainStage.preAmpLimiter, -60.0f);
        BMHysteresisLimiter_setPowerLimit(&This->gainStage.powerAmpLimiter, -60.0f);
        BMHysteresisLimiter_setSag(&This->gainStage.preAmpLimiter, 1.58f/4000.0f);
        BMHysteresisLimiter_setSag(&This->gainStage.powerAmpLimiter, 2.0f/4000.0f);
		
		// Preamp EQ settings. A bell with a skirt is the skirt gain times a
		// plain bell of (bell - skirt) dB, so the skirts of the original
		// voicing (-17 dB on the 540 Hz bell, -12 dB on the post-amp 85 Hz
		// bell) are carried by the base input and output gains instead
		// (19 - 17 = 2 dB, 1 - 12 = -11 dB): the same response, and the
		// filters alone (which the synth's Pre EQ / Post EQ copy, see
		// SynthAmpEQ) are plain bells. 2026-09-17.
		sat2Tone_setHighPass6db(This, SAT2_TONE_PRE, 135.0f, 0);
		sat2Tone_setBellQ(This, SAT2_TONE_PRE, 510.0f, 2.0f, -17.0f, 1);
		sat2Tone_setBellQ(This, SAT2_TONE_PRE, 540.0f, 0.25f, +17.0f, 2);
		for(size_t i=0; i<3; i++)
			sat2Tone_setActive(This, SAT2_TONE_PRE, true, i);
		
		// Saturator settings
		This->baseInputGain = 2.0f;
		This->baseOutputGain = -11.0f;
		
		// post-amp EQ settings
		sat2Tone_setHighPassQ12db(This, SAT2_TONE_POST, 110.0f, 1.0, 0);
		sat2Tone_setBellQ(This, SAT2_TONE_POST, 85.0f, 0.35f, +10.0f, 1);
		sat2Tone_setBellQ(This, SAT2_TONE_POST, 3000.0f, 0.5f, +8.0f, 2);
        sat2Tone_setHighShelfAdjustableSlope(This, SAT2_TONE_POST, 5800.0f, -14.0f, 0.75f, 3);
		for(size_t i=0; i<4; i++)
			sat2Tone_setActive(This, SAT2_TONE_POST, true, i);
	}
    
    
    
    
    
    // Vox AC30 clean
    if(This->ampType == amp_clean2){
        // transformer settings
        BMHysteresisLimiter_setPowerLimit(&This->gainStage.preAmpLimiter, -60.0f);
        BMHysteresisLimiter_setPowerLimit(&This->gainStage.powerAmpLimiter, -60.0f);
        BMHysteresisLimiter_setSag(&This->gainStage.preAmpLimiter, 5.3f/4000.0f);
        BMHysteresisLimiter_setSag(&This->gainStage.powerAmpLimiter, 5.3f/4000.0f);
        
        // Preamp EQ settings
        sat2Tone_setHighPass6db(This, SAT2_TONE_PRE, 250.0f, 0);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 240.0f, 0.5f, +7.0f, 0.0f, 1);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 900.0f, 2.0f, -16.0f, 0.0f, 2);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 3800.0f, 0.35f, +11.0f, 0.0f, 3);
        for(size_t i=0; i<4; i++)
            sat2Tone_setActive(This, SAT2_TONE_PRE, true, i);
        
        // Saturator settings
        This->baseInputGain = 13.0f;
        This->baseOutputGain = -11.0f;
        
        // post-amp EQ settings
        sat2Tone_setHighPass6db(This, SAT2_TONE_POST, 100.0f, 0);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 230.0f, 0.25f, +8.0f, -6.0f, 1);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 3000.0f, 0.5f, +6.0f, 0.0f, 2);
        for(size_t i=0; i<3; i++)
            sat2Tone_setActive(This, SAT2_TONE_POST, true, i);
    }

	
	
	// spanky clean
	if(This->ampType == amp_clean3){
		// Preamp EQ settings
		sat2Tone_setHighPass6db(This, SAT2_TONE_PRE, 170.0f, 0);
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 290.0f, 1.0f, -9.0f, 0.0f, 1);
		for(size_t i=0; i<2; i++)
			sat2Tone_setActive(This, SAT2_TONE_PRE, true, i);
		
		
		// Saturator settings
		This->baseInputGain = 1.0f;
		This->baseOutputGain = 17.0f;
		
		
		// post-amp EQ settings
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 100.0f, 0.5f, +7.0f, 0.0f, 0);
		sat2Tone_setHighPass6db(This, SAT2_TONE_POST, 140.0f, 1);
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 740.0f, 0.71f, +2.0f, 0.0f, 2);
		sat2Tone_setHighShelfAdjustableSlope(This, SAT2_TONE_POST, 8100.0f, -26.0f, 1.0f, 3);
		for(size_t i=0; i<4; i++)
			sat2Tone_setActive(This, SAT2_TONE_POST, true, i);
	}
	

	// clean humbucker
	if(This->ampType == amp_clean4){
		// transformer settings
		BMHysteresisLimiter_setPowerLimit(&This->gainStage.preAmpLimiter, -50.0f);
		BMHysteresisLimiter_setPowerLimit(&This->gainStage.powerAmpLimiter, -50.0f);
		BMHysteresisLimiter_setSag(&This->gainStage.preAmpLimiter, 2.0f/4000.0f);
		BMHysteresisLimiter_setSag(&This->gainStage.powerAmpLimiter, 2.0f/4000.0f);

		// Preamp EQ settings
		sat2Tone_setHighPass6db(This, SAT2_TONE_PRE, 140.0f, 0);
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 810.0f, 1.4f, -11.0f, 0.0f, 1);
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 1900.0f, 2.0f, -5.0f, 0.0f, 2);
		sat2Tone_setHighShelfAdjustableSlope(This, SAT2_TONE_PRE, 6700.0f, -8.0f, 1.0f, 3);
		for(size_t i=0; i<4; i++)
			sat2Tone_setActive(This, SAT2_TONE_PRE, true, i);

		// Saturator settings
		This->baseInputGain = 16.0f;
		This->baseOutputGain = -3.0f;

		// post-amp EQ settings
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 380.0f, 0.25f, +0.0f, -15.0f, 0);
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 4000.0f, 0.71f, +7.0f, 0.0f, 1);
		for(size_t i=0; i<2; i++)
			sat2Tone_setActive(This, SAT2_TONE_POST, true, i);
	}
	
	
	// slow dancing in a burning room
	if(This->ampType == amp_blues1){
		// transformer settings
		BMHysteresisLimiter_setPowerLimit(&This->gainStage.preAmpLimiter, -60.0f);
		BMHysteresisLimiter_setPowerLimit(&This->gainStage.powerAmpLimiter, -60.0f);
		BMHysteresisLimiter_setSag(&This->gainStage.preAmpLimiter, 4.0f/4000.0f);
		BMHysteresisLimiter_setSag(&This->gainStage.powerAmpLimiter, 4.0f/4000.0f);
		
		// Preamp EQ settings
		sat2Tone_setHighPass6db(This, SAT2_TONE_PRE, 170.0f, 0);
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 530.0f, 2.0f, -25.0f, 0.0f, 1);
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 540.0f, 0.5f, +2.0f, -20.0f, 2);
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_PRE, 540.0f, 0.5f, +9.0f, -25.0f, 3);
		for(size_t i=0; i<4; i++)
			sat2Tone_setActive(This, SAT2_TONE_PRE, true, i);
		
		
		// Saturator settings
		This->baseInputGain = 27.0f;
		This->baseOutputGain = -30.0f;
		
		// post-amp EQ settings
		sat2Tone_setHighPassQ12db(This, SAT2_TONE_POST, 100.0f, 1.4, 0);
		sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 420.0f, 0.25f, 26.0f, 0.0f, 1);
        sat2Tone_setBellWithSkirt(This, SAT2_TONE_POST, 6500.0f, 1.0f, +3.0f, 0.0f, 2);
		for(size_t i=0; i<3; i++)
			sat2Tone_setActive(This, SAT2_TONE_POST, true, i);
	}

    // update the input and output gain
    BMSaturator2_setInputGain(This, This->preEQState.gain);
    BMSaturator2_setOutputGain(This, This->postEQState.gain);

	BMSaturator2_setStereoCabWetMixDb(This, This->stereoCabWetMixDb);

	// mark the amp type change done
	This->ampType = This->requestedAmpType;
}





/*!
 *BMSaturator2_setAmpNumber
 *
 * @param This pointer to an initialised struct
 * @param ampNumber this is the amp type as a number in [0,numAmpTypes - 1]
 */
void BMSaturator2_setAmpNumber(BMSaturator2 *This, size_t ampNumber){
    printf("BMSaturator2_setAmpNumber %zu\n",ampNumber);
	BMSaturator2_setAmpType(This, (enum Sat2AmpType)(ampNumber+1));
}



/*!
 *BMSaturator2_getNumAmpTypes
 *
 * @returns the number of available amplifier types
 */
size_t BMSaturator2_getNumAmpTypes(BMSaturator2* This){
	return amp_last - amp_first;
}


/*!
 *BMSaturator2_getAmpNameForTypeNumber
 *
 * @abstract ampName is a null-terminatred character array of length 100.
 */
void BMSaturator2_getAmpNameForTypeNumber(BMSaturator2* This, size_t ampType, char* ampName){
	
	if(ampType + 1 == (size_t)amp_clean1){
		char *s = "clean - american standard";
		strcpy(ampName, s);
	}

    if(ampType + 1 == (size_t)amp_clean2){
        char *s = "clean - british chime";
        strcpy(ampName, s);
    }
    
	if(ampType + 1 == (size_t)amp_clean3){
		char *s = "clean - punchy country";
		strcpy(ampName, s);
	}
	
	if(ampType + 1 == (size_t)amp_clean4){
		char *s = "clean - sweet humbuckers";
		strcpy(ampName, s);
	}
	
	if(ampType + 1 == (size_t)amp_blues1){
		char *s = "blues - slow dancing";
		strcpy(ampName, s);
	}
	
	if(ampType + 1 == (size_t)amp_crunch1){
		char *s = "crunch";
		strcpy(ampName, s);
	}
}
