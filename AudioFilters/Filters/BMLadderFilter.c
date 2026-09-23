//
//  BMLadderFilter.c
//  AudioFilters
//
//  See BMLadderFilter.h.
//
//  Created by hans anderson on 13/9/26.
//  Anyone may use this file without restrictions.
//

#include "BMLadderFilter.h"
#include <string.h>
#include <assert.h>

// the bilinear warp blows up toward Nyquist; BMMultiLevelSVF and SynthFilter
// use the same limit
#define BM_LADDER_MAX_FC_FRACTION 0.46



void BMLadderFilter_init(BMLadderFilter *This, float sampleRate, bool isStereo){
	memset(This, 0, sizeof *This);
	This->sampleRate = sampleRate;
	This->numChannels = isStereo ? 2 : 1;
	BMLock_init(&This->lock);
	
	// a sane starting point: open, no feedback
	This->fc_pending = BM_LADDER_MAX_FC_FRACTION * sampleRate;
	This->k_pending = 0.0;
	This->pending = BMLadderFilter_coefs(tanf((float)(M_PI * This->fc_pending / sampleRate)), 0.0f);
	This->target = This->coefs = This->pending;
}



void BMLadderFilter_setLowpass24dB(BMLadderFilter *This, double fc, double feedback){
	fc = fmin(fmax(fc, 0.0), BM_LADDER_MAX_FC_FRACTION * This->sampleRate);
	feedback = fmax(feedback, 0.0);
	const float g = (float)tan(M_PI * fc / This->sampleRate);
	
	BMLock_lock(&This->lock);
	This->fc_pending = fc;
	This->k_pending = feedback;
	This->pending = BMLadderFilter_coefs(g, (float)feedback);
	This->shouldUpdateParam = true;
	BMLock_unlock(&This->lock);
}



void BMLadderFilter_enableFilterSweep(BMLadderFilter *This, bool sweepOn){
	This->filterSweep = sweepOn;
}

void BMLadderFilter_forceImmediateUpdate(BMLadderFilter *This){
	This->updateImmediately = true;
}

void BMLadderFilter_clearBuffers(BMLadderFilter *This){
	This->needsClearStateVariables = true;
}



/*
 * Start of a buffer: clear the state if asked, and pick up a queued update.
 * With sweep on, the update becomes the target the coefficients ramp to
 * across this buffer; otherwise (or with forceImmediateUpdate) it is applied
 * now. As in BMMultiLevelSVF the pending values are copied under the lock
 * with a trylock; if the UI thread holds it, the update waits for the next
 * buffer.
 */
static inline void BMLadderFilter_beginBuffer(BMLadderFilter *This){
	if(This->needsClearStateVariables){
		memset(This->state, 0, sizeof This->state);
		This->needsClearStateVariables = false;
	}
	
	// the previous buffer's ramp ends here
	This->coefs = This->target;
	
	if(This->shouldUpdateParam){
		if(BMLock_trylock(&This->lock)){
			This->target = This->pending;
			This->shouldUpdateParam = false;
			BMLock_unlock(&This->lock);
			if(!This->filterSweep || This->updateImmediately){
				This->coefs = This->target;
				This->updateImmediately = false;
			}
		}
	}
}



/*
 * The coefficients for sample i of a buffer of numSamples during a sweep:
 * G and k ramp linearly from coefs to target, stopping one sample short of
 * the target, which the next buffer starts on (BMMultiLevelSVF's convention).
 */
static inline BMLadderFilterCoefs BMLadderFilter_sweepCoefs(const BMLadderFilter *This, size_t i, size_t numSamples){
	const float t = (float)i / (float)numSamples;
	BMLadderFilterCoefs c;
	c.G = This->coefs.G + (This->target.G - This->coefs.G) * t;
	c.G4 = c.G * c.G;
	c.G4 *= c.G4;
	c.k = This->coefs.k + (This->target.k - This->coefs.k) * t;
	return c;
}



void BMLadderFilter_processBufferMono(BMLadderFilter *This, const float *input, float *output, size_t numSamples){
	assert(This->numChannels == 1);
	BMLadderFilter_beginBuffer(This);
	
	// state in a local so the compiler keeps it in registers across samples
	BMLadderFilterState s = This->state[0];
	
	if(This->coefs.G == This->target.G && This->coefs.k == This->target.k){
		const BMLadderFilterCoefs c = This->coefs;
		for(size_t i = 0; i < numSamples; i++)
			output[i] = BMLadderFilter_tick(&s, &c, input[i]);
	} else {
		for(size_t i = 0; i < numSamples; i++){
			const BMLadderFilterCoefs c = BMLadderFilter_sweepCoefs(This, i, numSamples);
			output[i] = BMLadderFilter_tick(&s, &c, input[i]);
		}
	}
	
	This->state[0] = s;
}



void BMLadderFilter_processBufferStereo(BMLadderFilter *This,
										const float *inputL, const float *inputR,
										float *outputL, float *outputR,
										size_t numSamples){
	assert(This->numChannels == 2);
	BMLadderFilter_beginBuffer(This);
	
	BMLadderFilterState sL = This->state[0], sR = This->state[1];
	
	if(This->coefs.G == This->target.G && This->coefs.k == This->target.k){
		const BMLadderFilterCoefs c = This->coefs;
		for(size_t i = 0; i < numSamples; i++){
			outputL[i] = BMLadderFilter_tick(&sL, &c, inputL[i]);
			outputR[i] = BMLadderFilter_tick(&sR, &c, inputR[i]);
		}
	} else {
		for(size_t i = 0; i < numSamples; i++){
			const BMLadderFilterCoefs c = BMLadderFilter_sweepCoefs(This, i, numSamples);
			outputL[i] = BMLadderFilter_tick(&sL, &c, inputL[i]);
			outputR[i] = BMLadderFilter_tick(&sR, &c, inputR[i]);
		}
	}
	
	This->state[0] = sL;
	This->state[1] = sR;
}



double BMLadderFilter_tfMag(double fc, double feedback, double sampleRate, double frequency){
	// the trapezoidal stages are the bilinear transform of the analog one-pole
	// with the cutoff prewarped, so the digital response at f is the analog
	// prototype (unit cutoff) at s = jW, W = tan(pi f / fs) / g
	const double g = tan(M_PI * fc / sampleRate);
	const double W = tan(M_PI * fmin(frequency, 0.4999 * sampleRate) / sampleRate) / g;
	const double W2 = W * W;
	// (1 + jW)^4 = (1 - W^2)^2 - 4 W^2 + 4 j W (1 - W^2)
	const double re = (1.0 - W2) * (1.0 - W2) - 4.0 * W2 + feedback;
	const double im = 4.0 * W * (1.0 - W2);
	return 1.0 / sqrt(re * re + im * im);
}



void BMLadderFilter_tfMagVector(BMLadderFilter *This, const float *frequency, float *magnitude, size_t length){
	BMLock_lock(&This->lock);
	const double fc = This->fc_pending, k = This->k_pending;
	BMLock_unlock(&This->lock);
	for(size_t i = 0; i < length; i++)
		magnitude[i] = (float)BMLadderFilter_tfMag(fc, k, This->sampleRate, frequency[i]);
}
