// Benchmark driver for temporary builds with different candidate counts.
// Built by reverb_candidate_benchmark.py; production settings are unchanged.
#include "BMOptimizedReverbBass.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double milliseconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec*1000 + (double)t.tv_nsec/1e6;
}

int main(int argc, char **argv) {
    if(argc != 9) return 1;
    size_t units = (size_t)strtoul(argv[1], NULL, 0);
    BMOptimizedReverbConfiguration request = {
        strtof(argv[2], NULL), strtof(argv[3], NULL), strtof(argv[4], NULL),
        (uint32_t)strtoul(argv[5], NULL, 0), 0
    };
    float highHz = strtof(argv[6], NULL), sampleRate = strtof(argv[7], NULL);
    int repeats = atoi(argv[8]);
    if(repeats < 1 || repeats > 100) return 1;
    BMOptimizedReverb live;
    if(!BMOptimizedReverb_initWithNumDelayUnits(&live, sampleRate,
        fmaxf(.1f, request.maxDelay_seconds), units)) return 2;
    BMOptimizedReverbBassResult result;
    // Warm one full selection before repeated timings. Each measured call
    // still includes all temporary allocation, FFT setup, renders and cleanup.
    if(repeats > 1 && !BMOptimizedReverb_selectBassConfiguration(
        &live, &request, 20, highHz, &result)) return 3;
    double elapsed[100];
    for(int i = 0; i < repeats; i++) {
        double start = milliseconds();
        if(!BMOptimizedReverb_selectBassConfiguration(&live, &request, 20, highHz, &result)) return 3;
        elapsed[i] = milliseconds()-start;
    }
    printf("{\"count\":%d,\"pattern\":%u,\"low_db\":%.12g,\"high_db\":%.12g,\"samples\":%zu,\"times_ms\":[",
           BMOR_NUM_SIGN_PATTERNS, result.configuration.signPattern,
           result.minimumGainDB, result.maximumGainDB, result.impulseSamples);
    for(int i = 0; i < repeats; i++) printf("%s%.6f", i ? "," : "", elapsed[i]);
    printf("],\"scores_db\":[");
    for(size_t i = 0; i < BMOR_NUM_SIGN_PATTERNS; i++)
        printf("%s%.12g", i ? "," : "", result.candidateDeviationDB[i]);
    puts("]}");
    BMOptimizedReverb_free(&live);
}
