//
//  BMIntegerMath.h
//  BMAudioFilters
//
//  Created by Hans on 14/9/17.
//  Anyone can use this file without restrictions
//

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef BMIntegerMath_h
#define BMIntegerMath_h

size_t BMFactorial(size_t n);


int BMFactorialI(int n);


/*
 * https://stackoverflow.com/questions/600293/how-to-check-if-a-number-is-a-power-of-2
 */
bool isPowerOfTwo(size_t x);



/*
 * Integer absolute value
 *
 * https://stackoverflow.com/questions/9772348/get-absolute-value-without-using-abs-function-nor-if-statement
 */
int32_t absi(int32_t x);



/*
 * integer log base 2
 *
 * https://stackoverflow.com/questions/994593/how-to-do-an-integer-log2-in-c
 */
uint32_t log2i(uint32_t x);




// returns the smallest power of 2 >= n
uint32_t nextPowOf2(uint32_t n);





/*!
 *gcd_ui
 *
 * @returns the greatest common divisor of a and b
 */
size_t gcd_ui(size_t a, size_t b);





/*!
 *gcd_i
 *
 * @returns the greatest common divisor of a and b
 */
int gcd_i (int a, int b);





/*
 * a^b
 *
 * https://stackoverflow.com/questions/101439/the-most-efficient-way-to-implement-an-integer-based-power-function-powint-int
 */
int powi(int a, int b);




// calcul a^n%mod
// https://rosettacode.org/wiki/Miller–Rabin_primality_test#Deterministic_up_to_341.2C550.2C071.2C728.2C321
size_t powerMod(size_t a, size_t n, size_t mod);



// REQUIRED FOR MILLER-RABIN PRIMALITY TEST
// n−1 = 2^s * d with d odd by factoring powers of 2 from n−1
// https://rosettacode.org/wiki/Miller–Rabin_primality_test#Deterministic_up_to_341.2C550.2C071.2C728.2C321
bool witness(size_t n, size_t s, size_t d, size_t a);

/*
 * MILLER-RABIN PRIMALITY TEST
 *
 * if n < 1,373,653, it is enough to test a = 2 and 3;
 * if n < 9,080,191, it is enough to test a = 31 and 73;
 * if n < 4,759,123,141, it is enough to test a = 2, 7, and 61;
 * if n < 1,122,004,669,633, it is enough to test a = 2, 13, 23, and 1662803;
 * if n < 2,152,302,898,747, it is enough to test a = 2, 3, 5, 7, and 11;
 * if n < 3,474,749,660,383, it is enough to test a = 2, 3, 5, 7, 11, and 13;
 * if n < 341,550,071,728,321, it is enough to test a = 2, 3, 5, 7, 11, 13, and 17.
 */
// https://rosettacode.org/wiki/Miller–Rabin_primality_test#Deterministic_up_to_341.2C550.2C071.2C728.2C321
bool isPrimeMr(size_t n);



#endif /* BMIntegerMath_h */

