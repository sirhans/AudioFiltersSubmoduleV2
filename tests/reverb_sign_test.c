// Offline sign-selection regression and measurement tool.
// See BMOptimizedReverb.md for build commands. No arguments runs regressions.
// Measurement: units minSeconds maxSeconds RT60 seed highHz [sampleRate]
#include "BMOptimizedReverbBass.h"
#include <Accelerate/Accelerate.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr, "Failed: %s at %d\n", #x, __LINE__); abort(); } } while(0)

// Independent check: actually mix the dry impulse before the FFT, for every
// candidate, and sweep 201 wet values. Use twice the selector's FFT length
// to also check tail truncation and frequency-grid sensitivity.
static void checkExplicitMix(BMOptimizedReverb *rv, const BMOptimizedReverbBassResult *result,
                             float highHz) {
    size_t n = 2*result->impulseSamples;
    float *data = calloc(4*n, sizeof(float));
    CHECK(data);
    float *left = data, *right = data+n, *real = data+2*n, *imag = data+3*n;
    vDSP_Length log2n = 0;
    for(size_t i = n; i > 1; i /= 2) log2n++;
    FFTSetup fft = vDSP_create_fftsetup(log2n, kFFTRadix2);
    CHECK(fft);
    size_t first = (size_t)ceil(20.0*(double)n/rv->sampleRate);
    size_t last = (size_t)floor((double)highHz*(double)n/rv->sampleRate);
    double selectedScore = INFINITY;
    for(uint32_t pattern = 0; pattern < BMOR_NUM_SIGN_PATTERNS; pattern++) {
        BMOptimizedReverbConfiguration c = result->configuration;
        c.signPattern = pattern;
        CHECK(BMOptimizedReverb_setConfiguration(rv, &c));
        memset(data, 0, 2*n*sizeof(float));
        left[0] = right[0] = 1;
        BMOptimizedReverb_process(rv, left, right, left, right, n);
        double a = 0, b = 0;
        // FFT explicit mixed responses at two nonzero wet values. Separately
        // derive A/B from the wet FFT for a dense mix sweep at this resolution.
        for(size_t channel = 0; channel < 2; channel++) {
            memcpy(real, data+channel*n, n*sizeof(float));
            memset(imag, 0, n*sizeof(float));
            DSPSplitComplex spectrum = {real, imag};
            vDSP_fft_zip(fft, &spectrum, 1, log2n, FFT_FORWARD);
            double ca = 0, cb = 0;
            for(size_t k = first; k <= last; k++) {
                ca += real[k];
                cb += (double)real[k]*real[k]+(double)imag[k]*imag[k];
            }
            ca /= (double)(last-first+1);
            cb /= (double)(last-first+1);
            a += ca*.5; b += cb*.5;
            for(int mix = 1; mix <= 2; mix++) {
                float wet = (float)mix*.25f, dry = sqrtf(1-wet*wet);
                for(size_t k = 0; k < n; k++) real[k] = wet*data[channel*n+k];
                real[0] += dry;
                memset(imag, 0, n*sizeof(float));
                vDSP_fft_zip(fft, &spectrum, 1, log2n, FFT_FORWARD);
                double explicitPower = 0;
                for(size_t k = first; k <= last; k++)
                    explicitPower += (double)real[k]*real[k]+(double)imag[k]*imag[k];
                explicitPower /= (double)(last-first+1);
                double expected = dry*dry + wet*wet*cb + 2*wet*dry*ca;
                CHECK(fabs(explicitPower-expected) < 2e-5*fmax(1, expected));
            }
        }
        double worst = 0;
        for(int mix = 0; mix <= 200; mix++) {
            double wet = (double)mix/400;
            double power = 1+wet*wet*(b-1)+2*wet*sqrt(1-wet*wet)*a;
            worst = fmax(worst, fabs(10*log10(power)));
        }
        CHECK(fabs(worst-result->candidateDeviationDB[pattern]) < .03);
        selectedScore = fmin(selectedScore, result->candidateDeviationDB[pattern]);
    }
    CHECK(selectedScore == result->worstDeviationDB);
    CHECK(result->candidateDeviationDB[result->configuration.signPattern] == selectedScore);
    vDSP_destroy_fftsetup(fft);
    free(data);
}

static void regressions(void) {
    BMOptimizedReverb rv;
    CHECK(BMOptimizedReverb_init(&rv, 48000, .5f));
    const BMOptimizedReverbConfiguration cases[] = {
        {.018f, .18f, 1.8f, 0x9E3779B9u, 0},
        {.0018f, .018f, .18f, 2, 0},
        {.001f, .02f, 1.8f, 1, 0},
    };
    for(size_t i = 0; i < sizeof cases/sizeof cases[0]; i++) {
        BMOptimizedReverbBassResult result;
        float high = i == 1 ? 50 : 300;
        CHECK(BMOptimizedReverb_selectBassConfiguration(&rv, &cases[i], 20, high, &result));
        CHECK(result.worstDeviationDB <= result.candidateDeviationDB[0]);
        CHECK(rv.rt60 == BMOR_DEFAULT_RT60); // analysis did not touch live state
        if(i == 2) CHECK(result.worstDeviationDB > 1.75); // poor results still succeed
        checkExplicitMix(&rv, &result, high);
        BMOptimizedReverb_free(&rv);
        CHECK(BMOptimizedReverb_init(&rv, 48000, .5f));
    }
    BMOptimizedReverbBassResult untouched;
    memset(&untouched, 0x5a, sizeof untouched);
    BMOptimizedReverbBassResult result = untouched;
    CHECK(!BMOptimizedReverb_selectBassConfiguration(&rv, &cases[0], 300, 20, &result));
    CHECK(memcmp(&result, &untouched, sizeof result) == 0);
    BMOptimizedReverbConfiguration huge = cases[0];
    huge.rt60 = 1000000;
    CHECK(!BMOptimizedReverb_selectBassConfiguration(&rv, &huge, 20, 300, &result));
    CHECK(memcmp(&result, &untouched, sizeof result) == 0);
    BMOptimizedReverb_free(&rv);
    printf("Best-of-%d selection, explicit dry mixing, double-length FFT and non-rejection: passed\n", BMOR_NUM_SIGN_PATTERNS);
}

int main(int argc, char **argv) {
    if(argc == 1) { regressions(); return 0; }
    if(argc != 7 && argc != 8) return 1;
    size_t units = (size_t)strtoul(argv[1], NULL, 0);
    BMOptimizedReverbConfiguration c = {strtof(argv[2], NULL), strtof(argv[3], NULL),
        strtof(argv[4], NULL), (uint32_t)strtoul(argv[5], NULL, 0), 0};
    float high = strtof(argv[6], NULL);
    float fs = argc == 8 ? strtof(argv[7], NULL) : 48000;
    BMOptimizedReverb rv;
    CHECK(BMOptimizedReverb_initWithNumDelayUnits(&rv, fs, fmaxf(.1f, c.maxDelay_seconds), units));
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    BMOptimizedReverbBassResult result;
    CHECK(BMOptimizedReverb_selectBassConfiguration(&rv, &c, 20, high, &result));
    clock_gettime(CLOCK_MONOTONIC, &end);
    double ms = 1000*(double)(end.tv_sec-start.tv_sec)+(double)(end.tv_nsec-start.tv_nsec)/1e6;
    printf("{\"pattern\":%u,\"before\":%.9g,\"after\":%.9g,\"low\":%.9g,\"high\":%.9g,\"milliseconds\":%.6g,\"candidates\":[",
        result.configuration.signPattern, result.candidateDeviationDB[0], result.worstDeviationDB,
        result.minimumGainDB, result.maximumGainDB, ms);
    for(size_t i = 0; i < BMOR_NUM_SIGN_PATTERNS; i++)
        printf("%s%.9g", i ? "," : "", result.candidateDeviationDB[i]);
    puts("]}");
    BMOptimizedReverb_free(&rv);
}
