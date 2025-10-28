//
//  BMQuadratureOscillator.c
//  BMAudioFilters
//
//  A sinusoidal quadrature oscillator suitable for efficient LFO. However, it
//  is not restricted to low frequencies and may also be used for other
//  applications.
//
//  Created by Hans on 31/10/16.
//
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
//

#include <math.h>
#include "BM2x2Matrix.h"
#include "BMQuadratureOscillator.h"
#include <stdio.h>
#include <string.h>
#include <Accelerate/Accelerate.h>

#ifdef __cplusplus
extern "C" {
#endif
	
#define BM_QUADRATURE_OSCILLATOR_START_ANGLE -0.5 * M_PI_2
	
	
	void BMQuadratureOscillator_initMatrix(float64x2x2_t* m,
										   double frequency,
										   double sampleRate){
		// we want a rotation matrix that completes a rotation of 2*M_PI in
		// (sampleRate / fHz) samples. That means that in 1 sample, we need to
		// rotate by an angle of fHz * 2 * M_PI / sampleRate.
		double oneSampleAngle = frequency * 2.0 * M_PI / sampleRate;
		
		// limit the rotation to within [0,2*pi]
		oneSampleAngle = fmod(oneSampleAngle, 2.0 * M_PI);
		
		*m = BM2x2MatrixD_rotationMatrix(oneSampleAngle);
	}
	
	
	
	
	void BMQuadratureOscillator_init(BMQuadratureOscillator *This,
									 float fHz,
									 float sampleRate){
		This->sampleRate = sampleRate;
		This->oscFreq = fHz;
		
		BMQuadratureOscillator_initMatrix(&This->m, fHz, sampleRate);
		This->mPending = This->m;
		
		BMQuadratureOscillator_setAngle(This,BM_QUADRATURE_OSCILLATOR_START_ANGLE);
	}
	
	
	
	
	void BMQuadratureOscillator_setAngle(BMQuadratureOscillator *This,
										 double angleRadians){
//		This->rq.x = cos(angleRadians);
//		This->rq.y = sin(angleRadians);
		This->rq = (float64x2_t){ cos(angleRadians), sin(angleRadians) };
	}

	
	float BMQuadratureOscillator_getAngle(BMQuadratureOscillator *This){
		// return atan2(This->rq.y, This->rq.x);
		return atan2(vgetq_lane_f64(This->rq,0), vgetq_lane_f64(This->rq,1));
	}
	
	
	void BMQuadratureOscillator_setTimeInSamples(BMQuadratureOscillator *This, size_t sampleTime){
		// the math here will not be exact due to limits of floating point precision
		double periodInSamples = This->sampleRate / This->oscFreq;
		double timeInRadians = (2.0 * M_PI) * fmod((double)sampleTime, periodInSamples) / periodInSamples;
		double angle = timeInRadians + BM_QUADRATURE_OSCILLATOR_START_ANGLE;
		BMQuadratureOscillator_setAngle(This, angle);
	}
	
	
	
	void BMQuadratureOscillator_setFrequency(BMQuadratureOscillator *This, float fHz){
		This->oscFreq = fHz;
		BMQuadratureOscillator_initMatrix(&This->mPending, fHz, This->sampleRate);
	}
	

TODO: test this to make sure the result is the same as simd_mul(M,v)
	static inline float64x2_t neon_f64_Mvmul(float64x2x2_t M, float64x2_t v) {
		float64x2_t r = vmulq_n_f64(M.val[0], vgetq_lane_f64(v, 0));
		r = vfmaq_n_f64(r, M.val[1], vgetq_lane_f64(v, 1));
		return r;
	}
	
TODO: test this to make sure the result is the same as simd_equal(A,B)
	static inline bool neon_f64_2x2_equal(float64x2x2_t A, float64x2x2_t B) {
		// Compare each column element-wise
		uint64x2_t cmp0 = vceqq_f64(A.val[0], B.val[0]);
		uint64x2_t cmp1 = vceqq_f64(A.val[1], B.val[1]);

		// Combine comparison results for all lanes
		uint64_t all0 = vgetq_lane_u64(cmp0, 0) & vgetq_lane_u64(cmp0, 1);
		uint64_t all1 = vgetq_lane_u64(cmp1, 0) & vgetq_lane_u64(cmp1, 1);

		// Both columns must be fully equal (all bits set)
		return (all0 & all1) == 0xFFFFFFFFFFFFFFFFULL;
	}
	
	
	/*
	 * // Mathematica Prototype code
	 *
	 * For[i = 1, i <= n, i++,
	 *   out[[i]] = r[[1]];
	 *
	 *   r = m.r
	 * ];
	 *
	 */
	void BMQuadratureOscillator_process(BMQuadratureOscillator *This,
											float* r,
											float* q,
											size_t numSamples){
		// update the matrix if necessary
		if(!neon_f64_2x2_equal(This->m, This->mPending)){
			This->m = This->mPending;
		}
		
		for(size_t i=0; i<numSamples; i++){
			// copy the current sample value to output
//			r[i] = (float)This->rq.y;
//			q[i] = (float)This->rq.x;
			r[i] = (float)vgetq_lane_f64(This->rq,0);
			q[i] = (float)vgetq_lane_f64(This->rq,1);
						
			
			// multiply the vector rq by the rotation matrix m to generate the
			// next sample of output
			// This->rq = simd_mul(This->m, This->rq);
			This->rq = neon_f64_Mvmul(This->m, This->rq);
		}
	}
	
	
	
	
	void BMQuadratureOscillator_advance(BMQuadratureOscillator *This,
										float *r,
										float *q,
										size_t numSamples){
		// update the matrix if necessary
		if(!neon_f64_2x2_equal(This->m, This->mPending)){
			This->m = This->mPending;
		}
		
		// copy the current sample value to output
		*r = (float)vgetq_lane_f64(This->rq,0);
		*q = (float)vgetq_lane_f64(This->rq,1);
		
		// compute the rotation matrix for skipping ahead numSamples
		float64x2x2_t m;
		double skipFreq = This->oscFreq * (double)numSamples;
		BMQuadratureOscillator_initMatrix(&m, skipFreq, This->sampleRate);
		
		// multiply the vector rq by the rotation matrix m to skip ahead numSamples
		This->rq = neon_f64_Mvmul(m, This->rq);
	}

	
	
	
	void BMQuadratureOscillator_volumeEnvelope4Stereo(BMQuadratureOscillator *This,
												float** buffersL, float** buffersR,
												bool* zeros,
												size_t numSamples){
		
		for(size_t i=0; i<numSamples; i++){
			// multiply channels 0 and 1 by the positive side of the quadrature
			// oscillator
//			float g = (float)simd_max(This->rq.x, 0.0);
			float g = (float)MAX(vgetq_lane_f64(This->rq, 0), 0.0);
			buffersL[0][i] *= g;
			buffersR[0][i] *= g;

//			g = (float)simd_min(This->rq.x, 0.0);
			g = (float)MIN(vgetq_lane_f64(This->rq, 0), 0.0);
			buffersL[1][i] *= g;
			buffersR[1][i] *= g;
			
			// multiple channels 2 and 3 by the negative side of the oscillator
//			g = (float)simd_max(This->rq.y, 0.0);
			g = (float)MAX(vgetq_lane_f64(This->rq, 1), 0.0);
			buffersL[2][i] *= g;
			buffersR[2][i] *= g;
			
//			g = (float)simd_min(This->rq.y, 0.0);
			g = (float)MIN(vgetq_lane_f64(This->rq, 1), 0.0);
			buffersL[3][i] *= g;
			buffersR[3][i] *= g;
			
			// multiply the vector rq by the rotation matrix m to generate the
			// next sample of the envelope
			This->rq = neon_f64_Mvmul(This->m, This->rq);
		}
		
		// output an array of 4 bools to let the calling function know which
		// of the channels are at zero gain. This allows the calling function
		// to make changes to the filters on those channels without causing clicks.
		//
		// the following 4 statements will be true if the gain is non-zero
//		zeros[0] = This->rq.x > 0.0;
		zeros[0] = vgetq_lane_f64(This->rq, 0) > 0.0;
//		zeros[1] = This->rq.x < 0.0;
		zeros[1] = vgetq_lane_f64(This->rq, 0) < 0.0;
//		zeros[2] = This->rq.y > 0.0;
		zeros[2] = vgetq_lane_f64(This->rq, 1) > 0.0;
//		zeros[3] = This->rq.y < 0.0;
		zeros[3] = vgetq_lane_f64(This->rq, 1) < 0.0;
	}
	
	

#ifdef __cplusplus
}
#endif
