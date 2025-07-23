//
//  BMUnitConversion.h
//  OscilloScope
//
//  Created by hans anderson on 9/8/20.
//  This file is public domain.
//

#ifndef BMUnitConversion_h
#define BMUnitConversion_h

#include <stdio.h>


float BMConv_dBToGain(float db);

void BMConv_dBToGainV(const float *input, float *output, size_t numSamples);

double BMConv_dBToGain_d(double db);

float BMConv_gainToDb(float gain);

double BMConv_gainToDb_d(double gain);

float BMConv_hzToBark(float hz);

float BMConv_barkToHz(float bark);

float BMConv_hzToFFTBin(float hz, float fftSize, float sampleRate);

// how many fft bins are represented by a single pixel at freqHz?
float BMConv_fftBinsPerPixel(float freqHz,
							 float fftSize,
							 float pixelHeight,
							 float minFrequency,
							 float maxFrequency,
							 float sampleRate);


/*!
 *BMLerp
 * Linear interpolation between min and max
 *
 * @param min minimum boundary
 * @param max maximum boundary
 * @param fraction interpolation fraction between min and max
 */
float BMLerp(float min, float max, float fraction);


/*!
 *BMLogInterp
 *
 * Interpolate between min and max in log scale
 *
 * @param min minimum boundary
 * @param max maximum boundary
 * @param fraction if this is 0 then output is min; if 1 then max; if 0.5 then output is half way between min and max on a logarithmic scale.
 */
float BMLogInterp(float min, float max, float fraction);

#endif /* BMUnitConversion_h */
