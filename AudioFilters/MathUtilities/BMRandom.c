//
//  BMRandom.c
//  AudioFilters
//
//  See BMRandom.h. Public domain.
//

#include "BMRandom.h"
#include <time.h>

// TinyMT is a family of generators selected by three parameters, which must
// be set before tinymt32_init: it copies them into the state and its
// recurrence and tempering use them. These are the reference set from the
// TinyMT distribution (check32.c). Left at zero (as they were until
// 2026-09-18, in any zero-initialised struct) the recurrence loses its
// mixing and stretches of the stream repeat a pattern: the speaker cabinet's
// left and right tap times, drawn from consecutive runs, came out within 3
// samples of each other in 10 cells of 12.
#define BM_RANDOM_TINYMT_MAT1 UINT32_C(0x8f7011ee)
#define BM_RANDOM_TINYMT_MAT2 UINT32_C(0xfc78ff1f)
#define BM_RANDOM_TINYMT_TMAT UINT32_C(0x3793fdff)

void BMRandom_init(BMRandom *This, uint32_t seed){
    This->seed = seed;
    This->generator.mat1 = BM_RANDOM_TINYMT_MAT1;
    This->generator.mat2 = BM_RANDOM_TINYMT_MAT2;
    This->generator.tmat = BM_RANDOM_TINYMT_TMAT;
    tinymt32_init(&This->generator, seed);
}

void BMRandom_initFromClock(BMRandom *This){
    BMRandom_init(This, (uint32_t)time(NULL) ^ (uint32_t)clock());
}

uint32_t BMRandom_next(BMRandom *This){
    return tinymt32_generate_uint32(&This->generator);
}

uint32_t BMRandom_uniform(BMRandom *This, uint32_t upperBound){
    if(upperBound < 2) return 0;
    // reject draws from the incomplete last interval so every value in
    // [0, upperBound) is equally likely (same method as arc4random_uniform)
    uint32_t threshold = (uint32_t)(-upperBound) % upperBound;   // 2^32 mod upperBound
    uint32_t r;
    do { r = tinymt32_generate_uint32(&This->generator); } while(r < threshold);
    return r % upperBound;
}

size_t BMRandom_inRange(BMRandom *This, size_t min, size_t max){
    if(max <= min) return min;
    return min + (size_t)BMRandom_uniform(This, (uint32_t)(max - min + 1));
}

float BMRandom_float01(BMRandom *This){
    return tinymt32_generate_float(&This->generator);   // 24-bit mantissa, [0, 1)
}

void BMRandom_shuffleFloats(BMRandom *This, float *A, size_t length){
    for(size_t i = length; i > 1; i--){
        size_t j = BMRandom_uniform(This, (uint32_t)i);
        float t = A[i - 1]; A[i - 1] = A[j]; A[j] = t;
    }
}
