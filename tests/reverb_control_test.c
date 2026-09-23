// Parameter handoff and delay-distribution regressions for BMOptimizedReverb.
// Build/run commands are in ../BMOptimizedReverb.md. These checks also run with
// ThreadSanitizer or AddressSanitizer; CHECK remains enabled under NDEBUG.
#include "BMOptimizedReverb.h"
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BMOR_NUMDELAYUNITS BMOR_DEFAULT_NUMDELAYUNITS
#define BMOR_NUMDELAYS (4 * BMOR_NUMDELAYUNITS)

#define CHECK(condition) do { if(!(condition)) { \
    fprintf(stderr, "Failed: %s (line %d)\n", #condition, __LINE__); abort(); \
} } while(0)

static void applyPending(BMOptimizedReverb *rv) {
    BMOptimizedReverb_process(rv, NULL, NULL, NULL, NULL, 0);
}

static void checkGains(const BMOptimizedReverb *rv) {
    for(size_t d = 0; d < rv->numDelays; d++) {
        double seconds = (double)(rv->lineLength[d] + 1) / rv->sampleRate;
        float expected = (float)pow(10.0, -3.0 * seconds / rv->rt60);
        CHECK(rv->decayGain[d] == expected);
    }
    for(size_t i = 0; i < rv->numDelayUnits; i++)
        for(size_t q = 0; q < 4; q++)
            CHECK(rv->group[i].gain[q] == rv->decayGain[rv->group[i].writeDelay[q]]);
}

static void testTransitions(void) {
    BMOptimizedReverb rv;
    CHECK(BMOptimizedReverb_init(&rv, 48000, 0.5f));
    float input[64], left[64], right[64];
    for(size_t i = 0; i < 64; i++) input[i] = 0.1f;
    for(int i = 0; i < 100; i++)
        BMOptimizedReverb_process(&rv, input, input, left, right, 64);

    size_t bytes = BMOR_NUMDELAYS * rv.lineCapacity * sizeof(float);
    float *savedLines = malloc(bytes);
    memcpy(savedLines, rv.lines, bytes);
    float previousMix[BMOR_NUMDELAYUNITS][4];
    for(size_t i = 0; i < BMOR_NUMDELAYUNITS; i++)
        memcpy(previousMix[i], rv.group[i].previousMix, sizeof previousMix[i]);
    float oldRT60 = rv.rt60;
    BMOptimizedReverb_setRT60DecayTime(&rv, 0.1f);
    BMOptimizedReverb_setRT60DecayTime(&rv, 3.0f);
    CHECK(rv.rt60 == oldRT60); // setters must not modify active state
    applyPending(&rv);
    CHECK(rv.rt60 == 3.0f);
    CHECK(memcmp(savedLines, rv.lines, bytes) == 0); // RT60 retains the tail
    for(size_t i = 0; i < BMOR_NUMDELAYUNITS; i++)
        CHECK(memcmp(previousMix[i], rv.group[i].previousMix, sizeof previousMix[i]) == 0);
    checkGains(&rv);

    BMOptimizedReverb_setDelayTimes(&rv, 0.001f, 0.02f);
    BMOptimizedReverb_setDelayTimes(&rv, 0.02f, 0.2f);
    CHECK(rv.minDelay_seconds == BMOR_DEFAULT_MINDELAY);
    applyPending(&rv);
    CHECK(rv.minDelay_seconds == 0.02f && rv.maxDelay_seconds == 0.2f);
    checkGains(&rv);
    for(size_t d = 0; d < BMOR_NUMDELAYS; d++)
        for(size_t k = 0; k < rv.lineLength[d]; k++)
            CHECK(rv.lines[rv.lineStart[d] + k] == 0.0f);
    for(size_t i = 0; i < BMOR_NUMDELAYUNITS; i++)
        for(size_t q = 0; q < 4; q++) CHECK(rv.group[i].previousMix[q] == 0.0f);

    // A later request must still work after the previous one was consumed.
    BMOptimizedReverb_setRT60DecayTime(&rv, 0.7f);
    applyPending(&rv);
    CHECK(rv.rt60 == 0.7f);
    checkGains(&rv);
#ifdef NDEBUG
    BMOptimizedReverb_setRT60DecayTime(&rv, NAN);
    BMOptimizedReverb_setRT60DecayTime(&rv, INFINITY);
    BMOptimizedReverb_setRT60DecayTime(&rv, 0.0f);
    BMOptimizedReverb_setDelayTimes(&rv, NAN, 0.2f);
    BMOptimizedReverb_setDelayTimes(&rv, 0.001f, INFINITY);
    BMOptimizedReverb_setDelayTimes(&rv, 0.001f, 0.6f);
    BMOptimizedReverb_setDelayTimes(&rv, 0.1f, 0.15f);
    BMOptimizedReverb_setDelayTimes(&rv, 2.0f / 48000, 36.0f / 48000); // only 35 positions
    applyPending(&rv);
    CHECK(rv.rt60 == 0.7f);
    CHECK(rv.minDelay_seconds == 0.02f && rv.maxDelay_seconds == 0.2f);
#endif
    free(savedLines);
    BMOptimizedReverb_free(&rv);
}

static void testDelayDistribution(void) {
    const float rates[] = {8000, 44100, 48000, 96000, 192000};
    const size_t counts[] = {1, 2, 3, 4, 8, 9, 16};
    for(size_t unit = 0; unit < sizeof counts / sizeof *counts; unit++) {
      for(size_t rate = 0; rate < sizeof rates / sizeof *rates; rate++) {
        BMOptimizedReverb rv;
        CHECK(BMOptimizedReverb_initWithNumDelayUnits(&rv, rates[rate], 0.5f, counts[unit]));
        for(unsigned i = 0; i < 400; i++) {
            // Narrow ranges through the capacity limit at each sample rate.
            float maxTime = i == 399 ? 0.5f : 0.01f + (float)i * 0.0012f;
            float minTime = maxTime * (0.1f + (float)(i % 79) * 0.005f);
            BMOptimizedReverb_setDelayTimes(&rv, minTime, maxTime);
            applyPending(&rv);
            size_t low = (size_t)(minTime * rates[rate]);
            size_t high = (size_t)(maxTime * rates[rate]);
            size_t sum = 0, actualLow = SIZE_MAX, actualHigh = 0;
            int signSum[2] = {0, 0};
            for(size_t d = 0; d < rv.numDelays; d++) {
                size_t n = rv.lineLength[d] + 1;
                CHECK(n >= low && n <= high);
                CHECK(rv.lineLength[d] <= rv.lineCapacity);
                sum += n;
                if(n < actualLow) actualLow = n;
                if(n > actualHigh) actualHigh = n;
                CHECK(rv.outputSign[d] == 1 || rv.outputSign[d] == -1);
                signSum[d % 2] += (int)rv.outputSign[d];
                for(size_t other = 0; other < d; other++)
                    CHECK(rv.lineLength[other] != rv.lineLength[d]);
            }
            CHECK(actualLow == low && actualHigh == high);
            CHECK(sum == (rv.numDelays / 2) * (low + high));
            CHECK(signSum[0] == 0 && signSum[1] == 0);
            size_t lengths[rv.numDelays];
            float signs[rv.numDelays];
            memcpy(lengths, rv.lineLength, sizeof lengths);
            memcpy(signs, rv.outputSign, sizeof signs);
            BMOptimizedReverb_setDelayTimes(&rv, minTime, maxTime);
            applyPending(&rv);
            CHECK(memcmp(lengths, rv.lineLength, sizeof lengths) == 0);
            CHECK(memcmp(signs, rv.outputSign, sizeof signs) == 0);
        }
        BMOptimizedReverb_free(&rv);
      }
    }
}

// Use a power-of-two rate so these exact integer-sample boundaries survive
// float conversion unchanged: 36 positions succeed, 35 fail without widening.
static void testRangeBoundary(void) {
    BMOptimizedReverb rv;
    CHECK(BMOptimizedReverb_init(&rv, 65536, 0.5f));
    BMOptimizedReverb_setDelayTimes(&rv, 2.0f / 65536, 37.0f / 65536);
    applyPending(&rv);
    bool seen[BMOR_NUMDELAYS] = {false};
    for(size_t d = 0; d < BMOR_NUMDELAYS; d++) {
        size_t n = rv.lineLength[d] + 1;
        CHECK(n >= 2 && n <= 37);
        CHECK(!seen[n - 2]);
        seen[n - 2] = true;
    }
#ifdef NDEBUG
    BMOptimizedReverb_setDelayTimes(&rv, 2.0f / 65536, 36.0f / 65536);
    applyPending(&rv);
    CHECK(rv.maxDelay_seconds == 37.0f / 65536);
    BMOptimizedReverb invalid;
    CHECK(!BMOptimizedReverb_initWithNumDelayUnits(&invalid, 48000, 0.5f, 0));
    BMOptimizedReverb_free(&invalid);
    CHECK(!BMOptimizedReverb_initWithNumDelayUnits(&invalid, 48000, 0.5f, SIZE_MAX));
    BMOptimizedReverb_free(&invalid);
    CHECK(!BMOptimizedReverb_init(&invalid, 300, 0.5f)); // default range too narrow
    BMOptimizedReverb_free(&invalid);
    CHECK(!BMOptimizedReverb_init(&invalid, 48000, 0.05f)); // cannot fit defaults
    BMOptimizedReverb_free(&invalid);
    CHECK(!BMOptimizedReverb_init(&invalid, NAN, 0.5f));
    BMOptimizedReverb_free(&invalid);
#endif
    BMOptimizedReverb_free(&rv);
}

typedef struct {
    BMOptimizedReverb reverb;
    atomic_int finished;
} ConcurrentTest;

static void *writeRT60(void *context) {
    ConcurrentTest *test = context;
    for(int i = 0; i < 20000; i++)
        BMOptimizedReverb_setRT60DecayTime(&test->reverb, 0.1f + (float)(i % 100) * 0.1f);
    BMOptimizedReverb_setRT60DecayTime(&test->reverb, 2.5f);
    atomic_fetch_add(&test->finished, 1);
    return NULL;
}

static void *writeDelays(void *context) {
    ConcurrentTest *test = context;
    for(int i = 0; i < 20000; i++) {
        if(i % 2) BMOptimizedReverb_setDelayTimes(&test->reverb, 0.001f, 0.02f);
        else BMOptimizedReverb_setDelayTimes(&test->reverb, 0.04f, 0.11f);
    }
    BMOptimizedReverb_setDelayTimes(&test->reverb, 0.018f, 0.18f);
    atomic_fetch_add(&test->finished, 1);
    return NULL;
}

static void testConcurrentSetters(void) {
    ConcurrentTest test;
    CHECK(BMOptimizedReverb_init(&test.reverb, 48000, 0.5f));
    atomic_init(&test.finished, 0);
    pthread_t rt60Thread, delayThread;
    CHECK(pthread_create(&rt60Thread, NULL, writeRT60, &test) == 0);
    CHECK(pthread_create(&delayThread, NULL, writeDelays, &test) == 0);
    float input[64] = {0.1f}, left[64], right[64];
    do {
        BMOptimizedReverb_process(&test.reverb, input, input, left, right, 64);
        float low = test.reverb.minDelay_seconds, high = test.reverb.maxDelay_seconds;
        CHECK((low == BMOR_DEFAULT_MINDELAY && high == BMOR_DEFAULT_MAXDELAY) ||
              (low == 0.001f && high == 0.02f) || (low == 0.04f && high == 0.11f) ||
              (low == 0.018f && high == 0.18f));
        checkGains(&test.reverb);
        for(size_t k = 0; k < 64; k++) CHECK(isfinite(left[k]) && isfinite(right[k]));
    } while(atomic_load(&test.finished) != 2);
    CHECK(pthread_join(rt60Thread, NULL) == 0);
    CHECK(pthread_join(delayThread, NULL) == 0);
    applyPending(&test.reverb);
    CHECK(test.reverb.rt60 == 2.5f);
    CHECK(test.reverb.minDelay_seconds == 0.018f && test.reverb.maxDelay_seconds == 0.18f);
    checkGains(&test.reverb);
    BMOptimizedReverb_free(&test.reverb);
}

static BMOptimizedReverbConfiguration completeRequest(unsigned i) {
    BMOptimizedReverbConfiguration c = i % 2 ?
        (BMOptimizedReverbConfiguration){.001f, .02f, .4f, 17, 3} :
        (BMOptimizedReverbConfiguration){.04f, .11f, 2.5f, 93, 6};
    return c;
}

static void *writeConfigurations(void *context) {
    ConcurrentTest *test = context;
    for(unsigned i = 0; i < 50000; i++) {
        BMOptimizedReverbConfiguration c = completeRequest(i);
        CHECK(BMOptimizedReverb_setConfiguration(&test->reverb, &c));
    }
    atomic_store(&test->finished, 1);
    return NULL;
}

static void testCompleteConfigurations(void) {
    ConcurrentTest test;
    BMOptimizedReverb *rv = &test.reverb;
    CHECK(BMOptimizedReverb_init(rv, 48000, .5f));
    BMOptimizedReverbConfiguration c = completeRequest(0);
    CHECK(BMOptimizedReverb_setConfiguration(rv, &c));
    CHECK(rv->rt60 == BMOR_DEFAULT_RT60); // request is still pending
    BMOptimizedReverbConfiguration invalid = c;
    invalid.signPattern = BMOR_NUM_SIGN_PATTERNS;
    CHECK(!BMOptimizedReverb_setConfiguration(rv, &invalid));
    applyPending(rv); // invalid request did not displace the good one
    CHECK(rv->rt60 == c.rt60 && rv->delaySeed == c.delaySeed);

    size_t lengths[BMOR_NUMDELAYS];
    float evenSigns[BMOR_NUMDELAYS];
    memcpy(lengths, rv->lineLength, sizeof lengths);
    for(uint32_t pattern = 0; pattern < BMOR_NUM_SIGN_PATTERNS; pattern++) {
        c.signPattern = pattern;
        CHECK(BMOptimizedReverb_setConfiguration(rv, &c));
        applyPending(rv);
        CHECK(memcmp(lengths, rv->lineLength, sizeof lengths) == 0);
        int balance[2] = {0};
        for(size_t d = 0; d < rv->numDelays; d++) {
            balance[d%2] += (int)rv->outputSign[d];
            if(pattern % 2) CHECK(rv->outputSign[d] == -evenSigns[d]);
            else evenSigns[d] = rv->outputSign[d];
        }
        CHECK(balance[0] == 0 && balance[1] == 0);
    }

    atomic_init(&test.finished, 0);
    pthread_t writer;
    CHECK(pthread_create(&writer, NULL, writeConfigurations, &test) == 0);
    float input[64] = {.1f}, left[64], right[64];
    do {
        BMOptimizedReverb_process(rv, input, input, left, right, 64);
        BMOptimizedReverbConfiguration expected = completeRequest(rv->delaySeed == 17 ? 1 : 0);
        CHECK(rv->rt60 == expected.rt60 && rv->minDelay_seconds == expected.minDelay_seconds &&
              rv->maxDelay_seconds == expected.maxDelay_seconds && rv->delaySeed == expected.delaySeed);
        checkGains(rv);
        for(size_t k = 0; k < 64; k++) CHECK(isfinite(left[k]) && isfinite(right[k]));
    } while(!atomic_load(&test.finished));
    CHECK(pthread_join(writer, NULL) == 0);
    applyPending(rv);
    CHECK(rv->delaySeed == 17 && rv->rt60 == .4f);
    BMOptimizedReverb_free(rv);
}

int main(void) {
    testTransitions();
    testDelayDistribution();
    testRangeBoundary();
    testConcurrentSetters();
    testCompleteConfigurations();
    puts("Parameter transitions, 14,000 delay configurations, concurrent setters and complete configurations: passed");
    return 0;
}
