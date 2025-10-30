//
//  BMVectorOps.c
//  GainStageVintageCleanAU3
//
//  Created by hans anderson on 3/14/20.
//  This file is public domain with no restrictions.
//

#include "BMVectorOps.h"
#include "../AudioFilter.h"


/*!
 *BMVectorNorm
 *
 * @abstract find the l2 norm of v
 *
 * @param v input vector
 * @param length length of v
 */
float BMVectorNorm(const float* v, size_t length){
	float sumsq;
	vDSP_svesq(v, 1, &sumsq, length);
	return sqrtf(sumsq);
}


/*!
 *BMVectorNormalise
 *
 * @abstract scale v so that its l2 norm is equal to one
 */
void BMVectorNormalise(float* v, size_t length){
	float norm = BMVectorNorm(v, length);
	if(norm > 0){
		float scale = 1.0 / norm;
		vDSP_vsmul(v,1,&scale,v,1,length);
	}
}

