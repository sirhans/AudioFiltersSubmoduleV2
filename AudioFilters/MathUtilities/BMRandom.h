//
//  BMRandom.h
//  AudioFilters
//
//  A seedable replacement for arc4random(), wrapping TinyMT32 (Saito and
//  Matsumoto's small Mersenne Twister, tinymt32.h, BSD licence in
//  TinyMT-LICENSE.txt). Each BMRandom is an independent stream, so a
//  component that owns one produces the same random choices every time it is
//  initialised with the same seed, regardless of what else in the process has
//  drawn random numbers. That is what makes the speaker cabinet's velvet
//  noise identical from one engine instance to the next; arc4random cannot be
//  seeded and rand() is a single process-wide stream.
//
//  The function names follow arc4random: BMRandom_next() is arc4random(),
//  BMRandom_uniform(n) is arc4random_uniform(n) (unbiased, in [0, n)).
//
//  Created for Midi Tuning Synth, 12 Sep 2026. Public domain.
//

#ifndef BMRandom_h
#define BMRandom_h

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "TinyMT/tinymt32.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BMRandom {
    tinymt32_t generator;
    uint32_t seed;
} BMRandom;

/*!
 *BMRandom_init
 *
 * @abstract Start the stream from `seed`. The same seed always gives the same
 * sequence. Any 32-bit value is a valid seed, including 0.
 */
void BMRandom_init(BMRandom *This, uint32_t seed);

/*!
 *BMRandom_initFromClock
 *
 * @abstract Seed from the clock, for callers that want arc4random's
 * behaviour (different every time). The seed used is stored in This->seed.
 */
void BMRandom_initFromClock(BMRandom *This);

/*!
 *BMRandom_next
 *
 * @returns a uniformly distributed uint32_t, like arc4random()
 */
uint32_t BMRandom_next(BMRandom *This);

/*!
 *BMRandom_uniform
 *
 * @returns a uniformly distributed integer in [0, upperBound), like
 * arc4random_uniform(). Unbiased (rejection sampling). upperBound 0 returns 0.
 */
uint32_t BMRandom_uniform(BMRandom *This, uint32_t upperBound);

/*!
 *BMRandom_inRange
 *
 * @returns a uniformly distributed integer in [min, max] (inclusive)
 */
size_t BMRandom_inRange(BMRandom *This, size_t min, size_t max);

/*!
 *BMRandom_float01
 *
 * @returns a uniformly distributed float in [0, 1)
 */
float BMRandom_float01(BMRandom *This);

/*!
 *BMRandom_shuffleFloats
 *
 * @abstract Fisher-Yates shuffle of a float array
 */
void BMRandom_shuffleFloats(BMRandom *This, float *A, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* BMRandom_h */
