//
//  BMSaturator2.h
//  BMAUSaturator
//
//  Created by hans anderson on 7/5/19.
//  Copyright © 2019 bluemangoo. All rights reserved.
//

#ifndef BMSaturator2_h
#define BMSaturator2_h

#include <stdio.h>
#include "BMQuadraticRectifier.h"
#include "../Filters/BMMultiLevelBiquad.h"
#include "../DelayAndReverb/BMShortSimpleDelay.h"
#include "../SampleRate/BMUpsampler.h"
#include "../SampleRate/BMDownsampler.h"
#include "../OtherEffects/BMSmoothGain.h"
#include "../Filters/BMFirstOrderDirectForm.h"
#include "../DelayAndReverb/BMMonoToStereo.h"
#include "../CrossPlatform/BMLock.h"
#include "BMHysteresisLimiter.h"
#include "../DelayAndReverb/BMReverb.h"
#include "../OtherEffects/BMSmoothSwitch.h"
#include "BMPeakLimiter.h"
#include "../Filters/BMMultiLevelSVF.h"


#define BM_SAT2_DESIGN_SAMPLE_RATE 48000.0f
#define BM_SAT2_AA_FILTER_DEFAULT_FC 4000.0f
// The gain stage's audio-path delay is the control-signal path's group
// delay, measured at the AA filters' cutoff frequency (so it follows the
// cutoff slider), times this constant. The multiply of the control signal
// by the audio makes even small misalignments matter, and the group delay
// of these filters varies a lot with frequency, so the frequency at which
// it is measured is a voicing choice. History: the original asked for
// 20 kHz but BMMultiLevelBiquad_groupDelay lacked a factor pi and returned
// the value at 6.4 kHz, 39.48 samples at 16 x 48 kHz with the 4 kHz
// cutoff; that delay was liked (the control signal is meant to lag a
// little). The exact group delay at the 4 kHz cutoff is 54.03 samples;
// 0.73067 (= 39.476 / 54.027) reproduced the old 39 samples. Set to 1.2 on
// 2026-09-12 by ear (65 samples at the default cutoff: the control signal
// now leads slightly). At other cutoffs the delay is the exact value at
// that cutoff times the scale. Adjustable live from the "Delay scale"
// slider (BMSaturator2_setGroupDelayCompensationScale).
#define BM_SAT2_GROUP_DELAY_COMPENSATION_SCALE 1.2f
#define BM_SAT2_NUM_BUFFERS 16   // 12 for the gain stage's signals + 4 scratch for the limiter filter mirror
#define BM_SAT2_PRE_AMP_FILTER_NUM_LEVELS 5
#define BM_SAT2_POST_AMP_FILTER_NUM_LEVELS 6
#define BM_SAT2_EQ_NUM_LEVELS 5
// most caller-owned filters per position, BMSaturator2_setExternalToneFilters
#define BM_SAT2_MAX_EXTERNAL_TONE_FILTERS 4

// Build options for this copy (Midi Tuning Synth). The synth does not use the
// saturator's 5-band EQs or its built-in room reverb, so both are removed from
// the signal path: the EQ biquad levels are deactivated (vDSP skips inactive
// sections entirely) and the reverb is neither initialised nor processed.
// Set to 1 to restore the original plugin behaviour.
#define BM_SAT2_INCLUDE_EQ 0
#define BM_SAT2_INCLUDE_REVERB 0

// Tone-filter backend. The amp's pre- and post-amp tone filters (the
// highpass / bell / shelf cascades that voice each amp type) were biquad
// levels in the same BMMultiLevelBiquad cascades as the EQ. With
// BM_SAT2_TONE_SVF = 1 they are instead realised on BMMultiLevelSVF
// (Cytomic trapezoidal state-variable filters) with the same transfer
// functions: bells are converted through BMMultiLevelBiquad_QToBW so the
// widths match, and the SVF shelf / 6 dB setters are exact equivalents of the
// biquad ones. Reason: the float32 direct-form biquads produce a
// signal-dependent low-frequency noise floor (~-107 dB) from their near-DC
// sections; the SVF is about 50 dB cleaner. Set to 0 for the original biquads.
#define BM_SAT2_TONE_SVF 1

// Gain-stage arithmetic. The gain stage (rectifier, anti-aliasing lowpasses,
// two hysteresis limiters, control-signal divide) runs at the oversampled
// rate. In float32 its recursive parts, above all the limiters' charge
// recursion, leave a signal-proportional rounding-noise floor about 156 dB
// below the signal. With this option the stage runs in double after the
// rectifier (vDSP double biquads for the anti-aliasing filters, double
// charge states, double control-signal arithmetic) and reaches the float32
// output quantisation floor (about -180 dB). Measured cost: no slower than
// the float version on Apple silicon (vDSP's double biquads are as fast as
// the float ones here). Set to 0 for the original float32 stage.
#ifndef BM_SAT2_DOUBLE_GAINSTAGE
#define BM_SAT2_DOUBLE_GAINSTAGE 1
#endif


//
//typedef enum Sat2AmpType {amp_first,
//					  amp_clean1, amp_clean2, amp_clean3, amp_clean4,
//					  amp_blues1, amp_blues2,
//				      amp_crunch1, amp_crunch2, amp_crunch3,
//					  amp_brown,
//					  amp_smoothSolo1, amp_smoothSolo2,
//					  amp_metalBlack1,
//					  amp_hiGainRhythm1, amp_hiGainRhythm2,
//	                  amp_hiGainLead1, amp_hiGainLead2,
//                      amp_acoustic1,
//					  amp_fullSpectrum0dB, amp_fullSpectrum15dB, amp_fullSpectrum30dB, amp_fullSpectrum45dB,
//					  amp_fullSpectrum30dBsingleStage, amp_fullSpectrum30dBDHC, amp_fullSpectrum30dBRAT, amp_fullSpectrum30dBCOMP, amp_fullSpectrum30dBAC,
//					  amp_last
//} Sat2AmpType;


typedef enum Sat2AmpType {amp_first,
					  amp_clean1, amp_clean2, amp_clean3, amp_clean4,
					  amp_blues1, 
				      amp_crunch1, 
					  amp_last
} Sat2AmpType;



typedef enum Sat2RoomType {
	s2room_none, s2room_small, s2room_medium, s2room_large
} Sat2RoomType;



typedef struct BMGainstage {
    // The three 6 dB anti-aliasing lowpasses on the control-signal path
    // (BMGainstage_setAAFilterFc, default 4 kHz), on BMMultiLevelSVF since
    // 2026-09-12 (they were vDSP biquadm filters): the SVF takes a cutoff
    // change with its state preserved, so the cutoff can be moved live from
    // a slider, while vDSP_biquadm_SetCoefficientsDoubleD resets the delay
    // values (a burst on the control signal). Each is a 2-channel SVF: index
    // 0 filters (pos, neg) in mono and (posL, posR) in stereo, index 1
    // (negL, negR) in stereo. The double-precision gain stage uses
    // BMMultiLevelSVF_processBufferStereoD (double state); the float stage
    // the float process.
    BMMultiLevelSVF AAFilter1[2], AAFilter2_1[2], AAFilter2_2[2];
    size_t numAAFilterPairs;   // 1 mono, 2 stereo
    // Never processes audio: a biquad set to the same 6 dB lowpass, used only
    // so that the group-delay compensation is computed by
    // BMMultiLevelBiquad_groupDelay for every filter on the path, as it was
    // when these filters were biquads (BMGainstage_lowpass6dBGroupDelay is
    // the same number in closed form; the tests check they agree).
    BMMultiLevelBiquad aaFilterGroupDelayShadow;
    // multiplier on the measured group delay, BM_SAT2_GROUP_DELAY_COMPENSATION_SCALE
    // by default; BMSaturator2_setGroupDelayCompensationScale (UI slider)
    float groupDelayScale;
    BMQuadraticRectifier rectifier;
    BMShortSimpleDelay delay;
	BMHysteresisLimiter preAmpLimiter, powerAmpLimiter;
	bool usePowerAmpLimiter;
	float aaFilterFc;
    // Mirror of the limiters' internal anti-aliasing filters on the lowpassed
    // path. The waveshaped path passes through the preamp limiter's filter and
    // the power amp limiter's filter (two real poles at 20 kHz each); the
    // lowpassed path did not, so the control signal ws / lp did not cancel in
    // the linear regime: the numerator lagged the denominator by the two
    // filters' group delay (about 24 samples at 16 x 48 kHz). This filter
    // applies the same sections to the lowpassed path. Its coefficients are
    // never set directly: BMGainstage_updateLimiterFilterMirror copies them
    // from the limiters' own filters, so the two cannot drift apart. The
    // filter always runs (into scratch buffers) so that its state is current
    // whenever the switch is turned on; mirrorLimiterFilters only selects
    // which signal goes on to the division, which makes the switch click-free.
    BMMultiLevelBiquad limiterFilterMirror;
    vDSP_biquadm_SetupD limiterFilterMirrorD;
    bool mirrorLimiterFilters;
} BMGainstage;


typedef struct BMSat2EQConfig{
	float oneSlope, fiveSlope;
	float Q [5];
	float fc [5];
	bool oneIsShelf,fiveIsShelf;
} BMSat2EQConfig;


typedef struct BMSat2EQState {
	float gain;
	float eqGain [5];
} BMSat2EQState;


typedef struct BMSaturator2 {
    BMGainstage gainStage;
    BMMultiLevelBiquad preEQ, postEQ;
#if BM_SAT2_TONE_SVF
    BMMultiLevelSVF preTone, postTone;
#endif
    BMUpsampler upsampler;
    BMDownsampler downsampler;
    BMSmoothGain inputGain, outputGain;
	BMMonoToStereo monoToStereo;
	// BMSaturator2_setEarlyReflections: kept here and reapplied whenever the
	// decorrelator is re-initialised (amp type, stereo or oversampling change)
	bool earlyReflectionsCustom;
	float erMinDelayS, erMaxDelayS, erRT60S, erDryGain, erWetTapGain, erBassCrossoverHz, erTrebleCrossoverHz;
	size_t erNumWetTaps;
	// BMSaturator2_requestEarlyReflections: a new model is waiting for the audio thread
	bool erPending;
	// BMSaturator2_setEarlyReflectionsConvolved: the model runs as a convolution.
	// The lock keeps a control thread's BMSaturator2_installEarlyReflectionsConvolution
	// and the audio thread's re-initialisation of the cabinet apart.
	bool erConvolved;
	BMLock erConvolutionLock;
	// the side bands' reflections (BMSaturator2_setSideBandEarlyReflections; index: enum
	// BMMonoToStereoSideBand), kept and reapplied likewise
	struct {
		bool custom, pending, bypassed;
		float minDelayS, maxDelayS, rt60S, wetTapGain;
		size_t numWetTaps;
		uint32_t seed;   // 0: the band's own
	} erSide[2];
	BMSat2EQConfig preEQConfig, postEQConfig;
	BMSat2EQState preEQState, postEQState;
	BMReverb reverb;
	Sat2AmpType ampType, requestedAmpType;
    BMSmoothSwitch smoothSwitch;
	BMPeakLimiter peakLimiter;
    size_t oversamplingFactor, requestedOversamplingFactor;
    float sampleRate, baseInputGain, baseOutputGain, stereoCabWetMixDb;
    float *buffers [BM_SAT2_NUM_BUFFERS];
    double *dbuffers[BM_SAT2_NUM_BUFFERS];   // double scratch for the gain stage
    bool isStereo, requestedStereo, useStereoCab, useReverb, usePeakLimiter, initCompleted;
    // BMSaturator2_setLimiterFilterMirror. Kept here, not only in the gain
    // stage, because the gain stage is re-initialised on every amp type,
    // stereo or oversampling change and would otherwise fall back to on.
    bool useLimiterFilterMirror;
    // BMSaturator2_setAAFilterFc: the cutoff of the gain stage's three 6 dB
    // anti-aliasing lowpasses, reapplied after every gain stage re-init.
    float aaFilterFcSetting;
    float groupDelayScaleSetting;   // BMSaturator2_setGroupDelayCompensationScale, reapplied after re-init
    // BMSaturator2_setExternalToneFilters: filters the caller owns that run
    // in series in place of the built-in preTone / postTone cascades when
    // set. Not touched by BMSaturator2_init or by a re-init, so they can be
    // attached before the saturator is initialised.
    BMMultiLevelSVF *externalPreTone[BM_SAT2_MAX_EXTERNAL_TONE_FILTERS];
    BMMultiLevelSVF *externalPostTone[BM_SAT2_MAX_EXTERNAL_TONE_FILTERS];
    size_t numExternalPreTone, numExternalPostTone;
    // BMSaturator2_setGainStageBypass: the gain stage's output is crossfaded
    // against its input (at the oversampled rate, so nothing else in the
    // chain moves). gainStageMix is 1 with the gain stage in, 0 bypassed;
    // it moves gainStageMixStep per oversampled sample toward the target.
    bool gainStageBypass;
    float gainStageMix, gainStageMixStep;
    // BMSaturator2_setGainStageBypassGainDb: linear gain on the bypassed signal
    float gainStageBypassGain;
} BMSaturator2;




/*!
 *BMSaturator2_processBuffer
 *
 * @param This  pointer to an initialized saturator struct
 * @param inL   left input with length = numSamples
 * @param inR   right input with length = numSamples
 * @param outL  left output with length = numSamples
 * @param outR  right output with length = numSamples
 */
void BMSaturator2_processBuffer(BMSaturator2 *This,
								const float *inL, const float *inR,
								float *outL, float *outR,
								size_t numSamples);


/*!
 *BMSaturator2_init
 *
 * @param This       pointer to a saturator struct
 * @param sampleRate the gain setting in decibels
 * @param oversampleFactor this must be a multiple of 2
 * @param isStereo   set to true if you intend to process audio in stereo
 */
void BMSaturator2_init(BMSaturator2 *This, float sampleRate, size_t oversampleFactor, bool isStereo);



/*!
 *BMSaturator2_setAmpNumber
 *
 * @param This pointer to an initialised struct
 * @param ampNumber this is the amp type as a number in [0,numAmpTypes - 1]
 */
void BMSaturator2_setAmpNumber(BMSaturator2 *This, size_t ampNumber);



void BMSaturator2_free(BMSaturator2 *This);



/*!
 *BMSaturator2_setInputGain
 *
 * @param This   pointer to an initialized saturator struct
 * @param gainDb the gain setting in decibels
 */
void BMSaturator2_setInputGain(BMSaturator2 *This, float gainDb);



/*!
 *BMSaturator2_setInputEQ1
 */
/*!
 *BMSaturator2_setEQActive
 *
 * @abstract enable or disable the 5-band pre and post EQs. When disabled their
 * biquad levels are deactivated so they cost no CPU; the amp's own tone
 * filters in the same cascades are unaffected.
 */
void BMSaturator2_setEQActive(BMSaturator2 *This, bool active);

void BMSaturator2_setInputEQ1(BMSaturator2 *This, float gainDb);
/*!
 *BMSaturator2_setInputEQ2
 */
void BMSaturator2_setInputEQ2(BMSaturator2 *This, float gainDb);
/*!
 *BMSaturator2_setInputEQ3
 */
void BMSaturator2_setInputEQ3(BMSaturator2 *This, float gainDb);
/*!
 *BMSaturator2_setInputEQ4
 */
void BMSaturator2_setInputEQ4(BMSaturator2 *This, float gainDb);
/*!
 *BMSaturator2_setInputEQ5
 */
void BMSaturator2_setInputEQ5(BMSaturator2 *This, float gainDb);



/*!
 *BMSaturator2_setOutputEQ1
 */
void BMSaturator2_setOutputEQ1(BMSaturator2 *This, float gainDb);
/*!
 *BMSaturator2_setOutputEQ2
 */
void BMSaturator2_setOutputEQ2(BMSaturator2 *This, float gainDb);
/*!
 *BMSaturator2_setOutputEQ3
 */
void BMSaturator2_setOutputEQ3(BMSaturator2 *This, float gainDb);
/*!
 *BMSaturator2_setOutputEQ4
 */
void BMSaturator2_setOutputEQ4(BMSaturator2 *This, float gainDb);
/*!
 *BMSaturator2_setOutputEQ5
 */
void BMSaturator2_setOutputEQ5(BMSaturator2 *This, float gainDb);





/*!
 *BMSaturator2_setOutputGain
 *
 * @param This   pointer to an initialized saturator struct
 * @param gainDb the gain setting in decibels
 */
void BMSaturator2_setOutputGain(BMSaturator2 *This, float gainDb);






/*!
 *BMSaturator2_setStereoCabinet
 *
 * @param This      pointer to an initialized saturator struct
 * @param useStereo set true to enable the stereo cabinet simulation
 */
void BMSaturator2_setStereoCabinet(BMSaturator2 *This, bool useStereo);



/*!
 *BMSaturator2_setEarlyReflections
 *
 * @abstract Configure the speaker cabinet's decorrelator as an early-reflection
 * model (BMMonoToStereo_setEarlyReflections): a dry tap and numWetTaps velvet
 * taps per channel between minDelayMs and maxDelayMs after it, RT60
 * rt60Seconds, gains in dB relative to unity; wetTapGainDb is the energy of
 * the whole burst of wet taps (see BMMonoToStereo_setEarlyReflections), so
 * it is independent of the other settings. bassCrossoverHz > 0 keeps the
 * band below it dry and mono and decorrelates only the band above; 0 is full band.
 * Overrides BMSaturator2_setStereoCabWetMixDb. Survives the amp's internal
 * re-initialisations. Call before processing starts.
 */
/*!
 *BMSaturator2_setEarlyReflectionsWetTapGainDb
 *
 * @abstract The level of the early reflections set by
 * BMSaturator2_setEarlyReflections, changed while the amp runs, for a wet
 * control: the same reflections, louder or quieter. Survives the amp's
 * internal re-initialisations like the rest of the model.
 */
void BMSaturator2_setEarlyReflectionsWetTapGainDb(BMSaturator2 *This, float wetTapGainDb);

/*!
 *BMSaturator2_setSideBandEarlyReflections
 *
 * @abstract Early reflections for the bass or the treble band of the
 * three-band early-reflection model (BMMonoToStereo_setSideBandEarlyReflections):
 * the same kind of model as the mid band's, with its own first and last
 * delay, tap count, decay and level; the dry gain is the mid band's. bypassed
 * leaves the band dry until BMSaturator2_setSideBandEarlyReflectionsBypass
 * says otherwise. Requires BMSaturator2_setEarlyReflections with both
 * crossovers. Call before processing starts; while the amp runs, use
 * BMSaturator2_requestSideBandEarlyReflections.
 */
void BMSaturator2_setSideBandEarlyReflections(BMSaturator2 *This, enum BMMonoToStereoSideBand band,
											  float minDelayMs, float maxDelayMs, size_t numWetTaps,
											  float rt60Seconds, float wetTapGainDb, bool bypassed);

/*!
 *BMSaturator2_requestSideBandEarlyReflections
 *
 * @abstract The same while the amp runs: stored, and built by the audio
 * thread behind a fade of the amp's output, like
 * BMSaturator2_requestEarlyReflections.
 */
void BMSaturator2_requestSideBandEarlyReflections(BMSaturator2 *This, enum BMMonoToStereoSideBand band,
												  float minDelayMs, float maxDelayMs, size_t numWetTaps,
												  float rt60Seconds, float wetTapGainDb);

/*! A side band's reflections in or out (out: dry), at once, while the amp runs. */
void BMSaturator2_setSideBandEarlyReflectionsBypass(BMSaturator2 *This, enum BMMonoToStereoSideBand band, bool bypassed);

/*! A side band's reflections' level while the amp runs, seamlessly. */
void BMSaturator2_setSideBandEarlyReflectionsWetTapGainDb(BMSaturator2 *This, enum BMMonoToStereoSideBand band, float wetTapGainDb);

/*!
 *BMSaturator2_setEarlyReflectionsCrossoversHz
 *
 * @abstract The two crossovers of the three-band early-reflection model,
 * moved while the amp runs, seamlessly (BMMonoToStereo_setCrossoversHz). The
 * model must have been set with both. Survives the amp's internal
 * re-initialisations like the rest of the model.
 */
void BMSaturator2_setEarlyReflectionsCrossoversHz(BMSaturator2 *This, float bassCrossoverHz, float trebleCrossoverHz);

/*!
 *BMSaturator2_setSideBandEarlyReflectionsSeed
 *
 * @abstract The seed of a side band's taps (BMMonoToStereo_setSideBandSeed),
 * 0 for the band's own. Stored, and used the next time the band's reflections
 * are built: by the functions that set or request them, and by the amp's
 * internal re-initialisations. Set it before them.
 */
void BMSaturator2_setSideBandEarlyReflectionsSeed(BMSaturator2 *This, enum BMMonoToStereoSideBand band, uint32_t seed);

/*!
 *BMSaturator2_setEarlyReflectionsConvolved
 *
 * @abstract Run the early-reflection model (all three bands, their crossovers
 * and gains) as a convolution of the amp's mono output with the model's own
 * left and right impulse responses (BMMonoToStereo_bakeConvolution): the same
 * output to within rounding, cheaper, and at a cost that does not depend on
 * the number of taps.
 *
 * While it is on, the functions that change the model (the request functions,
 * the wet tap gains, the side bands' bypasses, the crossovers) only store
 * what they are given, for the amp's internal re-initialisations, which
 * measure the convolution again from the stored model. To change what is
 * heard, measure the changed model (BMMonoToStereo_measureModel, on any
 * thread) and put it in with BMSaturator2_installEarlyReflectionsConvolution,
 * which is click-free and leaves the rest of the amp alone; and give the same
 * settings to those functions.
 * Call after BMSaturator2_setEarlyReflections and the side bands' setters,
 * before processing starts.
 */
void BMSaturator2_setEarlyReflectionsConvolved(BMSaturator2 *This, bool convolved);

/*!
 *BMSaturator2_installEarlyReflectionsConvolution
 *
 * @abstract BMMonoToStereo_installConvolution on the amp's cabinet (see there
 * for the arguments and the return value), kept apart from the amp's own
 * re-initialisation of the cabinet. One control thread at a time.
 */
bool BMSaturator2_installEarlyReflectionsConvolution(BMSaturator2 *This, const float *left, const float *right, size_t length, bool immediate);

/*!
 *BMSaturator2_requestEarlyReflections
 *
 * @abstract BMSaturator2_setEarlyReflections for while the amp runs: the
 * model is rebuilt (memory and all), which cannot happen under the audio
 * thread's feet, so the new settings are stored here and the audio thread
 * applies them the way it applies an amp type change: it fades the amp's
 * output out, rebuilds, fades back in. A short dip per call, so call it when
 * a control is released, not while it moves. (For the reflections' level
 * alone, BMSaturator2_setEarlyReflectionsWetTapGainDb is seamless.)
 */
void BMSaturator2_requestEarlyReflections(BMSaturator2 *This,
										  float minDelayMs, float maxDelayMs, size_t numWetTaps,
										  float rt60Seconds, float dryGainDb, float wetTapGainDb,
										  float bassCrossoverHz, float trebleCrossoverHz);

void BMSaturator2_setEarlyReflections(BMSaturator2 *This,
									  float minDelayMs, float maxDelayMs, size_t numWetTaps,
									  float rt60Seconds, float dryGainDb, float wetTapGainDb,
										  float bassCrossoverHz, float trebleCrossoverHz);


/*!
 *BMSaturator2_setStereoCabWetMixDb
 *
 * @abstract Wet mix of the stereo speaker cabinet (BMMonoToStereo's velvet
 * noise decorrelator), in dB <= 0 of the decorrelator's constant-power
 * 0..1 wet scale: 0 dB is fully wet, -6.02 dB is 0.5. The amp types leave
 * the current value alone; BMSaturator2_setRoomType resets it to the
 * default (-0.1 dB), so call this after that.
 */
void BMSaturator2_setStereoCabWetMixDb(BMSaturator2 *This, float wetMixDb);





/*!
*BMSaturator2_enableStereoCabinet
*
* @param This   pointer to an initialized saturator struct
* @param roomType set the room reverb type
*/
void BMSaturator2_setRoomType(BMSaturator2 *This, enum Sat2RoomType roomType);




/*!
 *BMSaturator2_setStereo
 *
 * @param This   pointer to an initialized saturator struct
 * @param isStereo true for stereo, false for mono
 */
void BMSaturator2_setStereo(BMSaturator2 *This, bool isStereo);





/*!
 *BMSaturator2_setOversample
 *
 * @param This   pointer to an initialized saturator struct
 * @param oversampleFactor set to 1 for no oversampling; set > 1 for oversampling. must be a power of 2
 */
void BMSaturator2_setOversample(BMSaturator2 *This, size_t oversampleFactor);



void BMSaturator2_setOutputMid(BMSaturator2* This, float gainDb);

void BMSaturator2_setOutputTreble(BMSaturator2* This, float gainDb);


/*!
 *BMSaturator2_getNumAmpTypes
 *
 * @returns the number of available amplifier types
 */
size_t BMSaturator2_getNumAmpTypes(BMSaturator2* This);


/*!
 *BMSaturator2_getAmpNameForTypeNumber
 *
 * @abstract ampName is a null-terminatred character array of length 100.
 */
void BMSaturator2_getAmpNameForTypeNumber(BMSaturator2* This, size_t ampType, char* ampName);



/*!
 *BMSaturator2_getLatencyInSeconds
 *
 * @returns the processing latency in seconds. Note that the actual latency changes depending on the oversampling rate. Because the host app is not required to track changes in latency when plugin parameters change, it can not update the latency depending on oversample factor. The value returned by this function is the latency at the current oversampling rate.
 */
float BMSaturator2_getLatencyInSeconds(BMSaturator2 *This);




/*!
 *BMSaturator2_setPeakLimiter
 */
void BMSaturator2_setPeakLimiter(BMSaturator2 *This, bool limiterOn);



/*!
 *BMSaturator2_setExternalToneFilters
 *
 * @abstract Run the caller's filters in place of the amp's built-in tone
 * filters: the `pre` filters, in series, where the pre-amp cascade runs
 * (on the input, before the input gain and the upsampler) and the `post`
 * filters, in series, where the post-amp cascade runs (after the
 * downsampler, before the output gain, the speaker cabinet and the peak
 * limiter). A count of 0 keeps the built-in cascade at that position; at
 * most BM_SAT2_MAX_EXTERNAL_TONE_FILTERS per position (the pointer arrays
 * are copied). The filters must be initialised with the saturator's
 * channel count (BMMultiLevelSVF_init isStereo = the saturator's isStereo)
 * and are processed in chunks of at most BM_BUFFER_CHUNK_SIZE samples, so
 * their filter sweep mode may be on. The caller keeps ownership and sets
 * their coefficients; the amp type's voicing is not applied to them. The
 * setting survives BMSaturator2_init and every re-init (amp type, stereo
 * or oversampling change), and may be made before init. Setting or
 * clearing while audio runs is not click-free.
 */
void BMSaturator2_setExternalToneFilters(BMSaturator2 *This,
                                         BMMultiLevelSVF *const *pre, size_t numPre,
                                         BMMultiLevelSVF *const *post, size_t numPost);



/*!
 *BMSaturator2_setGainStageBypass
 *
 * @abstract Take the gain stage (the saturating part: rectifier, control
 * signal, hysteresis limiters) out of the signal path, leaving the tone
 * filters, the gains, the oversampling, the speaker cabinet and the peak
 * limiter as they are. Click-free: the gain stage keeps running and its
 * output is crossfaded against its input over about 20 ms at the
 * oversampled rate, so re-engaging it is instant and the rest of the chain
 * sees no change of latency. The input gain (including the amp type's base
 * gain) still applies to the bypassed signal. The gain stage's small-signal
 * gain is not unity, so the bypassed signal has a gain of its own to match
 * it (BMSaturator2_setGainStageBypassGainDb). Safe to call from the control
 * thread.
 */
void BMSaturator2_setGainStageBypass(BMSaturator2 *This, bool bypassed);
bool BMSaturator2_gainStageBypassed(const BMSaturator2 *This);


/*!
 *BMSaturator2_setGainStageBypassGainDb
 *
 * @abstract Gain, in dB, on the signal that replaces the gain stage's output
 * while it is bypassed. Default 0. Set it to the gain stage's small-signal
 * gain and switching the bypass leaves the level of a signal below the
 * saturation unchanged. Measured 2026-09-18 for amp_clean1 (16x, 48 kHz):
 * -3.55 dB, the same within 0.03 dB from 110 Hz to 3 kHz and at every level
 * below -25 dBFS RMS after the input gain; louder signals are compressed by
 * the saturation and come out lower than the bypassed signal (-0.4 dB more
 * at -15 dBFS, -4 dB more at -3 dBFS). Not smoothed: set it before playback.
 */
void BMSaturator2_setGainStageBypassGainDb(BMSaturator2 *This, float gainDb);



/*!
 *BMSaturator2_setLimiterFilterMirror
 *
 * @abstract Apply the hysteresis limiters' internal anti-aliasing filters to
 * the lowpassed path of the gain stage as well, so that the control-signal
 * division ws / lp cancels the same linear filtering on both sides. Default
 * on. Off reproduces the original Gain Stage Vintage Clean behaviour, where
 * the numerator of the division lags the denominator by those filters' group
 * delay. Click-free; safe to call from the control thread while audio runs.
 */
void BMSaturator2_setLimiterFilterMirror(BMSaturator2 *This, bool on);



/*!
 *BMSaturator2_setAAFilterFc
 *
 * @abstract Cutoff, in Hz, of the gain stage's three 6 dB anti-aliasing
 * lowpasses (AAFilter1, AAFilter2_1, AAFilter2_2; default
 * BM_SAT2_AA_FILTER_DEFAULT_FC = 4 kHz). These filters shape the control
 * signal, not the audio: below the cutoff the stage waveshapes, above it
 * it compresses. Clamped to [200, 20000]. Safe to call from the control
 * thread while audio runs: the SVFs take the new coefficients at the start
 * of their next buffer with the filter state preserved. The group-delay
 * compensation delay is retargeted too; when its integer length changes it
 * is re-initialised with zeros, a 50 us dropout, accepted for this testing
 * control (see BMGainstage_setAAFilterFcRealtime). The limiters' own 20 kHz
 * filters and their mirror are not affected.
 */
void BMSaturator2_setAAFilterFc(BMSaturator2 *This, float fc);



/*!
 *BMSaturator2_setGroupDelayCompensationScale
 *
 * @abstract Multiplier on the control-signal path's group delay (measured
 * at the AA cutoff) that sets the audio-path compensation delay. Default
 * BM_SAT2_GROUP_DELAY_COMPENSATION_SCALE. 1.0 would be exact alignment at
 * the cutoff; less makes the control signal lag. Live: retargets the delay,
 * which re-initialises with zeros when its length changes (a testing
 * control, see BMGainstage_setAAFilterFcRealtime).
 */
void BMSaturator2_setGroupDelayCompensationScale(BMSaturator2 *This, float scale);



/*!
 *BMGainstage_setLimiterAAFilterFc
 *
 * @abstract Set the cutoff of the anti-aliasing filters inside both
 * hysteresis limiters of the gain stage and keep their mirror on the
 * lowpassed path in step. Configuration time only.
 */
void BMGainstage_setLimiterAAFilterFc(BMGainstage *This, float fc);



/*!
 *BMGainstage_setAAFilterFc
 *
 * @abstract Cutoff of the gain stage's own three 6 dB anti-aliasing lowpasses
 * (AAFilter1, AAFilter2_1, AAFilter2_2). Recomputes the group-delay
 * compensation and refreshes the limiter filter mirror. Configuration time only.
 */
void BMGainstage_setAAFilterFc(BMGainstage *This, float fc);



/*!
 *BMGainstage_lowpass6dBGroupDelay
 *
 * @abstract Group delay, in samples, at frequency f of the 6 dB lowpass
 * (bilinear one-pole, the response of BMMultiLevelBiquad_setLowPass6db and
 * BMMultiLevelSVF_setLowpass6dB) with cutoff fc at sample rate sampleRate.
 * Exact (checked against a numerical phase derivative), and equal to
 * BMMultiLevelBiquad_groupDelay of the same filter since that function's
 * missing factor pi was fixed (2026-09-12). The gain stage's compensation
 * uses the biquad function through a shadow filter and scales the result by
 * BM_SAT2_GROUP_DELAY_COMPENSATION_SCALE, see there.
 */
float BMGainstage_lowpass6dBGroupDelay(float fc, float sampleRate, float f);



/*!
 *BMGainstage_limiterFilterMirrorIsInStep
 *
 * @returns true if the mirror filter's coefficients are identical to the
 * limiters' anti-aliasing filter coefficients (used by the tests).
 */
bool BMGainstage_limiterFilterMirrorIsInStep(BMGainstage *This);



/*!
 *BMSaturator2_getLimiterClipState
 *
 * @returns true if the limiter was clipping during the last buffer
 */
bool BMSaturator2_getLimiterClipState(BMSaturator2 *This);



/*!
 *BMSaturator2_getLatencyInSeconds
 *
 * @returns the estimated latency in seconds. Because of filter group delay, the actual latency is frequency dependent. 
 */
float BMSaturator2_getLatencyInSeconds(BMSaturator2 *This);


#endif /* BMSaturator2_h */
