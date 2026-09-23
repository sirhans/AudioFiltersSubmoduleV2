// Offline response measurements. Build:
// clang -O2 -I"AudioFilters/DelayAndReverb" tests/reverb_bass_test.c \
//   "AudioFilters/DelayAndReverb/BMOptimizedReverb.c" \
//   -framework Accelerate -o /tmp/reverb_bass_test
// Usage: reverb_bass_test units minSeconds maxSeconds RT60 seed [wet]
// Without wet, append exact gain extrema for all steady mixes from 0 to .5.
// Every measurement starts from silence, followed by a unit mono impulse and
// zeros. Gain is uncorrected. Stereo power means the mean of L and R powers.
#include "BMOptimizedReverb.h"
#include <Accelerate/Accelerate.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Extrema over every steady wet setting w in [0, .5], using the synth's
// dry = sqrt(1-w*w). With w=sin(theta), mixed power is
// (1+B)/2 + (1-B)/2*cos(2*theta) + A*sin(2*theta), where
// A=mean(Re(H)) and B=mean(|H|^2), averaged across both channels and the band.
static void mixedGainRange(double meanReal, double power, double *low, double *high){
    const double pi = acos(-1.0);
    double phase = atan2(2.0 * meanReal, 1.0 - power);
    *low = *high = 1.0; // all dry
    double end = 0.75 + 0.25 * power + sqrt(0.75) * meanReal;
    *low = fmin(*low, end);
    *high = fmax(*high, end);
    for(int k = -2; k <= 2; k++){
        double angle = phase + k * pi;
        if(angle < 0.0 || angle > pi / 3.0) continue;
        double value = (1.0 + power) / 2.0 + (1.0 - power) / 2.0 * cos(angle) + meanReal * sin(angle);
        *low = fmin(*low, value);
        *high = fmax(*high, value);
    }
    *low = 10.0 * log10(*low);
    *high = 10.0 * log10(*high);
}

int main(int argc, char **argv){
    if(argc != 6 && argc != 7) return 1;
    size_t units = (size_t)strtoul(argv[1], NULL, 0);
    float minDelay = strtof(argv[2], NULL), maxDelay = strtof(argv[3], NULL);
    float rt60 = strtof(argv[4], NULL);
    uint32_t seed = (uint32_t)strtoul(argv[5], NULL, 0);
    if(!seed) return 1;
    const float fs = 48000;
    size_t n = 1;
    vDSP_Length log2n = 0;
    double target = fmax(2.0, 2.0 * (double)rt60 + maxDelay) * fs;
    if(!isfinite(target) || target > 8388608) return 1;
    while((double)n < target){ n *= 2; log2n++; }
    float *left = calloc(n, sizeof(float)), *right = calloc(n, sizeof(float));
    float *imaginary = calloc(n, sizeof(float));
    FFTSetup fft = vDSP_create_fftsetup(log2n, kFFTRadix2);
    BMOptimizedReverb probe;
    bool ready = BMOptimizedReverb_initWithNumDelayUnits(&probe, fs,
        fmaxf(BMOR_DEFAULT_MAXDELAY, maxDelay), units);
    if(!left || !right || !imaginary || !fft || !ready) return 1;
    // White-box experiment only: no other thread owns this instance. The next
    // delay update redraws from this seed and clears all state before the pulse.
    probe.delaySeed = seed;
    if(!BMOptimizedReverb_setDelayTimes(&probe, minDelay, maxDelay) ||
       !BMOptimizedReverb_setRT60DecayTime(&probe, rt60)) return 1;
    left[0] = right[0] = 1.0f;
    BMOptimizedReverb_process(&probe, left, right, left, right, n);
    // Optional explicit mixed impulse, useful for independently checking the
    // extrema formula. The dry path is an impulse at sample zero.
    if(argc == 7){
        float wet = strtof(argv[6], NULL);
        if(wet < 0 || wet > 0.5f) return 1;
        float dry = sqrtf(1.0f - wet * wet);
        vDSP_vsmul(left, 1, &wet, left, 1, n);
        vDSP_vsmul(right, 1, &wet, right, 1, n);
        left[0] += dry;
        right[0] += dry;
    }
    double dc[2] = {0,0};
    for(size_t k = 0; k < n; k++){ dc[0] += left[k]; dc[1] += right[k]; }
    const double bands[][2] = {{0,20},{20,50},{20,100},{20,300},{20,2000}};
    double power[5] = {0}, realMean[5] = {0};
    size_t count[5];
    float *channels[2] = {left, right};
    for(size_t ch = 0; ch < 2; ch++){
        memset(imaginary, 0, n * sizeof(float));
        DSPSplitComplex spectrum = {channels[ch], imaginary};
        vDSP_fft_zip(fft, &spectrum, 1, log2n, FFT_FORWARD);
        for(size_t b = 0; b < 5; b++){
            size_t first = (size_t)ceil(bands[b][0] * (double)n / fs);
            size_t last = (size_t)floor(bands[b][1] * (double)n / fs);
            count[b] = last - first + 1;
            for(size_t k = first; k <= last; k++){
                double real = spectrum.realp[k], imag = spectrum.imagp[k];
                power[b] += real*real + imag*imag;
                realMean[b] += real;
            }
        }
    }
    printf("%.9g %.9g %.9g", dc[0], dc[1], 10*log10((dc[0]*dc[0]+dc[1]*dc[1])*.5));
    for(size_t b=0; b<5; b++) printf(" %.9g",10*log10(power[b]/(2.0*(double)count[b])));
    if(argc == 6){
        double low, high;
        mixedGainRange((dc[0]+dc[1])*.5, (dc[0]*dc[0]+dc[1]*dc[1])*.5, &low, &high);
        printf(" %.9g %.9g", low, high);
        for(size_t b=0; b<5; b++){
            mixedGainRange(realMean[b]/(2.0*(double)count[b]), power[b]/(2.0*(double)count[b]), &low, &high);
            printf(" %.9g %.9g",low,high);
        }
    }
    printf("\n");
    vDSP_destroy_fftsetup(fft);
    BMOptimizedReverb_free(&probe);
    free(left); free(right); free(imaginary);
}
