//
// Created by hans anderson on 7/4/23.
//

#ifndef THERAPYFILTER_BM2X2MATRIX_H
#define THERAPYFILTER_BM2X2MATRIX_H

#include <stdbool.h>
#include <math.h>

typedef struct matrix2x2 {
    float a1, a2;
    float b1, b2;
} BM2x2Matrix;


typedef struct vector2 {
    float x, y;
} BMVector2;




/*!
 *BM2x2Matrix_rotationMatrix
 *
 * @abstract returns a 2x2 rotation matrix with angle theta
 */
BM2x2Matrix BM2x2Matrix_rotationMatrix(float theta);




/*!
*BM2x2Matrix_rotate
*
* @abstract out = M.RotationMatrix(theta)
*/
BM2x2Matrix BM2x2Matrix_rotate(BM2x2Matrix M, float theta);



/*!
 *
 * @param A
 * @param B
 * @return true if A == B
 */
bool BM2x2Matrix_equal(BM2x2Matrix A, BM2x2Matrix B);

/*!
 *
 * @param A 2x2 matrix
 * @param x column vector
 * @return A.x
 */
BMVector2 BM2x2MVMul(BM2x2Matrix A, BMVector2 x);


#endif //THERAPYFILTER_BM2X2MATRIX_H
