//
// Created by hans anderson on 7/4/23.
//

#include "BM2x2Matrix.h"
#include <math.h>



BM2x2Matrix BM2x2Matrix_mul(BM2x2Matrix A, BM2x2Matrix B){
    // [(B.a1 * A.a1 +  B.b1 * A.a2) (B.a2 * A.a1 +  B.b2 * A.a2)]     [A.a1 A.a2]      [B.a1 B.a2]
    // [(B.a1 * A.b1 +  B.b1 * A.b2) (B.a2 * A.b1 +  B.b2 * A.b2)]  =  [A.b1 A.b2]   x  [B.b1 B.b2]
    BM2x2Matrix C = {(B.a1 * A.a1 +  B.b1 * A.a2),(B.a2 * A.a1 +  B.b2 * A.a2),
                     (B.a1 * A.b1 +  B.b1 * A.b2),(B.a2 * A.b1 +  B.b2 * A.b2)};
    return C;
}

BM2x2Matrix BM2x2Matrix_rotationMatrix(float theta){
//    simd_float2 column0 = {cosf(theta), sinf(theta)};
//    simd_float2 column1 = {-sinf(theta),cosf(theta)};
//    simd_float2x2 R = simd_matrix(column0, column1);

    BM2x2Matrix R = {cosf(theta), -sinf(theta),
                     sinf(theta), cosf(theta)};
    return R;
}


BM2x2Matrix BM2x2Matrix_rotate(BM2x2Matrix M, float theta){
    return BM2x2Matrix_mul(M,BM2x2Matrix_rotationMatrix(theta));
}


bool BM2x2Matrix_equal(BM2x2Matrix A, BM2x2Matrix B){
    if( A.a1 == B.a1 &&
        A.a2 == B.a2 &&
        A.b1 == B.b1 &&
        A.b2 == B.b2 )
    {
        return true;
    }

    return false;
}



/*!
 *
 * @param A 2x2 matrix
 * @param x column vector
 * @return A.x
 */
BMVector2 BM2x2MVMul(BM2x2Matrix A, BMVector2 X){
    BMVector2 y = {A.a1 * X.x + A.a2 * X.y,
                   A.b1 * X.x + A.b2 * X.y};
    return y;
}