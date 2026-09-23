// Released to the public domain. Use, distribute and modify without restrictions.
#include "BMMultibandReverb.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Keep checks active in the NDEBUG build as well.
#define CHECK(x) do { if(!(x)){ fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while(0)

static void near(double actual, double expected, double tolerance){
    if(!isfinite(actual) || fabs(actual - expected) > tolerance){
        fprintf(stderr, "%.12g != %.12g (tolerance %.3g)\n", actual, expected, tolerance);
        abort();
    }
}

static void configure(BMMultibandReverb *r){
    for(size_t b = 0; b < 4; b++){
        BMMultibandReverb_setWet(r, (float)b / 3.0f, b);
        BMMultibandReverb_setRT60DecayTime(r, 0.4f + 0.3f * b, b);
        BMMultibandReverb_setDelayTimes(r, 0.003f + 0.001f * b, 0.05f + 0.01f * b, b);
    }
}

static void processing(float sampleRate){
    enum { N = 4097 };
    BMMultibandReverb a, b;
    CHECK(BMMultibandReverb_init(&a, sampleRate, 0.25f));
    CHECK(BMMultibandReverb_init(&b, sampleRate, 0.25f));
    float x[N], y[N], l[N], r[N], inPlaceL[N], inPlaceR[N];
    for(size_t i = 0; i < N; i++){
        x[i] = 0.2f * sinf(i * 0.123f);
        y[i] = 0.3f * cosf(i * 0.087f);
    }
    // Dry startup must immediately equal the crossover sum, not fade from wet.
    BMCrossover4way ref;
    float fc[3]; BMMultibandReverb_getCrossoverFrequencies(&a, fc);
    CHECK(fc[0] > 0 && fc[0] < fc[1] && fc[1] < fc[2] && fc[2] < sampleRate / 2);
    BMCrossover4way_init(&ref, fc[0], fc[1], fc[2], sampleRate, true, true);
    BMMultibandReverb_processStereo(&a, x, y, l, r, N);
    float bands[8][BM_BUFFER_CHUNK_SIZE];
    for(size_t offset = 0; offset < N;){
        size_t n = BM_MIN(N - offset, BM_BUFFER_CHUNK_SIZE);
        BMCrossover4way_processStereo(&ref, x + offset, y + offset,
                                     bands[0], bands[1], bands[2], bands[3],
                                     bands[4], bands[5], bands[6], bands[7], n);
        for(size_t i = 0; i < n; i++){
            near(l[offset+i], bands[0][i]+bands[2][i]+bands[4][i]+bands[6][i], 2e-7);
            near(r[offset+i], bands[1][i]+bands[3][i]+bands[5][i]+bands[7][i], 2e-7);
        }
        offset += n;
    }
    BMCrossover4way_free(&ref);
    BMMultibandReverb_processStereo(&b, x, y, inPlaceL, inPlaceR, N);
    configure(&a); configure(&b);
    // Longer than scratch storage, including a 1-sample final chunk, wet ramps,
    // live crossover changes, distinct stereo input and identical mono input.
    for(size_t pass = 0; pass < 10; pass++){
        if(pass == 4){
            BMMultibandReverb_setCrossoverFrequency(&a, fc[1] * 0.8f, 1);
            BMMultibandReverb_setCrossoverFrequency(&b, fc[1] * 0.8f, 1);
        }
        const float *right = pass % 2 ? x : y;
        memcpy(inPlaceL, x, sizeof(x)); memcpy(inPlaceR, right, sizeof(y));
        BMMultibandReverb_processStereo(&a, NULL, NULL, NULL, NULL, 0);
        BMMultibandReverb_processStereo(&a, x, right, l, r, N);
        BMMultibandReverb_processStereo(&b, inPlaceL, inPlaceR, inPlaceL, inPlaceR, N);
        for(size_t i = 0; i < N; i++){
            near(l[i], inPlaceL[i], 0); near(r[i], inPlaceR[i], 0);
        }
    }
    // Change one band's bounds; no other band's settings or pending flag change.
    BMMultibandReverb_setMinDelay(&a, 0.008f, 2);
    BMMultibandReverb_setMaxDelay(&a, 0.2f, 2);
    CHECK(a.minDelay_seconds[2] == 0.008f && a.maxDelay_seconds[2] == 0.2f);
    for(size_t band = 0; band < 4; band++) CHECK((a.reverb[band].pendingDelayTimeBits != 0) == (band == 2));
    BMMultibandReverb_processStereo(&a, x, y, l, r, 1);
    CHECK(!a.reverb[2].pendingDelayTimeBits);
    // Repeating the same delay bounds must not clear a sounding tail.
    BMMultibandReverb_setDelayTimes(&a, 0.008f, 0.2f, 2);
    CHECK(!a.reverb[2].pendingDelayTimeBits);
    BMMultibandReverb_free(&a); BMMultibandReverb_free(&b);
}

static void wetReference(void){
    BMMultibandReverb r;
    CHECK(BMMultibandReverb_init(&r, 48000, 0.2f));
    configure(&r);
    float inputL[512] = {0}, inputR[512] = {0}, outputL[512], outputR[512];
    // Let all wet controls settle, keeping every delay line silent.
    for(size_t i = 0; i < 50; i++)
        BMMultibandReverb_processStereo(&r, inputL, inputR, outputL, outputR, 512);
    BMOptimizedReverb reference[4];
    for(size_t b = 0; b < 4; b++){
        CHECK(!r.mixer[b].inTransition);
        near(r.mixer[b].wetMix, (float)b / 3.0f, 0);
        BMOptimizedReverb_init(&reference[b], 48000, 0.2f);
        BMOptimizedReverb_setRT60DecayTime(&reference[b], 0.4f + 0.3f * b);
        BMOptimizedReverb_setDelayTimes(&reference[b], 0.003f + 0.001f * b, 0.05f + 0.01f * b);
    }
    BMCrossover4way crossover;
    BMCrossover4way_init(&crossover, 300, 3000, 8000, 48000, true, true);
    float bands[8][512], wetL[512], wetR[512], expectedL[512], expectedR[512];
    inputL[0] = 1; inputR[0] = -0.5f;
    double tailEnergy = 0;
    for(size_t block = 0; block < 32; block++){
        BMMultibandReverb_processStereo(&r, inputL, inputR, outputL, outputR, 512);
        BMCrossover4way_processStereo(&crossover, inputL, inputR,
                                     bands[0], bands[1], bands[2], bands[3],
                                     bands[4], bands[5], bands[6], bands[7], 512);
        memset(expectedL, 0, sizeof(expectedL)); memset(expectedR, 0, sizeof(expectedR));
        for(size_t b = 0; b < 4; b++){
            BMOptimizedReverb_process(&reference[b], bands[2*b], bands[2*b+1], wetL, wetR, 512);
            float wet = (float)b / 3.0f, dry = sqrtf(1 - wet * wet);
            for(size_t i = 0; i < 512; i++){
                expectedL[i] += dry * bands[2*b][i] + wet * wetL[i];
                expectedR[i] += dry * bands[2*b+1][i] + wet * wetR[i];
            }
        }
        for(size_t i = 0; i < 512; i++){
            near(outputL[i], expectedL[i], 2e-7);
            near(outputR[i], expectedR[i], 2e-7);
            if(block > 20) tailEnergy += outputL[i] * outputL[i] + outputR[i] * outputR[i];
        }
        inputL[0] = inputR[0] = 0;
    }
    CHECK(tailEnergy > 1e-8);
    for(size_t b = 0; b < 4; b++) BMOptimizedReverb_free(&reference[b]);
    BMCrossover4way_free(&crossover); BMMultibandReverb_free(&r);
}

static void plotting(float sampleRate){
    enum { N = 32768, F = 13 };
    float *bands = calloc(8 * N, sizeof(float));
    CHECK(bands);
    for(size_t pass = 0; pass < 2; pass++){
        // Start each measurement with fresh state using the public lifecycle,
        // without depending on the crossover's internal filter layout.
        BMMultibandReverb r;
        CHECK(BMMultibandReverb_init(&r, sampleRate, 0.2f));
        if(pass){
            // Close boundaries expose omitted low/high-pass factors in a plot.
            BMMultibandReverb_setCrossoverFrequencies(&r, 1400, 2100, 2800);
            BMMultibandReverb_setCrossoverFrequency(&r, 2000, 1);
        }
        float fc[3]; BMMultibandReverb_getCrossoverFrequencies(&r, fc);
        float frequencies[F] = {0, 20, 100, fc[0], 900, fc[1], 2500, fc[2],
                               6000, 9000, sampleRate * 0.4f,
                               sampleRate * 0.49f, sampleRate * 0.5f};
        float magnitudes[4][F];
        BMMultibandReverb_tfMagVectors(&r, NULL, NULL, NULL, NULL, NULL, 0);
        BMMultibandReverb_tfMagVectors(&r, frequencies, magnitudes[0], magnitudes[1],
                                     magnitudes[2], magnitudes[3], F);
        float impulse[BM_BUFFER_CHUNK_SIZE] = {1.0f};
        for(size_t offset = 0; offset < N; offset += BM_BUFFER_CHUNK_SIZE){
            BMCrossover4way_processStereo(&r.crossover, impulse, impulse,
                                         bands + offset, bands + N + offset,
                                         bands + 2*N + offset, bands + 3*N + offset,
                                         bands + 4*N + offset, bands + 5*N + offset,
                                         bands + 6*N + offset, bands + 7*N + offset,
                                         BM_BUFFER_CHUNK_SIZE);
            impulse[0] = 0;
        }
        for(size_t b = 0; b < 4; b++) for(size_t f = 0; f < F; f++){
            double real = 0, imag = 0;
            for(size_t i = 0; i < N; i++){
                double phase = 2.0 * M_PI * frequencies[f] * i / sampleRate;
                real += bands[2*b*N+i] * cos(phase);
                imag -= bands[2*b*N+i] * sin(phase);
            }
            near(magnitudes[b][f], hypot(real, imag), 3e-5);
        }
        BMMultibandReverb_free(&r);
    }
    free(bands);
}

static void invalidControls(void){
#ifdef NDEBUG
    BMMultibandReverb r;
    CHECK(BMMultibandReverb_init(&r, 48000, 0.2f));
    BMMultibandReverb_setWet(&r, NAN, 0);
    BMMultibandReverb_setWet(&r, 1.1f, 1);
    BMMultibandReverb_setWet(&r, 0.5f, 4);
    BMMultibandReverb_setRT60DecayTime(&r, 0, 0);
    BMMultibandReverb_setRT60DecayTime(&r, INFINITY, 1);
    BMMultibandReverb_setDelayTimes(&r, 0.06f, 0.1f, 0);
    BMMultibandReverb_setDelayTimes(&r, 0.01f, 0.3f, 1);
    BMMultibandReverb_setMinDelay(&r, -1, 2);
    BMMultibandReverb_setMaxDelay(&r, NAN, 3);
    BMMultibandReverb_setCrossoverFrequencies(&r, 2000, 1000, 3000);
    BMMultibandReverb_setCrossoverFrequency(&r, 24000, 2);
    BMMultibandReverb_setCrossoverFrequency(&r, NAN, 0);
    BMMultibandReverb_setCrossoverFrequency(&r, 100, 3);
    for(size_t b = 0; b < 4; b++){
        CHECK(r.mixer[b].mixTarget == 0);
        CHECK(r.reverb[b].rt60 == BMOR_DEFAULT_RT60);
        CHECK(r.reverb[b].minDelay_seconds == BMOR_DEFAULT_MINDELAY);
        CHECK(r.reverb[b].maxDelay_seconds == BMOR_DEFAULT_MAXDELAY);
        CHECK(!r.reverb[b].pendingDelayTimeBits);
    }
    CHECK(r.crossoverFrequencies[0] == 300 && r.crossoverFrequencies[1] == 3000 && r.crossoverFrequencies[2] == 8000);
    BMMultibandReverb_free(&r);
#endif
}

int main(void){
    const float rates[] = {8000, 44100, 48000, 96000};
    for(size_t i = 0; i < sizeof(rates)/sizeof(rates[0]); i++) processing(rates[i]);
    wetReference();
    plotting(44100); plotting(96000);
    invalidControls();
    puts("BMMultibandReverb: processing, band controls, in-place audio and measured crossover plots passed.");
    return 0;
}
