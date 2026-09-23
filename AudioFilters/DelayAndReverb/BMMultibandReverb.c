//
//  BMMultibandReverb.c
//  BMAudioFilters
//
//  Released to the public domain. This file may be used, distributed and
//  modified freely by anyone, for any purpose, without restrictions.
//

#include "BMMultibandReverb.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool BMMultibandReverb_validBand(size_t band){
    bool valid = band < BMMULTIBANDREVERB_NUM_BANDS;
    assert(valid);
    return valid;
}


bool BMMultibandReverb_init(BMMultibandReverb *This, float sampleRate,
                          float maxDelayCapacity_seconds){
    assert(isfinite(sampleRate) && sampleRate > 0.0f);
    assert(isfinite(maxDelayCapacity_seconds) &&
           maxDelayCapacity_seconds >= BMOR_DEFAULT_MAXDELAY);
    memset(This, 0, sizeof(*This));
    This->sampleRate = sampleRate;
    This->maxDelayCapacity_seconds = maxDelayCapacity_seconds;
    float scale = fminf(1.0f, sampleRate / 20000.0f);
    This->crossoverFrequencies[0] = 300.0f * scale;
    This->crossoverFrequencies[1] = 3000.0f * scale;
    This->crossoverFrequencies[2] = 8000.0f * scale;


    // Eight dry band buffers and one reusable stereo wet buffer. The crossover
    // finishes reading both inputs before either output is written, permitting
    // in-place processing without an additional copy of the input.
    This->buffer = malloc(10 * BM_BUFFER_CHUNK_SIZE * sizeof(float));
    if(!This->buffer) return false;
    for(size_t band = 0; band < BMMULTIBANDREVERB_NUM_BANDS; band++){
        This->bandL[band] = This->buffer + (2 * band) * BM_BUFFER_CHUNK_SIZE;
        This->bandR[band] = This->buffer + (2 * band + 1) * BM_BUFFER_CHUNK_SIZE;
        if(!BMOptimizedReverb_init(&This->reverb[band], sampleRate, maxDelayCapacity_seconds)){
            for(size_t i = 0; i <= band; i++) BMOptimizedReverb_free(&This->reverb[i]);
            free(This->buffer);
            This->buffer = NULL;
            return false;
        }
        This->minDelay_seconds[band] = BMOR_DEFAULT_MINDELAY;
        This->maxDelay_seconds[band] = BMOR_DEFAULT_MAXDELAY;
        BMWetDryMixer_init(&This->mixer[band], sampleRate);
        // Start fully dry immediately, rather than fading down from the mixer's
        // fully wet default. Initialise dryMix as well as the wet target.
        This->mixer[band].wetMix = This->mixer[band].mixTarget = 0.0f;
        This->mixer[band].dryMix = 1.0f;
    }
    BMCrossover4way_init(&This->crossover,
                        This->crossoverFrequencies[0],
                        This->crossoverFrequencies[1],
                        This->crossoverFrequencies[2], sampleRate, true, true);
    This->wetL = This->buffer + 8 * BM_BUFFER_CHUNK_SIZE;
    This->wetR = This->buffer + 9 * BM_BUFFER_CHUNK_SIZE;
    return true;
}


void BMMultibandReverb_free(BMMultibandReverb *This){
    if(!This->buffer) return;
    BMCrossover4way_free(&This->crossover);
    for(size_t band = 0; band < BMMULTIBANDREVERB_NUM_BANDS; band++)
        BMOptimizedReverb_free(&This->reverb[band]);
    free(This->buffer);
    This->buffer = This->wetL = This->wetR = NULL;
    for(size_t band = 0; band < BMMULTIBANDREVERB_NUM_BANDS; band++)
        This->bandL[band] = This->bandR[band] = NULL;
}


void BMMultibandReverb_processStereo(BMMultibandReverb *This,
                                   const float *inputL, const float *inputR,
                                   float *outputL, float *outputR,
                                   size_t numSamples){
    if(numSamples == 0) return;
    assert(inputL && inputR && outputL && outputR && outputL != outputR);
    while(numSamples > 0){
        size_t n = BM_MIN(numSamples, BM_BUFFER_CHUNK_SIZE);
        BMCrossover4way_processStereo(&This->crossover, inputL, inputR,
                                     This->bandL[0], This->bandR[0],
                                     This->bandL[1], This->bandR[1],
                                     This->bandL[2], This->bandR[2],
                                     This->bandL[3], This->bandR[3], n);
        for(size_t band = 0; band < BMMULTIBANDREVERB_NUM_BANDS; band++){
            BMOptimizedReverb_process(&This->reverb[band],
                                      This->bandL[band], This->bandR[band],
                                      This->wetL, This->wetR, n);
            BMWetDryMixer_processBufferRandomPhase(&This->mixer[band],
                                                   This->wetL, This->wetR,
                                                   This->bandL[band], This->bandR[band],
                                                   This->wetL, This->wetR, n);
            if(band == 0){
                memcpy(outputL, This->wetL, n * sizeof(float));
                memcpy(outputR, This->wetR, n * sizeof(float));
            } else {
                vDSP_vadd(outputL, 1, This->wetL, 1, outputL, 1, n);
                vDSP_vadd(outputR, 1, This->wetR, 1, outputR, 1, n);
            }
        }
        inputL += n; inputR += n;
        outputL += n; outputR += n;
        numSamples -= n;
    }
}


void BMMultibandReverb_setWet(BMMultibandReverb *This, float wet, size_t band){
    if(!BMMultibandReverb_validBand(band)) return;
    bool valid = isfinite(wet) && wet >= 0.0f && wet <= 1.0f;
    assert(valid);
    if(valid) BMWetDryMixer_setMix(&This->mixer[band], wet);
}


void BMMultibandReverb_setRT60DecayTime(BMMultibandReverb *This, float rt60,
                                     size_t band){
    if(!BMMultibandReverb_validBand(band)) return;
    bool valid = isfinite(rt60) && rt60 > 0.0f;
    assert(valid);
    if(valid) BMOptimizedReverb_setRT60DecayTime(&This->reverb[band], rt60);
}


void BMMultibandReverb_setDelayTimes(BMMultibandReverb *This,
                                   float minDelay_seconds, float maxDelay_seconds,
                                   size_t band){
    if(!BMMultibandReverb_validBand(band)) return;
    bool valid = isfinite(minDelay_seconds) && isfinite(maxDelay_seconds) &&
                 minDelay_seconds > 0.0f && maxDelay_seconds > 2.0f * minDelay_seconds &&
                 maxDelay_seconds <= This->maxDelayCapacity_seconds;
    assert(valid);
    if(!valid) return;
    // Match the underlying reverb's integer-sample constraints before publishing.
    size_t minimum = (size_t)(minDelay_seconds * This->sampleRate);
    size_t maximum = (size_t)(maxDelay_seconds * This->sampleRate);
    valid = minimum >= 2 && maximum >= minimum &&
            maximum - minimum >= This->reverb[band].numDelays - 1;
    assert(valid);
    if(!valid) return;
    if(minDelay_seconds != This->minDelay_seconds[band] ||
       maxDelay_seconds != This->maxDelay_seconds[band]){
        BMOptimizedReverb_setDelayTimes(&This->reverb[band], minDelay_seconds, maxDelay_seconds);
        This->minDelay_seconds[band] = minDelay_seconds;
        This->maxDelay_seconds[band] = maxDelay_seconds;
    }
}


bool BMMultibandReverb_setConfiguration(BMMultibandReverb *This,
                                      const BMOptimizedReverbConfiguration *configuration,
                                      size_t band){
    if(!BMMultibandReverb_validBand(band)) return false;
    if(!BMOptimizedReverb_setConfiguration(&This->reverb[band], configuration)) return false;
    This->minDelay_seconds[band] = configuration->minDelay_seconds;
    This->maxDelay_seconds[band] = configuration->maxDelay_seconds;
    return true;
}


void BMMultibandReverb_setMinDelay(BMMultibandReverb *This,
                                 float minDelay_seconds, size_t band){
    if(!BMMultibandReverb_validBand(band)) return;
    BMMultibandReverb_setDelayTimes(This, minDelay_seconds,
                                   This->maxDelay_seconds[band], band);
}


void BMMultibandReverb_setMaxDelay(BMMultibandReverb *This,
                                 float maxDelay_seconds, size_t band){
    if(!BMMultibandReverb_validBand(band)) return;
    BMMultibandReverb_setDelayTimes(This, This->minDelay_seconds[band],
                                   maxDelay_seconds, band);
}


void BMMultibandReverb_setCrossoverFrequencies(BMMultibandReverb *This,
                                            float cutoff1, float cutoff2,
                                            float cutoff3){
    bool valid = isfinite(cutoff1) && isfinite(cutoff2) && isfinite(cutoff3) &&
                 cutoff1 > 0.0f && cutoff1 < cutoff2 && cutoff2 < cutoff3 &&
                 cutoff3 < 0.5f * This->sampleRate;
    assert(valid);
    if(!valid) return;
    BMCrossover4way_setCutoff1(&This->crossover, cutoff1);
    BMCrossover4way_setCutoff2(&This->crossover, cutoff2);
    BMCrossover4way_setCutoff3(&This->crossover, cutoff3);
    This->crossoverFrequencies[0] = cutoff1;
    This->crossoverFrequencies[1] = cutoff2;
    This->crossoverFrequencies[2] = cutoff3;
}


void BMMultibandReverb_setCrossoverFrequency(BMMultibandReverb *This,
                                           float frequency, size_t crossover){
    assert(crossover < BMMULTIBANDREVERB_NUM_CROSSOVERS);
    if(crossover >= BMMULTIBANDREVERB_NUM_CROSSOVERS) return;
    float f[BMMULTIBANDREVERB_NUM_CROSSOVERS];
    BMMultibandReverb_getCrossoverFrequencies(This, f);
    f[crossover] = frequency;
    BMMultibandReverb_setCrossoverFrequencies(This, f[0], f[1], f[2]);
}


void BMMultibandReverb_getCrossoverFrequencies(const BMMultibandReverb *This,
                                            float frequencies[3]){
    memcpy(frequencies, This->crossoverFrequencies, sizeof(This->crossoverFrequencies));
}


void BMMultibandReverb_tfMagVectors(const BMMultibandReverb *This,
                                  const float *frequencies,
                                  float *magBand1, float *magBand2,
                                  float *magBand3, float *magBand4,
                                  size_t length){
    double g[BMMULTIBANDREVERB_NUM_CROSSOVERS];
    for(size_t c = 0; c < BMMULTIBANDREVERB_NUM_CROSSOVERS; c++)
        g[c] = tan(M_PI * This->crossoverFrequencies[c] / This->sampleRate);
    for(size_t i = 0; i < length; i++){
        bool valid = isfinite(frequencies[i]) && frequencies[i] >= 0.0f &&
                     frequencies[i] <= 0.5f * This->sampleRate;
        assert(valid);
        if(!valid){
            magBand1[i] = magBand2[i] = magBand3[i] = magBand4[i] = NAN;
            continue;
        }
        double lp[3], hp[3];
        double w = M_PI * frequencies[i] / This->sampleRate;
        for(size_t c = 0; c < BMMULTIBANDREVERB_NUM_CROSSOVERS; c++){
            // LR4 = two Butterworth sections. sin/cos form of the bilinear
            // transform avoids tan(pi/2) at Nyquist and cancellation in 1-lp.
            double low = g[c] * cos(w), high = sin(w);
            low *= low; low *= low;
            high *= high; high *= high;
            lp[c] = low / (low + high);
            hp[c] = high / (low + high);
        }
        magBand1[i] = (float)(lp[0] * lp[1] * lp[2]);
        magBand2[i] = (float)(hp[0] * lp[1] * lp[2]);
        magBand3[i] = (float)(hp[0] * hp[1] * lp[2]);
        magBand4[i] = (float)(hp[0] * hp[1] * hp[2]);
    }
}
