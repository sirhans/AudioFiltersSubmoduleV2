//
//  BMADSR.c
//  AudioFilters
//
//  Created by hans anderson on 10/9/26.
//  Anyone may use this file without restrictions.
//

#include "BMADSR.h"
#include <math.h>

/* The step response of six coincident real poles at wc has the residual
 *     r(x) = e^-x (1 + x + x^2/2 + x^3/6 + x^4/24 + x^5/120),   x = wc t.
 * A time T for a stage means wc = X / T with X the value of x where r
 * reaches the level that defines the stage's end. */
#define BMADSR_ATTACK_X   9.2747    /* r = 0.1: attack time is the time to 90 % */
#define BMADSR_60DB_X    16.4547    /* r = 0.001: decay and release times are 60 dB */
/* Decay begins once the attack residual is below this (-60 dB, at about
 * 1.8 attack times). Dropping the slope there costs a corner 70 dB or more
 * below the step for any attack time the UI allows. */
#define BMADSR_HANDOVER   1.0e-3
/* The release is over, and the voice may stop, at -100 dB. The step from
 * here to zero is flat in spectrum, so it must be far below the release's
 * own skirt; -60 dB was audible with a boost in the presence region. */
#define BMADSR_OFF        1.0e-5
/* Decay is reported as sustain (for callers reading the stage) this close
 * to the sustain level. The filter runs on regardless. */
#define BMADSR_SUSTAIN_DONE 1.0e-4

/* Integrator gain g = tan(pi fc / fs) for the cutoff fc = wc / 2pi that
 * gives residual level `x` at `seconds`. Zero or negative time means as fast
 * as the filter goes: g = 1, a cutoff of a quarter of the sample rate. */
static double gainForTime(float seconds, float sampleRate, double x)
{
    if (seconds <= 0.0f) return 1.0;
    double arg = x / (2.0 * (double)seconds * (double)sampleRate);
    if (arg >= M_PI / 4.0) return 1.0;
    return tan(arg);
}

static void setCoefficients(BMADSR *This, double g)
{
    This->g = g;
    This->a1 = 1.0 / (1.0 + g * (g + 2.0));   /* k = 1/Q = 2: critically damped */
    This->a2 = This->a1 * g;
    This->a3 = This->a2 * g;
}

/* Change the cutoff of the running chain without a jump in the output or
 * its slope. Each section's ic2 is its level and its ic1 its velocity (the
 * per-sample change of ic2 is about 2 g ic1). The last section's level is
 * the output and is never touched. */
static void switchCutoff(BMADSR *This, double gNew)
{
    const double gOld = This->g;
    const int last = BMADSR_SECTIONS - 1;
    if (gOld > 0.0 && gNew > gOld) {
        /* Faster: velocities and the gaps between sections scale with the
         * cutoff ratio, which is what the new filter has in the same
         * motion. Preserves value and slope, and leaves the least jerk. */
        const double r = gOld / gNew;
        for (int s = 0; s < BMADSR_SECTIONS; s++) This->ic1[s] *= r;
        for (int s = last - 1; s >= 0; s--)
            This->ic2[s] = This->ic2[s + 1] + (This->ic2[s] - This->ic2[s + 1]) * r;
    } else if (gOld > 0.0 && gNew < gOld) {
        /* Slower: a slow filter given a fast filter's velocity coasts far
         * past its target, so start it at rest at the output value. */
        for (int s = 0; s < BMADSR_SECTIONS; s++) {
            This->ic1[s] = 0.0;
            This->ic2[s] = This->ic2[last];
        }
    }
    setCoefficients(This, gNew);
}

static void reset(BMADSR *This)
{
    for (int s = 0; s < BMADSR_SECTIONS; s++) This->ic1[s] = This->ic2[s] = 0.0;
    This->value = 0.0f;
    This->target = 0.0;
    This->stage = BMADSR_IDLE;
}

void BMADSR_init(BMADSR *This, float sampleRate)
{
    This->sampleRate = sampleRate;
    This->g = 0.0;
    reset(This);
    BMADSR_setAttack(This, 0.006f);
    BMADSR_setDecay(This, 0.150f);
    BMADSR_setSustain(This, 0.8f);
    BMADSR_setRelease(This, 0.120f);
    setCoefficients(This, This->gRelease);
}

void BMADSR_setAttack(BMADSR *This, float seconds)
{
    This->gAttack = gainForTime(seconds, This->sampleRate, BMADSR_ATTACK_X);
    if (This->stage == BMADSR_ATTACK) switchCutoff(This, This->gAttack);
}

void BMADSR_setDecay(BMADSR *This, float seconds)
{
    This->gDecay = gainForTime(seconds, This->sampleRate, BMADSR_60DB_X);
    if (This->stage == BMADSR_DECAY || This->stage == BMADSR_SUSTAIN) switchCutoff(This, This->gDecay);
}

void BMADSR_setSustain(BMADSR *This, float level)
{
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    This->sustain = level;
    /* The target is the filter's input, so it may step; the output cannot. */
    if (This->stage == BMADSR_DECAY || This->stage == BMADSR_SUSTAIN) {
        This->target = level;
        This->stage = BMADSR_DECAY;
    }
}

void BMADSR_setRelease(BMADSR *This, float seconds)
{
    This->gRelease = gainForTime(seconds, This->sampleRate, BMADSR_60DB_X);
    if (This->stage == BMADSR_RELEASE) switchCutoff(This, This->gRelease);
}

void BMADSR_gateOn(BMADSR *This)
{
    This->stage = BMADSR_ATTACK;
    This->target = 1.0;
    switchCutoff(This, This->gAttack);
}

void BMADSR_gateOff(BMADSR *This)
{
    if (This->stage == BMADSR_IDLE) return;
    This->stage = BMADSR_RELEASE;
    This->target = 0.0;
    switchCutoff(This, This->gRelease);
}

bool BMADSR_isIdle(const BMADSR *This)
{
    return This->stage == BMADSR_IDLE;
}

void BMADSR_processBuffer(BMADSR *This, float *output, size_t numSamples)
{
    size_t i = 0;
    if (This->stage == BMADSR_IDLE) {
        for (; i < numSamples; i++) output[i] = 0.0f;
        return;
    }

    for (; i < numSamples; i++) {
        /* Three trapezoidal SVF lowpass sections in series, the target in. */
        double x = This->target;
        for (int s = 0; s < BMADSR_SECTIONS; s++) {
            const double v3 = x - This->ic2[s];
            const double v1 = This->a1 * This->ic1[s] + This->a2 * v3;
            const double v2 = This->ic2[s] + This->a2 * This->ic1[s] + This->a3 * v3;
            This->ic1[s] = 2.0 * v1 - This->ic1[s];
            This->ic2[s] = 2.0 * v2 - This->ic2[s];
            x = v2;
        }
        output[i] = This->value = (float)x;

        switch (This->stage) {
            case BMADSR_ATTACK:
                if (x >= 1.0 - BMADSR_HANDOVER) {
                    This->stage = BMADSR_DECAY;
                    This->target = This->sustain;
                    switchCutoff(This, This->gDecay);
                }
                break;
            case BMADSR_DECAY:
                if (fabs(x - This->sustain) < BMADSR_SUSTAIN_DONE) This->stage = BMADSR_SUSTAIN;
                break;
            case BMADSR_RELEASE:
                if (x < BMADSR_OFF) {
                    reset(This);
                    output[i] = 0.0f;
                    for (i++; i < numSamples; i++) output[i] = 0.0f;
                    return;
                }
                break;
            default:
                break;
        }
    }
}
