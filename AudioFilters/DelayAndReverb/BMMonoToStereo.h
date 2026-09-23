//
//  BMMonoToStereo.h
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 10/9/19.
//  Anyone may use this file without restrictions of any kind
//

#ifndef BMMonoToStereo_h
#define BMMonoToStereo_h

#include <stdio.h>
#include "BMVelvetNoiseDecorrelator.h"
#include "../Filters/BMCrossover.h"
#include "../Convolution/BMConvolver.h"
#include <stdatomic.h>



/// The bands on either side of the mid band of the three-band early-reflection model.
enum BMMonoToStereoSideBand { BM_MTS_BAND_BASS = 0, BM_MTS_BAND_TREBLE = 1 };

// one impulse-response pair, as convolvers
typedef struct BMMonoToStereoConvolution {
	BMConvolver left, right;
	bool initialised;
	size_t length;
} BMMonoToStereoConvolution;


// a band of the three-band early-reflection model, and the model: all of what
// BMMonoToStereo_setEarlyReflections, _setSideBandEarlyReflections and
// _setSideBandBypass are given (BMMonoToStereo_measureModel)
typedef struct BMMonoToStereoBandModel {
	float minDelaySeconds, maxDelaySeconds;
	size_t numWetTaps;
	float rt60Seconds, dryGain, wetTapGain;
	bool bypassed;      // side bands only
	uint32_t seed;      // side bands only: the seed of the band's taps; 0 for the band's own (BM_MTS_SIDE_BAND_SEED)
} BMMonoToStereoBandModel;

typedef struct BMMonoToStereoModel {
	float sampleRate;
	float bassCrossoverHz, trebleCrossoverHz;
	BMMonoToStereoBandModel mid, side[2];     // side: index enum BMMonoToStereoSideBand
} BMMonoToStereoModel;


typedef struct BMMonoToStereo {
    BMCrossover3way crossover;
    BMVelvetNoiseDecorrelator vnd;
	// BMMonoToStereo_setSideBandEarlyReflections: a decorrelator each for the
	// bands below and above the mid band of the three-band early-reflection
	// model (index: enum BMMonoToStereoSideBand). Bypassed, a side band passes
	// dry, and mono for a mono input.
	BMVelvetNoiseDecorrelator vndSide[2];
	bool sideBandInitialised[2], sideBandBypassed[2];
	uint32_t sideBandSeed[2];   // BMMonoToStereo_setSideBandSeed
	float *lowL, *lowR, *midL, *midR, *highL, *highR;
	bool stereoInput;
	// BMMonoToStereo_setEarlyReflections: run the decorrelator on the whole
	// signal (fullBand), or split the signal in three with a fourth-order
	// Linkwitz-Riley crossover of its own (threeBand) and run it on the mid
	// band, with the side bands' own decorrelators or nothing on the others,
	// instead of on the mid band of the second-order three-way crossover.
	bool fullBand, threeBand, erCrossoverInitialised;
	float bassCrossoverHz, trebleCrossoverHz;
	BMCrossover3way erCrossover;
	// BMMonoToStereo_setBypass: the output is crossfaded against the input.
	// bypassMix is 1 with the effect in, 0 bypassed; it moves bypassMixStep
	// per sample toward the target. refL, refR hold the input of the chunk
	// being processed (processing may be in place).
	bool bypassed;
	float bypassMix, bypassMixStep;
	float *refL, *refR;
	// The three-band model run as a convolution of the mono input with its own
	// left and right impulse responses instead of as crossover, delays and
	// sums (BMMonoToStereo_bakeConvolution, BMMonoToStereo_installConvolution).
	// Two slots: the audio thread runs the active one, the control thread
	// fills the other and hands it over through pendingConvolution; the audio
	// thread then runs both (the incoming one first catches up on the input's
	// history, then the two are crossfaded) and makes the incoming one active.
	bool convolved;
	BMMonoToStereoConvolution convolution[2];
	int activeConvolution, incomingConvolution;
	_Atomic(int) pendingConvolution;          // -1, or the slot that waits to be taken up
	_Atomic(bool) convolutionInTransition;
	size_t transitionPosition, transitionPreroll, transitionFade;
} BMMonoToStereo;


// BMMonoToStereo_bakeConvolution: the convolvers' block. Audio buffers that are
// a multiple of it are convolved with no delay (see BMConvolver); any other
// size delays the reflections by one block.
#define BM_MTS_CONVOLUTION_BLOCK 64
// how much of the model's response is measured, and how far under the whole
// response's energy the part that is cut off lies (in either channel)
#define BM_MTS_CONVOLUTION_MEASURE_SECONDS 0.5f
#define BM_MTS_CONVOLUTION_TAIL_DB -120.0f
// BMMonoToStereo_installConvolution: the crossfade from the old response to the new
#define BM_MTS_CONVOLUTION_FADE_SECONDS 0.02f


/*!
 *BMMonoToStereo_init
 */
void BMMonoToStereo_init(BMMonoToStereo *This, float sampleRate, bool stereoInput);

/*!
 *BMMonoToStereo_initBigger
 *
 * @abstract Larger legacy preset: 128 taps per channel (including the dry tap),
 * uniform random placement over 40 ms, RT60 0.5 s, wet mix 0.92. Uses an
 * automatic per-instance seed and the same 350/1200 Hz mid-band crossover.
 */
void BMMonoToStereo_initBigger(BMMonoToStereo *This, float sampleRate, bool stereoInput);



/*!
 *BMMonoToStereo_processBuffer
 */
void BMMonoToStereo_processBuffer(BMMonoToStereo *This,
								  const float* inputL, const float* inputR,
								  float* outputL, float* outputR,
								  size_t numSamples);

/*!
 *BMMonoToStereo_setBypass
 *
 * @abstract Click-free bypass: the effect keeps running (so its filters and
 * delay lines stay current and re-engaging it plays no stale audio) and its
 * output is crossfaded against its input over about 20 ms. Fully bypassed,
 * the output is an exact copy of the input (the mono input on both channels
 * if the input is mono). fade = false jumps to the new state at once, for
 * use before processing starts. Safe to call from the control thread.
 */
void BMMonoToStereo_setBypass(BMMonoToStereo *This, bool bypassed, bool fade);


/*!
 *BMMonoToStereo_free
 */
void BMMonoToStereo_free(BMMonoToStereo *This);



/*!
 *BMMonoToStereo_setEarlyReflections
 *
 * @abstract Replace the decorrelator's default pattern (24 taps per channel
 * between maxDelay/23 and 20 ms, RT60 0.5 s, wet mix 0.92, mid band only)
 * with an early-reflection model: a dry tap of dryGain and numWetTaps velvet
 * taps per channel between minDelaySeconds and maxDelaySeconds after it
 * (independent times and signs for L and R), decaying with rt60Seconds.
 * wetTapGain is the gain of the whole burst of wet taps, not of its first
 * tap: each channel's wet taps are normalised to unit energy (the root of
 * the sum of their squares) and then scaled by it, so that the decay, the
 * delays and the number of taps shape the reflections without changing how
 * loud they are. dryGain and wetTapGain set the balance only: the two are
 * then scaled together so that the total energy of each channel's taps, the
 * dry one included, is 1, and the band is as loud with the reflections as
 * without them (BMVelvetNoiseDecorrelator_setDryAndWetAtUnitEnergy).
 *
 * With 0 < bassCrossoverHz < trebleCrossoverHz the signal is split in three
 * by fourth-order Linkwitz-Riley crossovers and this model runs on the mid
 * band only. The bands on either side pass dry (the same on both channels for
 * a mono input) unless they are given reflections of their own with
 * BMMonoToStereo_setSideBandEarlyReflections. Splitting is what makes few
 * taps go far: a band needs taps in proportion to its width times the time
 * they are spread over, so a dozen taps do for the bass spread over tens of
 * milliseconds, for the mid over a few, and for the wide treble in a
 * millisecond or two, where one band for everything would need many times
 * that. It also keeps a short burst from cutting the bass: below about
 * 1 / (2 maxDelay) every tap is in phase with the dry tap, so the gain there
 * is 1 + the signed sum of the tap gains (measured -3 to -6 dB below 200 Hz
 * with 12 taps in 5 ms). Any other crossover values run the whole signal
 * through the decorrelator. Re-initialises the decorrelator and the
 * crossover: call before processing starts.
 */
void BMMonoToStereo_setEarlyReflections(BMMonoToStereo *This,
										float minDelaySeconds, float maxDelaySeconds,
										size_t numWetTaps, float rt60Seconds,
										float dryGain, float wetTapGain,
										float bassCrossoverHz, float trebleCrossoverHz);


// A side band's decorrelator is seeded apart from the mid band's and from the
// other side band's, so that no two bands get the same tap pattern from the
// same settings.
#define BM_MTS_SIDE_BAND_SEED(band) (BM_VND_DEFAULT_SEED + 1 + (uint32_t)(band))

/*!
 *BMMonoToStereo_setSideBandEarlyReflections
 *
 * @abstract Early reflections for the bass or the treble band of the
 * three-band model: a velvet-noise model like the mid band's, with its own
 * delays, decay, tap count and gains, and its own seed. Requires the
 * three-band model, and is lost when BMMonoToStereo_setEarlyReflections drops
 * the crossover. (Re)builds the band's decorrelator: not for while audio runs.
 */
void BMMonoToStereo_setSideBandEarlyReflections(BMMonoToStereo *This, enum BMMonoToStereoSideBand band,
												float minDelaySeconds, float maxDelaySeconds,
												size_t numWetTaps, float rt60Seconds,
												float dryGain, float wetTapGain);

/*!
 *BMMonoToStereo_bakeConvolution
 *
 * @abstract Run the three-band early-reflection model as a convolution. The
 * model is linear and time-invariant, so it is its impulse response: this
 * measures that response, left and right, by running the model itself on an
 * impulse (crossover, the three bands' taps, their gains and bypasses, all of
 * it), cuts it where the rest is BM_MTS_CONVOLUTION_TAIL_DB under the whole,
 * and from then on processBuffer convolves the mono input with the two
 * responses (BMConvolver) instead of running the model. The output is the
 * model's to within rounding (about -125 dB). It is cheaper than the model
 * (a half to a third of its cost with a hundred taps) and its cost does not
 * depend on the number of taps, so the bands can be as dense as they like.
 *
 * The convolution is a snapshot of this struct's own model, taken at once:
 * for setting up, where no other thread runs processBuffer. While audio runs,
 * a changed model goes in with BMMonoToStereo_measureModel and
 * BMMonoToStereo_installConvolution. Allocates. Mono input and the
 * three-band model only. The bypass (BMMonoToStereo_setBypass) works as before.
 */
void BMMonoToStereo_bakeConvolution(BMMonoToStereo *This);

/*!
 *BMMonoToStereo_measureModel
 *
 * @abstract The left and right impulse responses of a three-band model, for
 * BMMonoToStereo_installConvolution: builds a model of its own from the
 * description (the same calls, so the same seeded taps, as a BMMonoToStereo
 * set up with them), runs an impulse through it and frees it. Touches no
 * BMMonoToStereo: it can run on any thread, while audio runs.
 *
 * @param left   set to a malloc'd array of the returned length; the caller frees it
 * @param right  the same
 * @returns the length of the two responses
 */
size_t BMMonoToStereo_measureModel(const BMMonoToStereoModel *model, float **left, float **right);

/*!
 *BMMonoToStereo_installConvolution
 *
 * @abstract Replace the impulse responses the convolution runs with, without
 * a click and while the audio thread runs processBuffer: the convolvers for
 * the new responses are built here, on the calling thread, in the slot the
 * audio thread is not using, and handed over. The audio thread then runs the
 * new convolvers beside the old ones for the length of the response, so that
 * they have the input's history, crossfades to them over
 * BM_MTS_CONVOLUTION_FADE_SECONDS, and drops the old ones; it allocates and
 * frees nothing. One control thread at a time.
 *
 * @param immediate  no audio thread is running processBuffer (setting up, or
 *                   offline rendering from the calling thread): the new
 *                   responses are in use when this returns, with no crossfade
 * @returns false if the audio thread is still crossfading to the responses
 *          installed before: nothing was done, try again in a few milliseconds.
 *          (A response that was handed over but not yet taken up is replaced.)
 */
bool BMMonoToStereo_installConvolution(BMMonoToStereo *This, const float *left, const float *right, size_t length, bool immediate);

/*!
 *BMMonoToStereo_clearConvolution
 *
 * @abstract Back to running the model itself. Frees the convolvers; not to be
 * called while another thread runs processBuffer.
 */
void BMMonoToStereo_clearConvolution(BMMonoToStereo *This);


/*!
 *BMMonoToStereo_setSideBandSeed
 *
 * @abstract The seed a side band's taps (times and signs) are drawn with, from
 * the next BMMonoToStereo_setSideBandEarlyReflections on. The default is the
 * band's own, BM_MTS_SIDE_BAND_SEED(band); _init goes back to it. See
 * BMMonoToStereo_findSideBandSeed for why one seed is better than another.
 */
void BMMonoToStereo_setSideBandSeed(BMMonoToStereo *This, enum BMMonoToStereoSideBand band, uint32_t seed);

/*!
 *BMMonoToStereo_sideBandDeviationDb
 *
 * @abstract How far a band's reflections take the level from 0 dB: the largest
 * |20 log10 |H(f)||, in either channel, over lowHz...highHz, where H is the
 * response of the band's decorrelator, the dry tap included, with the dry and
 * wet gains of the description and the taps its seed draws.
 *
 * The taps' energy is 1 (dry^2 + wet^2), so over many ripples of the response
 * the level averages to 0 dB whatever the seed is. But a ripple is about
 * 1 / (the taps' span) wide whatever the frequency, 29 Hz for 35 ms, and the
 * bass band is narrow: below a crossover at 176 Hz it holds six ripples, a
 * bass note sits on one of them, and one draw of the taps can take several dB
 * from it, or tens at a high wet, where the next draw adds a few. The mid and
 * treble bands are hundreds of ripples wide and do not have the problem.
 */
float BMMonoToStereo_sideBandDeviationDb(const BMMonoToStereoBandModel *band, float sampleRate, float lowHz, float highHz);

/*!
 *BMMonoToStereo_findSideBandSeed
 *
 * @abstract Try the seeds firstSeed ..< firstSeed + tries on the band (its own
 * seed field is ignored) and stop at the first whose
 * BMMonoToStereo_sideBandDeviationDb is within toleranceDb. Seeds are tried
 * in order, so the result does not depend on how the search is cut into
 * calls (cut it to be able to give up between them).
 *
 * @param deviationDbOut  the deviation of the returned seed
 * @returns the first seed that passes, or, if none of them does (*deviationDbOut > toleranceDb), the best of them
 */
uint32_t BMMonoToStereo_findSideBandSeed(const BMMonoToStereoBandModel *band, float sampleRate, float lowHz, float highHz,
										 float toleranceDb, uint32_t firstSeed, size_t tries, float *deviationDbOut);


/*!
 *BMMonoToStereo_setSideBandBypass
 *
 * @abstract true passes the band dry (its decorrelator stops running); false
 * runs its reflections. Takes effect at once, so switch it where the
 * reflections are quiet. Safe from the control thread.
 */
void BMMonoToStereo_setSideBandBypass(BMMonoToStereo *This, enum BMMonoToStereoSideBand band, bool bypassed);

/*!
 *BMMonoToStereo_setSideBandWetTapGain
 *
 * @abstract BMMonoToStereo_setEarlyReflectionsWetTapGain for a side band.
 */
void BMMonoToStereo_setSideBandWetTapGain(BMMonoToStereo *This, enum BMMonoToStereoSideBand band, float wetTapGain);


/*!
 *BMMonoToStereo_setCrossoversHz
 *
 * @abstract Move the two crossovers of the three-band early-reflection model
 * while it runs: the crossovers' filters take new cutoffs from the control
 * thread, so this is seamless. Requires the three-band model, and
 * 0 < bassCrossoverHz < trebleCrossoverHz.
 */
void BMMonoToStereo_setCrossoversHz(BMMonoToStereo *This, float bassCrossoverHz, float trebleCrossoverHz);


/*!
 *BMMonoToStereo_setEarlyReflectionsWetTapGain
 *
 * @abstract The wet tap gain of the early-reflection model, changed while it
 * runs: the level of the reflections moves, the reflections stay the same,
 * and the dry tap is rescaled to keep the total energy at 1
 * (BMVelvetNoiseDecorrelator_setWetTapGain). Call after _setEarlyReflections.
 */
void BMMonoToStereo_setEarlyReflectionsWetTapGain(BMMonoToStereo *This, float wetTapGain);


/*!
 *BMMonoToStereo_setWetMix
 *
 * @param This pointer to an initialised struct
 * @param wetMix01 linear scale mix in [0,1]
 */
void BMMonoToStereo_setWetMix(BMMonoToStereo *This, float wetMix01);


#endif /* BMMonoToStereo_h */
