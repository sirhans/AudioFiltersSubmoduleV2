// Released to the public domain. Use, distribute and modify without restrictions.
#include "BMOptimizedReverbBass.h"
#include <Accelerate/Accelerate.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// With A = mean(Re(H)), B = mean(|H|^2), the mixed power is
// 1 + w*w*(B-1) + 2*w*sqrt(1-w*w)*A. Substituting w = sin(theta)
// gives a sinusoid in 2*theta. Check its endpoints and stationary points;
// no wet-value grid is necessary, and dry/wet interference is retained.
static void mixedGainRange(double a, double b, double *lowDB, double *highDB){
    const double pi = acos(-1.0);
    double end = .75 + .25*b + sqrt(.75)*a;
    double low = fmin(1.0, end), high = fmax(1.0, end);
    double phase = atan2(2*a, 1-b);
    for(int k = -1; k <= 1; k++){
        double angle = phase + k*pi;
        if(angle < 0 || angle > pi/3) continue;
        double p = (1+b)/2 + (1-b)/2*cos(angle) + a*sin(angle);
        low = fmin(low, p);
        high = fmax(high, p);
    }
    // Clamp numerical cancellation at zero; a true zero has -infinite gain.
    *lowDB = 10*log10(fmax(0, low));
    *highDB = 10*log10(high);
}

bool BMOptimizedReverb_selectBassConfiguration(const BMOptimizedReverb *This,
    const BMOptimizedReverbConfiguration *requested, float lowHz, float highHz,
    BMOptimizedReverbBassResult *result){
    if(!result || !BMOptimizedReverb_configurationIsValid(This, requested) ||
       !isfinite(lowHz) || !isfinite(highHz) || lowHz < 0 ||
       highHz <= lowHz || highHz > This->sampleRate*.5f) return false;

    double target = fmax(2, 2*(double)requested->rt60 + requested->maxDelay_seconds) * This->sampleRate;
    if(!isfinite(target) || target > 8388608) return false;
    size_t n = 1;
    vDSP_Length log2n = 0;
    while((double)n < target){ n *= 2; log2n++; }
    size_t first = (size_t)ceil((double)lowHz * (double)n / This->sampleRate);
    size_t last = (size_t)floor((double)highHz * (double)n / This->sampleRate);
    if(first > last) return false;

    BMOptimizedReverb probe;
    if(!BMOptimizedReverb_initWithNumDelayUnits(&probe, This->sampleRate,
        This->maxDelayCapacity_seconds, This->numDelayUnits)) return false;
    float *storage = calloc(3*n, sizeof(float));
    FFTSetup fft = vDSP_create_fftsetup(log2n, kFFTRadix2);
    if(!storage || !fft){
        free(storage);
        if(fft) vDSP_destroy_fftsetup(fft);
        BMOptimizedReverb_free(&probe);
        return false;
    }
    float *channels[2] = {storage, storage+n};
    float *imaginary = storage+2*n;
    BMOptimizedReverbBassResult best = {0};
    best.worstDeviationDB = INFINITY;
    best.impulseSamples = n;
    bool finite = true;

    for(uint32_t pattern = 0; pattern < BMOR_NUM_SIGN_PATTERNS; pattern += 2){
        BMOptimizedReverbConfiguration candidate = *requested;
        candidate.signPattern = pattern;
        // Publishing the complete request ensures this probe runs the same
        // setup path as the live audio thread, including its buffer reset.
        BMOptimizedReverb_setConfiguration(&probe, &candidate);
        memset(storage, 0, 2*n*sizeof(float));
        channels[0][0] = channels[1][0] = 1;
        BMOptimizedReverb_process(&probe, channels[0], channels[1], channels[0], channels[1], n);

        double a = 0, b = 0, dc[2] = {0};
        for(size_t channel = 0; channel < 2; channel++){
            for(size_t k = 0; k < n; k++) dc[channel] += channels[channel][k];
            memset(imaginary, 0, n*sizeof(float));
            DSPSplitComplex spectrum = {channels[channel], imaginary};
            vDSP_fft_zip(fft, &spectrum, 1, log2n, FFT_FORWARD);
            for(size_t k = first; k <= last; k++){
                double real = spectrum.realp[k], imag = spectrum.imagp[k];
                a += real;
                b += real*real + imag*imag;
            }
        }
        double bins = 2.0 * (double)(last-first+1);
        a /= bins;
        b /= bins;
        finite = finite && isfinite(a) && isfinite(b) && isfinite(dc[0]) && isfinite(dc[1]);
        for(uint32_t inverse = 0; inverse < 2; inverse++){
            double sign = inverse ? -1 : 1;
            double low, high;
            mixedGainRange(sign*a, b, &low, &high);
            double deviation = fmax(fabs(low), fabs(high));
            best.candidateDeviationDB[pattern+inverse] = deviation;
            if(pattern+inverse == 0 || deviation < best.worstDeviationDB){
                best.configuration = candidate;
                best.configuration.signPattern += inverse;
                best.minimumGainDB = low;
                best.maximumGainDB = high;
                best.worstDeviationDB = deviation;
                best.dcLeft = sign*dc[0];
                best.dcRight = sign*dc[1];
            }
        }
    }
    vDSP_destroy_fftsetup(fft);
    free(storage);
    BMOptimizedReverb_free(&probe);
    if(!finite) return false;
    *result = best;
    return true;
}
