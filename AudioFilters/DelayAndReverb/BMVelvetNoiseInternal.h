// Private implementation shared by velvet noise and its decorrelator.
// Applications should include BMVelvetNoise.h or BMVelvetNoiseDecorrelator.h.
#ifndef BMVelvetNoiseInternal_h
#define BMVelvetNoiseInternal_h

#include "../MathUtilities/BMRandom.h"

#ifdef __cplusplus
extern "C" {
#endif

void BMVelvetNoise_initRandomizer(BMRandom *rng);
void BMVelvetNoise_setTapIndicesInternal(BMRandom *rng,
                                        float startTimeMS, float endTimeMS,
                                        size_t *indicesOut, float sampleRate,
                                        size_t numTaps);
void BMVelvetNoise_setTapSignsInternal(BMRandom *rng, float *tapSigns, size_t numTaps);

#ifdef __cplusplus
}
#endif
#endif
