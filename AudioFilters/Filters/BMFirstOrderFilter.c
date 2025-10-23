//
// Created by hans anderson on 7/4/23.
//

#include "BMFirstOrderFilter.h"

void BMFirstOrderFilterMono_process(BMFirstOrderFilterMono* This,
                                    const float *input,
                                    float *output,
                                    size_t numSamples){

    for(size_t i=0; i<numSamples; i++){
        output[i] = (input[i] * This->b0_f) + (This->az_f * This->b1_f) - (This->z1_f * This->a1_f);
        This->z1_f = output[i];
        This->az_f = input[i];
    }

}

void BMFirstOrderFilterStereo_process(BMFirstOrderFilterStereo *This,
                                     const float *inputL, const float *inputR,
                                     float *outputL, float *outputR,
                                     size_t numSamples){

    BMFirstOrderFilterMono_process(&This->filterL,
                                   inputL,
                                   outputL,
                                   numSamples);

    BMFirstOrderFilterMono_process(&This->filterR,
                                   inputR,
                                   outputR,
                                   numSamples);
}


void BMFirstOrderFilterStereo_init(BMFirstOrderFilterStereo *This, float sampleRate){
    BMFirstOrderFilterMono_init(&This->filterL, sampleRate);
    BMFirstOrderFilterMono_init(&This->filterR, sampleRate);
}


void BMFirstOrderFilterMono_init(BMFirstOrderFilterMono *This, float sampleRate){
    This->sampleRate = sampleRate;
    This->az_f = 0.0f;
    This->z1_f = 0.0f;
}


void BMFirstOrderFilterStereo_setLowpass(BMFirstOrderFilterStereo *This, float fc){
    BMFirstOrderFilterMono_setLowpass(&This->filterL, fc);
    BMFirstOrderFilterMono_setLowpass(&This->filterR, fc);
}


void BMFirstOrderFilterMono_setLowpass(BMFirstOrderFilterMono *This, float fc){
    double gamma = tanf(M_PI * fc / This->sampleRate);
    double one_over_denominator = 1.0 / (gamma + 1.0);

    This->b0_f = (float)(gamma * one_over_denominator);
    This->b1_f = This->b0_f;

    This->a1_f = (float)((gamma - 1.0) * one_over_denominator);
}


void BMFirstOrderFilterStereo_setHighpass(BMFirstOrderFilterStereo *This, float fc){
    BMFirstOrderFilterMono_setHighpass(&This->filterL, fc);
    BMFirstOrderFilterMono_setHighpass(&This->filterR, fc);
}




void BMFirstOrderFilterMono_setHighpass(BMFirstOrderFilterMono *This, float fc){
    double gamma = tanf(M_PI * fc / This->sampleRate);
    double one_over_denominator = 1.0 / (gamma + 1.0);

    This->b0_f = 1.0f * (float)one_over_denominator;
    This->b1_f = -1.0f * (float)one_over_denominator;

    This->a1_f = (float)((gamma - 1.0) * one_over_denominator);
}



void BMFirstOrderFilterStereo_clearBuffers(BMFirstOrderFilterStereo *This){
    BMFirstOrderFilterMono_clearBuffers(&This->filterL);
    BMFirstOrderFilterMono_clearBuffers(&This->filterR);
}


void BMFirstOrderFilterMono_clearBuffers(BMFirstOrderFilterMono *This){
    This->z1_f = 0.0f;
    This->az_f = 0.0f;
}