//
//  reverb_bench.c
//
//  Checks BMOptimizedReverb against BMReverb and times the two.
//
//    1. Same output: BMReverb set up as the synth's late reflections run it
//       (9 delay units, fully wet, tone filter bypassed, width 1, both decay
//       multipliers 1) against BMOptimizedReverb given BMReverb's delay
//       lengths and output signs.
//    2. BMOptimizedReverb_process (blocks) against the same network run
//       sample by sample (processReference below), with its own
//       delay lengths, at several buffer sizes, in place and not.
//    3. Time per sample of each, at the synth's buffer sizes.
//
//  Build and run (from the repository's root):
//
//    python3 tests/check-optimized-reverb.py --benchmark
//
//  `reverb_bench profile <which> <seconds>` only runs one of them for that long
//  (which: bmreverb, reference, optimized), for a time profiler to look at.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mach/mach_time.h>
#include "BMReverb.h"
#include "BMOptimizedReverb.h"

#define BMOR_NUMDELAYUNITS BMOR_DEFAULT_NUMDELAYUNITS
#define BMOR_NUMDELAYS (4 * BMOR_NUMDELAYUNITS)

#define FS 48000.0f
#define RT60 1.4f
#define MIN_DELAY 0.014f
#define MAX_DELAY 0.140f

static double now(void){
	static mach_timebase_info_data_t tb;
	if(tb.denom == 0) mach_timebase_info(&tb);
	return (double)mach_absolute_time() * (double)tb.numer / (double)tb.denom * 1e-9;
}

// white noise for the first half second, then silence: a burst and its tail
static void makeInput(float *l, float *r, size_t n){
	uint32_t s = 12345;
	for(size_t i = 0; i < n; i++){
		s = s * 1664525u + 1013904223u; float a = (float)(s >> 8) / 8388608.0f - 1.0f;
		s = s * 1664525u + 1013904223u; float b = (float)(s >> 8) / 8388608.0f - 1.0f;
		int on = i < (size_t)(FS / 2);
		l[i] = on ? a * 0.5f : 0.0f;
		r[i] = on ? b * 0.5f : 0.0f;
	}
}

static void setUpBMReverb(BMReverb *rv){
	BMReverbInit(rv, FS);
	BMReverbSetNumDelayUnits(rv, BMOR_NUMDELAYUNITS);
	BMReverbSetRoomSize(rv, MIN_DELAY, MAX_DELAY);
	BMReverbSetRT60DecayTime(rv, RT60);
	BMReverbSetHFDecayMultiplier(rv, 1.0f);
	BMReverbSetLFDecayMultiplier(rv, 1.0f);
	BMReverbSetWetMix(rv, 1.0f);
	BMReverbSetStereoWidth(rv, 1.0f);
	BMReverbSetWetFilterBypass(rv, true);
	// the delays are rebuilt at the end of the next buffer
	float z[64] = {0}, o1[64], o2[64];
	BMReverbProcessBuffer(rv, z, z, o1, o2, 64);
	// the wet/dry mixer fades to its mix: run it there
	float *zz = calloc(48000, sizeof(float)), *oo = calloc(96000, sizeof(float));
	for(int i = 0; i < 4; i++) BMReverbProcessBuffer(rv, zz, zz, oo, oo + 48000, 12000);
	free(zz); free(oo);
}

static void setUpOptimized(BMOptimizedReverb *rv){
	if(!BMOptimizedReverb_init(rv, FS, 0.5f)) abort();
	BMOptimizedReverb_setDelayTimes(rv, MIN_DELAY, MAX_DELAY);
	BMOptimizedReverb_setRT60DecayTime(rv, RT60);
}

/*
 * BMOptimizedReverb's network run one sample at a time, the way BMReverb goes through its own:
 * what BMOptimizedReverb_process has to agree with to the bit. On the same struct, so either can
 * carry on from the other. The groups are taken in BMOptimizedReverb_process's order (last to
 * first), so that the output's sums round the same way.
 */
static void processReference(BMOptimizedReverb *This, const float *inputL, const float *inputR,
							 float *outputL, float *outputR, size_t numSamples){
	// Apply pending settings through the production handoff, without processing audio.
	BMOptimizedReverb_process(This, inputL, inputR, outputL, outputR, 0);

	for(size_t t = 0; t < numSamples; t++){
		float in[2] = { This->inputAttenuation * inputL[t], This->inputAttenuation * inputR[t] };
		float r[This->numDelays], out[2] = { 0.0f, 0.0f };
		float feedback[This->numDelays];
		for(size_t i = 0; i < This->numDelayUnits; i++) for(size_t q = 0; q < 4; q++) feedback[This->group[i].writeDelay[q]] = This->group[i].previousMix[q];
		size_t untilWrap = (size_t)-1;
		for(size_t d = 0; d < This->numDelays; d++){
			float *line = This->lines + This->lineStart[d];
			r[d] = line[This->pos[d]];
			line[This->pos[d]] = (in[d & 1] + feedback[d]) * This->decayGain[d];
			if(++This->pos[d] == This->lineLength[d]) This->pos[d] = 0;
			untilWrap = (This->lineLength[d] - This->pos[d] < untilWrap ? This->lineLength[d] - This->pos[d] : untilWrap);
		}
		This->untilWrap = untilWrap;
		// the groups in the block version's order, so that the sums round the same way
		for(size_t i = This->numDelayUnits; i-- > 0; ){
			float a = r[i], b = r[This->numDelayUnits + i], c = r[2*This->numDelayUnits + i], d = r[3*This->numDelayUnits + i];
			out[i & 1] += This->outputSign[i] * a + This->outputSign[2*This->numDelayUnits + i] * c;
			out[(i + This->numDelayUnits) & 1] += This->outputSign[This->numDelayUnits + i] * b + This->outputSign[3*This->numDelayUnits + i] * d;
			float t0 = a + c, t1 = b + d, t2 = a - c, t3 = b - d;
			This->group[i].previousMix[0] = 0.5f * (t0 + t1);
			This->group[i].previousMix[1] = 0.5f * (t0 - t1);
			This->group[i].previousMix[2] = 0.5f * (t2 + t3);
			This->group[i].previousMix[3] = 0.5f * (t2 - t3);
		}
		outputL[t] = out[0];
		outputR[t] = out[1];
	}
}

typedef enum { kBMReverb, kReference, kOptimized } Which;

static void run(Which which, BMReverb *a, BMOptimizedReverb *b, const float *inL, const float *inR,
				float *outL, float *outR, size_t n, size_t buffer){
	for(size_t done = 0; done < n; done += buffer){
		size_t m = n - done < buffer ? n - done : buffer;
		if(which == kBMReverb) BMReverbProcessBuffer(a, inL + done, inR + done, outL + done, outR + done, m);
		else if(which == kReference) processReference(b, inL + done, inR + done, outL + done, outR + done, m);
		else BMOptimizedReverb_process(b, inL + done, inR + done, outL + done, outR + done, m);
	}
}

static double maxDifference(const float *a, const float *b, size_t n, double *peak){
	double d = 0; *peak = 0;
	for(size_t i = 0; i < n; i++){
		double x = fabs((double)a[i] - (double)b[i]);
		if(x > d) d = x;
		if(fabs(a[i]) > *peak) *peak = fabs(a[i]);
	}
	return d;
}

int main(int argc, char **argv){
	const size_t n = (size_t)(FS * 4);
	float *inL = malloc(n * sizeof(float)), *inR = malloc(n * sizeof(float));
	float *aL = malloc(n * sizeof(float)), *aR = malloc(n * sizeof(float));
	float *bL = malloc(n * sizeof(float)), *bR = malloc(n * sizeof(float));
	makeInput(inL, inR, n);

	BMReverb bm; setUpBMReverb(&bm);
	BMOptimizedReverb opt; setUpOptimized(&opt);

	if(argc >= 4 && strcmp(argv[1], "profile") == 0){
		Which which = strcmp(argv[2], "bmreverb") == 0 ? kBMReverb : strcmp(argv[2], "reference") == 0 ? kReference : kOptimized;
		double until = now() + atof(argv[3]);
		size_t passes = 0;
		while(now() < until){ run(which, &bm, &opt, inL, inR, aL, aR, n, 64); passes++; }
		printf("%zu passes of %zu samples\n", passes, n);
		return 0;
	}

	int failures = 0;
	double peak;

	// 1. BMOptimizedReverb is BMReverb
	{
		float signs[BMOR_NUMDELAYS];
		memcpy(signs, bm.delayOutputSigns, sizeof signs);
		BMOptimizedReverb same; setUpOptimized(&same);
		float z[8] = {0}, o[16];
		BMOptimizedReverb_process(&same, z, z, o, o + 8, 8);   // takes the delay times, so the test lengths are not drawn over
		BMOptimizedReverb_setDelayLengthsForTest(&same, bm.bufferLengths, signs);
		run(kBMReverb, &bm, NULL, inL, inR, aL, aR, n, 512);
		run(kOptimized, NULL, &same, inL, inR, bL, bR, n, 512);
		double dL = maxDifference(aL, bL, n, &peak), dR = maxDifference(aR, bR, n, &peak);
		printf("1. BMReverb vs BMOptimizedReverb, same delay lengths: max difference L %.3g  R %.3g (peak %.3g)  %s\n",
			   dL, dR, peak, dL < 1e-4 * peak && dR < 1e-4 * peak ? "ok" : "DIFFERENT");
		if(!(dL < 1e-4 * peak && dR < 1e-4 * peak)) failures++;
		BMOptimizedReverb_free(&same);
		BMReverbFree(&bm); setUpBMReverb(&bm);
	}

	// 2. blocks against sample by sample
	{
		size_t buffers[] = { 1, 7, 64, 512, 4096, 100000 };
		for(size_t i = 0; i < sizeof buffers / sizeof *buffers; i++){
			for(int inPlace = 0; inPlace < 2; inPlace++){
				BMOptimizedReverb ref, blk; setUpOptimized(&ref); setUpOptimized(&blk);
				run(kReference, NULL, &ref, inL, inR, aL, aR, n, 512);
				if(inPlace){
					memcpy(bL, inL, n * sizeof(float)); memcpy(bR, inR, n * sizeof(float));
					run(kOptimized, NULL, &blk, bL, bR, bL, bR, n, buffers[i]);
				} else run(kOptimized, NULL, &blk, inL, inR, bL, bR, n, buffers[i]);
				double dL = maxDifference(aL, bL, n, &peak), dR = maxDifference(aR, bR, n, &peak);
				int ok = dL <= 1e-6 * peak && dR <= 1e-6 * peak;
				printf("2. blocks vs sample by sample, buffers of %6zu%s: max difference L %.3g  R %.3g  %s\n",
					   buffers[i], inPlace ? ", in place" : "          ", dL, dR, ok ? "ok" : "DIFFERENT");
				if(!ok) failures++;
				BMOptimizedReverb_free(&ref); BMOptimizedReverb_free(&blk);
			}
		}
		// a change of the delay times while running, to short delays (the blocks get short with them)
		BMOptimizedReverb ref, blk; setUpOptimized(&ref); setUpOptimized(&blk);
		run(kReference, NULL, &ref, inL, inR, aL, aR, n / 2, 512);
		run(kOptimized, NULL, &blk, inL, inR, bL, bR, n / 2, 64);
		BMOptimizedReverb_setDelayTimes(&ref, 0.001f, 0.020f); BMOptimizedReverb_setDelayTimes(&blk, 0.001f, 0.020f);
		run(kReference, NULL, &ref, inL, inR, aL, aR, n, 512);
		run(kOptimized, NULL, &blk, inL, inR, bL, bR, n, 64);
		double dL = maxDifference(aL, bL, n, &peak), dR = maxDifference(aR, bR, n, &peak);
		int ok = dL <= 1e-6 * peak && dR <= 1e-6 * peak && peak > 0.01;
		printf("2. after a change to delays of 1 to 20 ms (blocks of %zu):  max difference L %.3g  R %.3g  %s\n",
			   blk.minLineLength, dL, dR, ok ? "ok" : "DIFFERENT");
		if(!ok) failures++;
		// the tail's level against the RT60: 60 dB in RT60 seconds
		BMOptimizedReverb_free(&ref); BMOptimizedReverb_free(&blk);
	}

	// General routing: one unit, odd counts, and even counts where all four
	// outputs of a group feed the same channel. Check against the scalar FDN.
	{
		size_t units[] = {1, 2, 3, 4, 8, 16};
		for(size_t u = 0; u < sizeof units / sizeof *units; u++){
			BMOptimizedReverb ref, blk;
			if(!BMOptimizedReverb_initWithNumDelayUnits(&ref, FS, 0.5f, units[u]) ||
			   !BMOptimizedReverb_initWithNumDelayUnits(&blk, FS, 0.5f, units[u])) abort();
			run(kReference, NULL, &ref, inL, inR, aL, aR, n, 512);
			run(kOptimized, NULL, &blk, inL, inR, bL, bR, n, 64);
			double dL = maxDifference(aL, bL, n, &peak), dR = maxDifference(aR, bR, n, &peak);
			int ok = dL <= 1e-6 * peak && dR <= 1e-6 * peak;
			printf("2. %2zu units vs scalar: L %.3g R %.3g %s\n", units[u], dL, dR, ok ? "ok" : "DIFFERENT");
			if(!ok) failures++;
			BMOptimizedReverb_free(&ref);
			BMOptimizedReverb_free(&blk);
		}
	}

	// 3. time
	{
		size_t buffers[] = { 64, 512 };
		const char *names[] = { "BMReverb (as the synth ran it)", "sample by sample, stripped", "BMOptimizedReverb" };
		for(size_t i = 0; i < sizeof buffers / sizeof *buffers; i++){
			double base = 0;
			for(int w = 0; w < 3; w++){
				double best = 1e9;
				for(int rep = 0; rep < 7; rep++){
					double t0 = now();
					run((Which)w, &bm, &opt, inL, inR, aL, aR, n, buffers[i]);
					double t = now() - t0;
					if(t < best) best = t;
				}
				if(w == 0) base = best;
				printf("3. buffers of %4zu  %-32s %7.1f ns per sample  %6.0f x real time  %5.1f x BMReverb\n",
					   buffers[i], names[w], best / (double)n * 1e9, (double)n / FS / best, base / best);
			}
		}
	}

	printf(failures ? "\n%d FAILED\n" : "\nall ok\n", failures);
	return failures != 0;
}
