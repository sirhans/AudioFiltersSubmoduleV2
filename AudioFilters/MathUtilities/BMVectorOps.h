//
//  BMVectorOps.h
//  BMAudioFilters
//
//  Created by hans anderson on 1/19/19.
//
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
//

#ifndef BMVectorOps_h
#define BMVectorOps_h

#include "../AudioFilter.h"

/*!
 *BMVectorNorm
 *
 * @abstract find the l2 norm of v
 *
 * @param v input vector
 * @param length length of v
 */
float BMVectorNorm(const float* v, size_t length);



/*!
 *BMVectorNormalise
 *
 * @abstract scale v so that its l2 norm is equal to one
 */
void BMVectorNormalise(float* v, size_t length);


#endif /* BMVectorOps_h */
