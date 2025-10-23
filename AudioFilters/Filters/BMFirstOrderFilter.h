//
// Created by hans anderson on 7/4/23.
//

#ifndef THERAPYFILTER_BMFIRSTORDERFILTER_H
#define THERAPYFILTER_BMFIRSTORDERFILTER_H

#include "../AudioFilter.h"

typedef struct BMFirstOrderFilterMono {
    float z1_f, b0_f, b1_f, a1_f, az_f;
    float sampleRate;
} BMFirstOrderFilterMono;

typedef struct BMFirstOrderFilterStereo {
    BMFirstOrderFilterMono filterL, filterR;
} BMFirstOrderFilterStereo;


/*!
 *
 * @param This pointer to an initialised struct
 * @param input array of input, length numSamples
 * @param output array of output, length numSamples
 * @param numSamples length of input and output
 */
void BMFirstOrderFilterMono_process(BMFirstOrderFilterMono* This,
                                    const float *input,
                                    float *output,
                                    size_t numSamples);

/*!
 *
 * @param This pointer to an initialised struct
 * @param inputL array of left channel input, length numSamples
 * @param inputR array of right channel input, length numSamples
 * @param outputL array of left channel output, length numSamples
 * @param outputR array of right channel output, length numSamples
 * @param numSamples length of input and output arrays
 */
void BMFirstOrderFilterStereo_process(BMFirstOrderFilterStereo *This,
                                     const float *inputL, const float *inputR,
                                     float *outputL, float *outputR,
                                     size_t numSamples);

/*!
 *
 * @param This
 * @param sampleRate
 */
void BMFirstOrderFilterStereo_init(BMFirstOrderFilterStereo *This, float sampleRate);

/*!
 *
 * @param This
 * @param sampleRate
 */
void BMFirstOrderFilterMono_init(BMFirstOrderFilterMono *This, float sampleRate);

/*!
 *
 * @param This
 * @param fc
 */
void BMFirstOrderFilterStereo_setLowpass(BMFirstOrderFilterStereo *This, float fc);

/*!
 *
 * @param This
 * @param fc
 */
void BMFirstOrderFilterMono_setLowpass(BMFirstOrderFilterMono *This, float fc);

/*!
 *
 * @param This
 * @param fc
 */
void BMFirstOrderFilterStereo_setHighpass(BMFirstOrderFilterStereo *This, float fc);

/*!
 *
 * @param This
 * @param fc
 */
void BMFirstOrderFilterMono_setHighpass(BMFirstOrderFilterMono *This, float fc);

/*!
 *
 * @param This
 */
void BMFirstOrderFilterStereo_clearBuffers(BMFirstOrderFilterStereo *This);

/*!
 *
 * @param This
 */
void BMFirstOrderFilterMono_clearBuffers(BMFirstOrderFilterMono *This);

#endif //THERAPYFILTER_BMFIRSTORDERFILTER_H
