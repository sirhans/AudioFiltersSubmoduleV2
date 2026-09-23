//
//  BMMonoToStereo.c
//  AudioFiltersXcodeProject
//
//  Created by hans anderson on 10/9/19.
//  Anyone may use this file without restrictions of any kind
//

#include "BMMonoToStereo.h"
#include <math.h>
#include <assert.h>


#define BM_MTS_RT60 0.50f
#define BM_MTS_LOW_CROSSOVER_FC 350.0f
#define BM_MTS_HIGH_CROSSOVER_FC 1200.0f
#define BM_MTS_TAPS_PER_CHANNEL 24
#define BM_MTS_TAPS_PER_CHANNEL_BIG 128
#define BM_MTS_WET_MIX 0.92f
#define BM_MTS_DECORRELATOR_FREQUENCY_BAND_WIDTH 50.0f
#define BM_MTS_DIFFUSION_TIME 1.0f / BM_MTS_DECORRELATOR_FREQUENCY_BAND_WIDTH
#define BM_MTS_DIFFUSION_TIME_BIG 2.0f / BM_MTS_DECORRELATOR_FREQUENCY_BAND_WIDTH



/*!
 *BMMonoToStereo_init
 */
static void BMMonoToStereo_initInternal(BMMonoToStereo *This, float sampleRate,
									   bool stereoInput, bool bigger){
	This->stereoInput = stereoInput;
	This->convolved = false;
	This->convolution[0].initialised = This->convolution[1].initialised = false;
	This->activeConvolution = 0;
	This->incomingConvolution = 0;
	atomic_store(&This->pendingConvolution, -1);
	atomic_store(&This->convolutionInTransition, false);
	This->transitionPosition = This->transitionPreroll = This->transitionFade = 0;
	This->fullBand = false;
	This->threeBand = false;
	This->erCrossoverInitialised = false;
	This->bassCrossoverHz = This->trebleCrossoverHz = 0.0f;
	for(size_t b=0; b<2; b++){
		This->sideBandInitialised[b] = false;
		This->sideBandBypassed[b] = true;
		This->sideBandSeed[b] = BM_MTS_SIDE_BAND_SEED(b);
	}
	
	if(bigger){
		// Preserve the original larger preset: uniform random placement,
		// 128 taps including the dry tap, over a 40 ms window.
		BMVelvetNoiseDecorrelator_init(&This->vnd, BM_MTS_DIFFUSION_TIME_BIG,
			BM_MTS_TAPS_PER_CHANNEL_BIG, BM_MTS_RT60, true, sampleRate);
	} else {
		// Preserve the synth's reproducible, evenly distributed default pattern.
		BMRandom randomizer;
		BMRandom_init(&randomizer, BM_VND_DEFAULT_SEED);
		BMVelvetNoiseDecorrelator_initWithRandomizerState(&This->vnd, -1.0f,
			BM_MTS_DIFFUSION_TIME, BM_MTS_TAPS_PER_CHANNEL, BM_MTS_RT60,
			true, true, sampleRate, &randomizer);
	}

	// set the wet/dry mix
	BMVelvetNoiseDecorrelator_setWetMix(&This->vnd, BM_MTS_WET_MIX);
	
	// initialise a pair of crossover filters that will isolate only the midrange
	// frequencies for mono-to-stereo conversion
    BMCrossover3way_init(&This->crossover,
                         BM_MTS_LOW_CROSSOVER_FC,
                         BM_MTS_HIGH_CROSSOVER_FC,
                         sampleRate,
                         false,
                         stereoInput);
	
	// effect in; 20 ms bypass crossfade
	This->bypassed = false;
	This->bypassMix = 1.0f;
	This->bypassMixStep = 1.0f / (0.020f * sampleRate);
	
	// allocate memory for buffers
	size_t numBuffers = 8;
	This->lowL = malloc(sizeof(float) * BM_BUFFER_CHUNK_SIZE * numBuffers);
	This->lowR = This->lowL + BM_BUFFER_CHUNK_SIZE;
	This->midL = This->lowR + BM_BUFFER_CHUNK_SIZE;
	This->midR = This->midL + BM_BUFFER_CHUNK_SIZE;
	This->highL = This->midR + BM_BUFFER_CHUNK_SIZE;
	This->highR = This->highL + BM_BUFFER_CHUNK_SIZE;
	This->refL = This->highR + BM_BUFFER_CHUNK_SIZE;
	This->refR = This->refL + BM_BUFFER_CHUNK_SIZE;
}





void BMMonoToStereo_init(BMMonoToStereo *This, float sampleRate, bool stereoInput){
	BMMonoToStereo_initInternal(This, sampleRate, stereoInput, false);
}


void BMMonoToStereo_initBigger(BMMonoToStereo *This, float sampleRate, bool stereoInput){
	BMMonoToStereo_initInternal(This, sampleRate, stereoInput, true);
}


static void BMMonoToStereo_processEffect(BMMonoToStereo *This,
										 const float* inputL, const float* inputR,
										 float* outputL, float* outputR,
										 size_t numSamples);
static void BMMonoToStereo_processConvolution(BMMonoToStereo *This, const float *input, float *outputL, float *outputR, size_t numSamples);


/*!
 *BMMonoToStereo_processBuffer
 *
 * The effect always runs; its output is crossfaded against the input of the
 * same chunk (see BMMonoToStereo_setBypass).
 */
void BMMonoToStereo_processBuffer(BMMonoToStereo *This,
								  const float* inputL, const float* inputR,
								  float* outputL, float* outputR,
								  size_t numSamples){
	while(numSamples > 0){
		size_t n = BM_MIN(numSamples, BM_BUFFER_CHUNK_SIZE);
		const float target = This->bypassed ? 0.0f : 1.0f;
		float mix = This->bypassMix;
		bool needRef = (mix != 1.0f || target != 1.0f);
		
		// processing may be in place: keep the input
		if(needRef){
			memcpy(This->refL, inputL, sizeof(float) * n);
			memcpy(This->refR, This->stereoInput ? inputR : inputL, sizeof(float) * n);
		}
		
		BMMonoToStereo_processEffect(This, inputL, inputR, outputL, outputR, n);
		
		if(needRef){
			if(mix == target){   // fully bypassed
				memcpy(outputL, This->refL, sizeof(float) * n);
				memcpy(outputR, This->refR, sizeof(float) * n);
			} else {
				const float step = target > mix ? This->bypassMixStep : -This->bypassMixStep;
				for(size_t i = 0; i < n; i++){
					mix += step;
					if((step > 0.0f && mix >= target) || (step < 0.0f && mix <= target)) mix = target;
					// smoothstep: no corner at either end of the fade
					const float shaped = mix * mix * (3.0f - 2.0f * mix);
					outputL[i] = This->refL[i] + shaped * (outputL[i] - This->refL[i]);
					outputR[i] = This->refR[i] + shaped * (outputR[i] - This->refR[i]);
				}
				This->bypassMix = mix;
			}
		}
		
		inputL += n; outputL += n; outputR += n; numSamples -= n;
		if(This->stereoInput) inputR += n;
	}
}


void BMMonoToStereo_setBypass(BMMonoToStereo *This, bool bypassed, bool fade){
	This->bypassed = bypassed;
	if(!fade) This->bypassMix = bypassed ? 0.0f : 1.0f;
}


static void BMMonoToStereo_processEffect(BMMonoToStereo *This,
								  const float* inputL, const float* inputR,
								  float* outputL, float* outputR,
								  size_t numSamples){
	
	// the three-band model as a convolution with its own impulse responses
	if(This->convolved){
		BMMonoToStereo_processConvolution(This, inputL, outputL, outputR, numSamples);
		return;
	}
	
	// early-reflection model in three bands: split, run the mid band's
	// reflections and those of the side bands that are in, and add the bands
	// back together. A side band that is out passes dry, the same on both
	// channels for a mono input.
	if(This->threeBand){
		// read once: the control thread may switch the side bands while this runs
		const bool bass = This->sideBandInitialised[BM_MTS_BAND_BASS] && !This->sideBandBypassed[BM_MTS_BAND_BASS];
		const bool treble = This->sideBandInitialised[BM_MTS_BAND_TREBLE] && !This->sideBandBypassed[BM_MTS_BAND_TREBLE];
		BMVelvetNoiseDecorrelator *bassVnd = &This->vndSide[BM_MTS_BAND_BASS], *trebleVnd = &This->vndSide[BM_MTS_BAND_TREBLE];
		while(numSamples > 0){
			size_t n = BM_MIN(numSamples, BM_BUFFER_CHUNK_SIZE);
			if(This->stereoInput){
				BMCrossover3way_processStereo(&This->erCrossover, inputL, inputR, This->lowL, This->lowR,
											  This->midL, This->midR, This->highL, This->highR, n);
				BMVelvetNoiseDecorrelator_processBufferStereo(&This->vnd, This->midL, This->midR, This->midL, This->midR, n);
				if(bass) BMVelvetNoiseDecorrelator_processBufferStereo(bassVnd, This->lowL, This->lowR, This->lowL, This->lowR, n);
				if(treble) BMVelvetNoiseDecorrelator_processBufferStereo(trebleVnd, This->highL, This->highR, This->highL, This->highR, n);
				inputR += n;
			} else {
				BMCrossover3way_processMono(&This->erCrossover, inputL, This->lowL, This->midL, This->highL, n);
				BMVelvetNoiseDecorrelator_processBufferMonoToStereo(&This->vnd, This->midL, This->midL, This->midR, n);
				// a side band's own reflections: mono in, left and right out; without them, the same on both sides
				if(bass) BMVelvetNoiseDecorrelator_processBufferMonoToStereo(bassVnd, This->lowL, This->lowL, This->lowR, n);
				else memcpy(This->lowR, This->lowL, sizeof(float)*n);
				if(treble) BMVelvetNoiseDecorrelator_processBufferMonoToStereo(trebleVnd, This->highL, This->highL, This->highR, n);
				else memcpy(This->highR, This->highL, sizeof(float)*n);
			}
			BMCrossover3way_recombine(This->lowL, This->lowR, This->midL, This->midR, This->highL, This->highR, outputL, outputR, n);
			inputL += n; outputL += n; outputR += n; numSamples -= n;
		}
		return;
	}
	
	// early-reflection model: the whole signal through the decorrelator
	if(This->fullBand){
		if(This->stereoInput)
			BMVelvetNoiseDecorrelator_processBufferStereo(&This->vnd, (float*)inputL, (float*)inputR, outputL, outputR, numSamples);
		else
			BMVelvetNoiseDecorrelator_processBufferMonoToStereo(&This->vnd, (float*)inputL, outputL, outputR, numSamples);
		return;
	}
	
	while(numSamples > 0){
		size_t samplesProcessing = BM_MIN(numSamples, BM_BUFFER_CHUNK_SIZE);
	
		if(This->stereoInput){
			// split low, mid, and high
			BMCrossover3way_processStereo(&This->crossover,
										  inputL, inputR,
										  This->lowL, This->lowR,
										  This->midL, This->midR,
										  This->highL, This->highR,
										  samplesProcessing);
			
			
			// process the Velvet Noise Decorrelator on the mid frequencies
			BMVelvetNoiseDecorrelator_processBufferStereo(&This->vnd,
														  This->midL, This->midR,
														  This->midL, This->midR,
														  samplesProcessing);
			
			// combine the low, mid, and high back together
			BMCrossover3way_recombine(This->lowL, This->lowR,
									  This->midL, This->midR,
									  This->highL, This->highR,
									  outputL, outputR,
									  samplesProcessing);
			
			// advance pointers
			numSamples -= samplesProcessing;
			inputL     += samplesProcessing;
			inputR     += samplesProcessing;
			outputL    += samplesProcessing;
			outputR    += samplesProcessing;
		}
		
		// mono input
		else {
			// split low, mid, and high
			BMCrossover3way_processMono(&This->crossover,
										inputL,
										This->lowL,
										This->midL,
										This->highL,
										samplesProcessing);
			
			
			// process the Velvet Noise Decorrelator on the mid frequencies
			BMVelvetNoiseDecorrelator_processBufferMonoToStereo(&This->vnd,
																This->midL,
																This->midL, This->midR,
																samplesProcessing);
			
			// combine the low, mid, and high back together
			BMCrossover3way_recombine(This->lowL, This->lowL,
									  This->midL, This->midR,
									  This->highL, This->highL,
									  outputL, outputR,
									  samplesProcessing);
			
			// advance pointers
			numSamples -= samplesProcessing;
			inputL     += samplesProcessing;
			outputL    += samplesProcessing;
			outputR    += samplesProcessing;
		}
	}
}





/*!
 *BMMonoToStereo_free
 */
static void BMMonoToStereo_freeConvolutionSlot(BMMonoToStereoConvolution *slot){
	if(!slot->initialised) return;
	BMConvolver_free(&slot->left);
	BMConvolver_free(&slot->right);
	slot->initialised = false;
	slot->length = 0;
}


void BMMonoToStereo_clearConvolution(BMMonoToStereo *This){
	This->convolved = false;
	atomic_store(&This->pendingConvolution, -1);
	atomic_store(&This->convolutionInTransition, false);
	BMMonoToStereo_freeConvolutionSlot(&This->convolution[0]);
	BMMonoToStereo_freeConvolutionSlot(&This->convolution[1]);
}


/*
 * The model's response to an impulse, cut where what follows is negligible
 * beside the whole in either channel. The model's state (filters, delay
 * lines) must be at rest. Returns malloc'd arrays.
 */
static size_t BMMonoToStereo_measure(BMMonoToStereo *This, float **leftOut, float **rightOut){
	size_t length = (size_t)(BM_MTS_CONVOLUTION_MEASURE_SECONDS * This->vnd.sampleRate);
	length += BM_MTS_CONVOLUTION_BLOCK - length % BM_MTS_CONVOLUTION_BLOCK;
	float *impulse = calloc(length, sizeof(float));
	float *left = calloc(length, sizeof(float)), *right = calloc(length, sizeof(float));
	impulse[0] = 1.0f;
	BMMonoToStereo_processEffect(This, impulse, impulse, left, right, length);
	free(impulse);
	
	double totalL = 0.0, totalR = 0.0;
	for(size_t i=0; i<length; i++){ totalL += (double)left[i]*left[i]; totalR += (double)right[i]*right[i]; }
	const double floor = pow(10.0, BM_MTS_CONVOLUTION_TAIL_DB / 10.0);
	double tailL = 0.0, tailR = 0.0;
	size_t keep = length;
	while(keep > 1){
		tailL += (double)left[keep-1]*left[keep-1];
		tailR += (double)right[keep-1]*right[keep-1];
		if(tailL > floor * totalL || tailR > floor * totalR) break;
		keep--;
	}
	*leftOut = left; *rightOut = right;
	return keep;
}


size_t BMMonoToStereo_measureModel(const BMMonoToStereoModel *model, float **left, float **right){
	BMMonoToStereo scratch;
	BMMonoToStereo_init(&scratch, model->sampleRate, false);
	BMMonoToStereo_setBypass(&scratch, false, false);
	BMMonoToStereo_setEarlyReflections(&scratch, model->mid.minDelaySeconds, model->mid.maxDelaySeconds, model->mid.numWetTaps,
									   model->mid.rt60Seconds, model->mid.dryGain, model->mid.wetTapGain,
									   model->bassCrossoverHz, model->trebleCrossoverHz);
	for(int b=0; b<2; b++){
		const BMMonoToStereoBandModel *band = &model->side[b];
		if(band->seed != 0) BMMonoToStereo_setSideBandSeed(&scratch, (enum BMMonoToStereoSideBand)b, band->seed);
		BMMonoToStereo_setSideBandEarlyReflections(&scratch, (enum BMMonoToStereoSideBand)b, band->minDelaySeconds, band->maxDelaySeconds,
												   band->numWetTaps, band->rt60Seconds, band->dryGain, band->wetTapGain);
		BMMonoToStereo_setSideBandBypass(&scratch, (enum BMMonoToStereoSideBand)b, band->bypassed);
	}
	size_t length = BMMonoToStereo_measure(&scratch, left, right);
	BMMonoToStereo_free(&scratch);
	return length;
}


bool BMMonoToStereo_installConvolution(BMMonoToStereo *This, const float *left, const float *right, size_t length, bool immediate){
	assert(!This->stereoInput && length > 0);
	
	if(!immediate && This->convolved){
		// the audio thread is using both slots
		if(atomic_load(&This->convolutionInTransition)) return false;
		// take back a slot that was handed over and not taken up. Whichever of
		// the two threads' exchanges comes first gets it: if the audio thread's
		// did, it is in transition now.
		atomic_exchange(&This->pendingConvolution, -1);
		if(atomic_load(&This->convolutionInTransition)) return false;
	}
	
	// idle, and nothing pending: the slot that is not active is this thread's
	int slot = This->convolved ? 1 - This->activeConvolution : 0;
	BMMonoToStereoConvolution *c = &This->convolution[slot];
	BMMonoToStereo_freeConvolutionSlot(c);
	BMConvolver_init(&c->left, length, BM_MTS_CONVOLUTION_BLOCK);
	BMConvolver_init(&c->right, length, BM_MTS_CONVOLUTION_BLOCK);
	BMConvolver_setImpulseResponse(&c->left, left, length);
	BMConvolver_setImpulseResponse(&c->right, right, length);
	c->length = length;
	c->initialised = true;
	
	if(immediate || !This->convolved){
		This->activeConvolution = slot;
		This->convolved = true;
	}
	else atomic_store(&This->pendingConvolution, slot);
	return true;
}


void BMMonoToStereo_bakeConvolution(BMMonoToStereo *This){
	assert(This->threeBand && !This->stereoInput);
	BMMonoToStereo_clearConvolution(This);   // the measurement runs the model itself
	float *left, *right;
	size_t length = BMMonoToStereo_measure(This, &left, &right);
	BMMonoToStereo_installConvolution(This, left, right, length, true);
	free(left); free(right);
}


/*
 * Audio thread: the active convolution, and beside it one that is coming in.
 * numSamples <= BM_BUFFER_CHUNK_SIZE; the input may be outputL.
 */
static void BMMonoToStereo_processConvolution(BMMonoToStereo *This, const float *input, float *outputL, float *outputR, size_t numSamples){
	// take up a convolution that was handed over
	if(!atomic_load(&This->convolutionInTransition)){
		int pending = atomic_exchange(&This->pendingConvolution, -1);
		if(pending >= 0){
			This->incomingConvolution = pending;
			This->transitionPosition = 0;
			This->transitionPreroll = This->convolution[pending].length;
			This->transitionFade = (size_t)(BM_MTS_CONVOLUTION_FADE_SECONDS * This->vnd.sampleRate);
			atomic_store(&This->convolutionInTransition, true);
		}
	}
	
	BMMonoToStereoConvolution *active = &This->convolution[This->activeConvolution];
	if(!atomic_load(&This->convolutionInTransition)){
		// the right channel first: the input may be the left output
		BMConvolver_processBuffer(&active->right, input, outputR, numSamples);
		BMConvolver_processBuffer(&active->left, input, outputL, numSamples);
		return;
	}
	
	// the incoming one runs on the same input; it is heard once it has heard
	// as much of the input as its response is long
	BMMonoToStereoConvolution *incoming = &This->convolution[This->incomingConvolution];
	float *newL = This->lowL, *newR = This->lowR;
	BMConvolver_processBuffer(&incoming->left, input, newL, numSamples);
	BMConvolver_processBuffer(&incoming->right, input, newR, numSamples);
	BMConvolver_processBuffer(&active->right, input, outputR, numSamples);
	BMConvolver_processBuffer(&active->left, input, outputL, numSamples);
	for(size_t i=0; i<numSamples; i++){
		size_t position = This->transitionPosition + i;
		if(position < This->transitionPreroll) continue;
		float t = (float)(position - This->transitionPreroll + 1) / (float)This->transitionFade;
		if(t > 1.0f) t = 1.0f;
		const float shaped = t * t * (3.0f - 2.0f * t);
		outputL[i] += shaped * (newL[i] - outputL[i]);
		outputR[i] += shaped * (newR[i] - outputR[i]);
	}
	This->transitionPosition += numSamples;
	if(This->transitionPosition >= This->transitionPreroll + This->transitionFade){
		This->activeConvolution = This->incomingConvolution;
		atomic_store(&This->convolutionInTransition, false);
	}
}


void BMMonoToStereo_free(BMMonoToStereo *This){
	BMMonoToStereo_clearConvolution(This);
	BMVelvetNoiseDecorrelator_free(&This->vnd);
	for(size_t b=0; b<2; b++){
		if(This->sideBandInitialised[b]){
			BMVelvetNoiseDecorrelator_free(&This->vndSide[b]);
			This->sideBandInitialised[b] = false;
		}
	}
	BMCrossover3way_free(&This->crossover);
	if(This->erCrossoverInitialised){
		BMCrossover3way_free(&This->erCrossover);
		This->erCrossoverInitialised = false;
	}
	
	// free buffer memory
	// we only call free once because all buffers were allocated with a single
	// call to malloc
	free(This->lowL);
	This->lowL = NULL;
}





void BMMonoToStereo_setEarlyReflections(BMMonoToStereo *This,
										float minDelaySeconds, float maxDelaySeconds,
										size_t numWetTaps, float rt60Seconds,
										float dryGain, float wetTapGain,
										float bassCrossoverHz, float trebleCrossoverHz){
	float sampleRate = This->vnd.sampleRate;
	BMVelvetNoiseDecorrelator_free(&This->vnd);
	// numTaps counts the dry tap
	BMRandom randomizer;
    BMRandom_init(&randomizer, BM_VND_DEFAULT_SEED);
    BMVelvetNoiseDecorrelator_initWithRandomizerState(&This->vnd, minDelaySeconds, maxDelaySeconds,
                                                 numWetTaps + 1, rt60Seconds, true, true,
                                                 sampleRate, &randomizer);
	BMVelvetNoiseDecorrelator_setDryAndWetAtUnitEnergy(&This->vnd, dryGain, wetTapGain);
	
	if(This->erCrossoverInitialised){
		BMCrossover3way_free(&This->erCrossover);
		This->erCrossoverInitialised = false;
	}
	This->bassCrossoverHz = bassCrossoverHz;
	This->trebleCrossoverHz = trebleCrossoverHz;
	This->threeBand = bassCrossoverHz > 0.0f && trebleCrossoverHz > bassCrossoverHz;
	This->fullBand = !This->threeBand;
	// no crossover, no side bands
	if(!This->threeBand){
		for(size_t b=0; b<2; b++){
			This->sideBandBypassed[b] = true;
			if(This->sideBandInitialised[b]){
				BMVelvetNoiseDecorrelator_free(&This->vndSide[b]);
				This->sideBandInitialised[b] = false;
			}
		}
	}
	if(This->threeBand){
		// fourth order: Linkwitz-Riley, the three bands sum flat
		BMCrossover3way_init(&This->erCrossover, bassCrossoverHz, trebleCrossoverHz, sampleRate, true, This->stereoInput);
		This->erCrossoverInitialised = true;
	}
}


void BMMonoToStereo_setSideBandEarlyReflections(BMMonoToStereo *This, enum BMMonoToStereoSideBand band,
												float minDelaySeconds, float maxDelaySeconds,
												size_t numWetTaps, float rt60Seconds,
												float dryGain, float wetTapGain){
	assert(This->threeBand);
	float sampleRate = This->vnd.sampleRate;
	bool wasBypassed = This->sideBandBypassed[band];
	// out of the signal path while it is rebuilt
	This->sideBandBypassed[band] = true;
	if(This->sideBandInitialised[band]) BMVelvetNoiseDecorrelator_free(&This->vndSide[band]);
	// numTaps counts the dry tap
	BMVelvetNoiseDecorrelator_initWithDelayRange(&This->vndSide[band], minDelaySeconds, maxDelaySeconds,
												 numWetTaps + 1, rt60Seconds, true, sampleRate);
	BMVelvetNoiseDecorrelator_setSeed(&This->vndSide[band], This->sideBandSeed[band]);
	BMVelvetNoiseDecorrelator_setDryAndWetAtUnitEnergy(&This->vndSide[band], dryGain, wetTapGain);
	This->sideBandInitialised[band] = true;
	This->sideBandBypassed[band] = wasBypassed;
}


void BMMonoToStereo_setSideBandSeed(BMMonoToStereo *This, enum BMMonoToStereoSideBand band, uint32_t seed){
	This->sideBandSeed[band] = seed;
}


/*
 * The largest deviation from 0 dB of a decorrelator's response over a grid of
 * frequencies, in either channel; gives up once it is over giveUpDb (the
 * caller has no use for the exact figure of a draw that fails).
 */
static float BMMonoToStereo_decorrelatorDeviationDb(const BMVelvetNoiseDecorrelator *vnd, float lowHz, float highHz, float giveUpDb){
	size_t numTaps = BMVelvetNoiseDecorrelator_getNumTaps(vnd);
	// several points to a ripple of the response, which is about 1 / (the taps' span) wide
	double step = fmin(2.0, 1.0 / (8.0 * (double)vnd->maxDelayTimeS));
	double worst = 0.0;
	for(double hz = lowHz; hz <= highHz; hz += step){
		double w = 2.0 * M_PI * hz / (double)vnd->sampleRate;
		for(int channel=0; channel<2; channel++){
			const size_t *delays = channel ? vnd->delayLengthsR : vnd->delayLengthsL;
			const float *gains = channel ? vnd->gainsR : vnd->gainsL;
			double re = 0.0, im = 0.0;
			for(size_t i=0; i<numTaps; i++){
				double a = w * (double)delays[i];
				re += gains[i] * cos(a); im -= gains[i] * sin(a);
			}
			double db = fabs(10.0 * log10(re*re + im*im + 1e-30));
			if(db > worst) worst = db;
		}
		if(worst > giveUpDb) break;
	}
	return (float)worst;
}


/* the band's decorrelator as BMMonoToStereo_setSideBandEarlyReflections builds it, less the seed */
static void BMMonoToStereo_initBandDecorrelator(BMVelvetNoiseDecorrelator *vnd, const BMMonoToStereoBandModel *band, float sampleRate){
	BMVelvetNoiseDecorrelator_initWithDelayRange(vnd, band->minDelaySeconds, band->maxDelaySeconds,
												 band->numWetTaps + 1, band->rt60Seconds, true, sampleRate);
}

static float BMMonoToStereo_trySeed(BMVelvetNoiseDecorrelator *vnd, const BMMonoToStereoBandModel *band, uint32_t seed,
									float lowHz, float highHz, float giveUpDb){
	BMVelvetNoiseDecorrelator_setSeed(vnd, seed);
	BMVelvetNoiseDecorrelator_setDryAndWetAtUnitEnergy(vnd, band->dryGain, band->wetTapGain);
	return BMMonoToStereo_decorrelatorDeviationDb(vnd, lowHz, highHz, giveUpDb);
}


float BMMonoToStereo_sideBandDeviationDb(const BMMonoToStereoBandModel *band, float sampleRate, float lowHz, float highHz){
	assert(band->seed != 0);   // say which: the band's own default is not known here
	BMVelvetNoiseDecorrelator vnd;
	BMMonoToStereo_initBandDecorrelator(&vnd, band, sampleRate);
	float deviation = BMMonoToStereo_trySeed(&vnd, band, band->seed, lowHz, highHz, INFINITY);
	BMVelvetNoiseDecorrelator_free(&vnd);
	return deviation;
}


uint32_t BMMonoToStereo_findSideBandSeed(const BMMonoToStereoBandModel *band, float sampleRate, float lowHz, float highHz,
										 float toleranceDb, uint32_t firstSeed, size_t tries, float *deviationDbOut){
	BMVelvetNoiseDecorrelator vnd;
	BMMonoToStereo_initBandDecorrelator(&vnd, band, sampleRate);
	uint32_t best = firstSeed;
	float bestDeviation = INFINITY;
	for(size_t i=0; i<tries; i++){
		uint32_t seed = firstSeed + (uint32_t)i;
		// a draw that is worse than the best so far is of no interest beyond that
		float deviation = BMMonoToStereo_trySeed(&vnd, band, seed, lowHz, highHz, bestDeviation);
		if(deviation < bestDeviation){ bestDeviation = deviation; best = seed; }
		if(bestDeviation <= toleranceDb) break;
	}
	BMVelvetNoiseDecorrelator_free(&vnd);
	*deviationDbOut = bestDeviation;
	return best;
}


void BMMonoToStereo_setSideBandBypass(BMMonoToStereo *This, enum BMMonoToStereoSideBand band, bool bypassed){
	This->sideBandBypassed[band] = bypassed;
}


void BMMonoToStereo_setSideBandWetTapGain(BMMonoToStereo *This, enum BMMonoToStereoSideBand band, float wetTapGain){
	assert(This->sideBandInitialised[band]);
	BMVelvetNoiseDecorrelator_setWetTapGain(&This->vndSide[band], wetTapGain);
}


void BMMonoToStereo_setCrossoversHz(BMMonoToStereo *This, float bassCrossoverHz, float trebleCrossoverHz){
	assert(This->threeBand && This->erCrossoverInitialised && bassCrossoverHz > 0.0f && trebleCrossoverHz > bassCrossoverHz);
	This->bassCrossoverHz = bassCrossoverHz;
	This->trebleCrossoverHz = trebleCrossoverHz;
	BMCrossover3way_setCutoff1(&This->erCrossover, bassCrossoverHz);
	BMCrossover3way_setCutoff2(&This->erCrossover, trebleCrossoverHz);
}


void BMMonoToStereo_setEarlyReflectionsWetTapGain(BMMonoToStereo *This, float wetTapGain){
	BMVelvetNoiseDecorrelator_setWetTapGain(&This->vnd, wetTapGain);
}


void BMMonoToStereo_setWetMix(BMMonoToStereo *This, float wetMix01){
	BMVelvetNoiseDecorrelator_setWetMix(&This->vnd, wetMix01);
}
