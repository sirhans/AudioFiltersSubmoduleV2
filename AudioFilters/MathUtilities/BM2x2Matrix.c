//
//  BM2x2Matrix.c
//  UnyteAudioFilterTest
//
//  Created by Nguyen Minh Tien on 29/10/2025.
//

#include <stdio.h>
#include "BM2x2Matrix.h"

inline simd_float2x2 BM2x2Matrix_rotationMatrix(float theta){
    simd_float2 column0 = {cosf(theta), sinf(theta)};
    simd_float2 column1 = {-sinf(theta),cosf(theta)};
    simd_float2x2 R = simd_matrix(column0, column1);
    return R;
}




/*!
 *BM2x2MatrixD_rotationMatrix
 *
 * @abstract returns a 2x2 rotation matrix with angle theta
 */
inline simd_double2x2 BM2x2MatrixD_rotationMatrix(double theta){
    simd_double2 column0 = {cos(theta), sin(theta)};
    simd_double2 column1 = {-sin(theta),cos(theta)};
    simd_double2x2 R = simd_matrix(column0, column1);
    return R;
}




/*!
*BM2x2Matrix_rotate
*
* @abstract out = M.RotationMatrix(theta)
*/
inline simd_float2x2 BM2x2Matrix_rotate(simd_float2x2 M, float theta){
    return simd_mul(M,BM2x2Matrix_rotationMatrix(theta));
}
