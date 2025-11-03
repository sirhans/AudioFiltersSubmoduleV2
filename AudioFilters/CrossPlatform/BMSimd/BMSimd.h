//
//  bmsimd.h
//  simdTest
//
//  Created by hans anderson on 10/29/25.
//

/************************************
 *    DEFINITIONS FROM simd/base.h  *
 ************************************/
#include <stdint.h>
#include <stdbool.h>
#include "BMSimdBase.h"
#include "BMSimdVector.h"

/*****************************************
 *    SIMD FUNCTIONS FROM simd/common.h  *
 *****************************************/

// simd_abs
extern SIMD_CFUNC simd_float2 simd_abs(simd_float2 x);
extern SIMD_CFUNC simd_float3 simd_abs(simd_float3 x);
extern SIMD_CFUNC simd_float4 simd_abs(simd_float4 x);
extern SIMD_CFUNC simd_double2 simd_abs(simd_double2 x);

/*****************************************
 *    __tg_ FUNCTIONS FROM simd/math.h  *
 *****************************************/

//reduce_add
static inline SIMD_CFUNC float simd_reduce_add(simd_float2 x) {
  return x.x + x.y;
}

static inline SIMD_CFUNC float simd_reduce_add(simd_float3 x) {
  return x.x + x.y + x.z;
}

static inline SIMD_CFUNC float simd_reduce_add(simd_float4 x) {
  return simd_reduce_add(x.lo + x.hi);
}

static inline SIMD_CFUNC double simd_reduce_add(simd_double2 x) {
  return x.x + x.y;
}

//muladd
static inline SIMD_CFUNC float simd_muladd(float x, float y, float z) {
#pragma STDC FP_CONTRACT ON
    return x*y + z;
}
static inline SIMD_CFUNC simd_float2 simd_muladd(simd_float2 x, simd_float2 y, simd_float2 z) {
#pragma STDC FP_CONTRACT ON
    return x*y + z;
}
static inline SIMD_CFUNC simd_float3 simd_muladd(simd_float3 x, simd_float3 y, simd_float3 z) {
#pragma STDC FP_CONTRACT ON
    return x*y + z;
}
static inline SIMD_CFUNC simd_float4 simd_muladd(simd_float4 x, simd_float4 y, simd_float4 z) {
#pragma STDC FP_CONTRACT ON
    return x*y + z;
}
static inline SIMD_CFUNC simd_float8 simd_muladd(simd_float8 x, simd_float8 y, simd_float8 z) {
#pragma STDC FP_CONTRACT ON
    return x*y + z;
}
static inline SIMD_CFUNC double simd_muladd(double x, double y, double z) {
#pragma STDC FP_CONTRACT ON
    return x*y + z;
}
static inline SIMD_CFUNC simd_double2 simd_muladd(simd_double2 x, simd_double2 y, simd_double2 z) {
#pragma STDC FP_CONTRACT ON
    return x*y + z;
}

//mul
static  simd_float2 SIMD_CFUNC simd_mul( simd_float2x2 __x,  simd_float2 __y)
{
    simd_float2 __r = __x.columns[0]*__y[0];
    __r = simd_muladd( __x.columns[1], __y[1],__r);
    return __r;
}
static  simd_float3 SIMD_CFUNC simd_mul( simd_float2x3 __x,  simd_float2 __y)
{  simd_float3 __r = __x.columns[0]*__y[0]; __r = simd_muladd( __x.columns[1], __y[1],__r); return __r; }
static  simd_float4 SIMD_CFUNC simd_mul( simd_float2x4 __x,  simd_float2 __y)
{  simd_float4 __r = __x.columns[0]*__y[0]; __r = simd_muladd( __x.columns[1], __y[1],__r); return __r; }
static  simd_float2 SIMD_CFUNC simd_mul( simd_float3x2 __x,  simd_float3 __y) {  simd_float2 __r = __x.columns[0]*__y[0]; __r = simd_muladd( __x.columns[1], __y[1],__r); __r = simd_muladd( __x.columns[2], __y[2],__r); return __r; }
static  simd_float3 SIMD_CFUNC simd_mul( simd_float3x3 __x,  simd_float3 __y) {  simd_float3 __r = __x.columns[0]*__y[0]; __r = simd_muladd( __x.columns[1], __y[1],__r); __r = simd_muladd( __x.columns[2], __y[2],__r); return __r; }
static  simd_float4 SIMD_CFUNC simd_mul( simd_float3x4 __x,  simd_float3 __y) {  simd_float4 __r = __x.columns[0]*__y[0]; __r = simd_muladd( __x.columns[1], __y[1],__r); __r = simd_muladd( __x.columns[2], __y[2],__r); return __r; }
static  simd_float2 SIMD_CFUNC simd_mul( simd_float4x2 __x,  simd_float4 __y) {  simd_float2 __r = __x.columns[0]*__y[0]; __r = simd_muladd( __x.columns[1], __y[1],__r); __r = simd_muladd( __x.columns[2], __y[2],__r); __r = simd_muladd( __x.columns[3], __y[3],__r); return __r; }
static  simd_float3 SIMD_CFUNC simd_mul( simd_float4x3 __x,  simd_float4 __y) {  simd_float3 __r = __x.columns[0]*__y[0]; __r = simd_muladd( __x.columns[1], __y[1],__r); __r = simd_muladd( __x.columns[2], __y[2],__r); __r = simd_muladd( __x.columns[3], __y[3],__r); return __r; }
static  simd_float4 SIMD_CFUNC simd_mul( simd_float4x4 __x,  simd_float4 __y) {  simd_float4 __r = __x.columns[0]*__y[0]; __r = simd_muladd( __x.columns[1], __y[1],__r); __r = simd_muladd( __x.columns[2], __y[2],__r); __r = simd_muladd( __x.columns[3], __y[3],__r); return __r; }
static simd_double2 SIMD_CFUNC simd_mul(simd_double2x2 __x, simd_double2 __y) { simd_double2 __r = __x.columns[0]*__y[0]; __r = simd_muladd( __x.columns[1], __y[1],__r); return __r; }

//maxtrix
static  simd_float2x2 SIMD_CFUNC simd_matrix(simd_float2  col0, simd_float2  col1) {  simd_float2x2 __r = { .columns[0] = col0, .columns[1] = col1 }; return __r; }
static  simd_float2x3 SIMD_CFUNC simd_matrix(simd_float3  col0, simd_float3  col1) {  simd_float2x3 __r = { .columns[0] = col0, .columns[1] = col1 }; return __r; }
static  simd_float2x4 SIMD_CFUNC simd_matrix(simd_float4  col0, simd_float4  col1) {  simd_float2x4 __r = { .columns[0] = col0, .columns[1] = col1 }; return __r; }
static simd_double2x2 SIMD_CFUNC simd_matrix(simd_double2 col0, simd_double2 col1) { simd_double2x2 __r = { .columns[0] = col0, .columns[1] = col1 }; return __r; }

static  simd_float2x2 SIMD_CFUNC simd_mul( simd_float2x2 __x,  simd_float2x2 __y) {  simd_float2x2 __r; for (int i=0; i<2; ++i) __r.columns[i] = simd_mul(__x, __y.columns[i]); return __r; }
static simd_double2x2 SIMD_CFUNC simd_mul(simd_double2x2 __x, simd_double2x2 __y) { simd_double2x2 __r; for (int i=0; i<2; ++i) __r.columns[i] = simd_mul(__x, __y.columns[i]); return __r; }

//simd_all
static inline SIMD_CFUNC simd_bool simd_all(simd_int2 x) {
#if BM_HAS_NEON && (defined __arm64__ || defined __aarch64__)
    return vminv_u32(x) & 0x80000000;
#else
    union { uint64_t i; simd_int2 v; } u = { .v = x };
  return (u.i & 0x8000000080000000) == 0x8000000080000000;
#endif
}

static inline SIMD_CFUNC simd_bool simd_all(simd_int4 x) {
#if BM_HAS_NEON && (defined __arm64__ || defined __aarch64__)
    // vminvq_u32 : Unsigned Minimum across Vector.
    //If the minimum value is 1 -> all elements of x are true
    return vminvq_u32(x) & 0x80000000;
#else
    return simd_all(x.lo & x.hi);
#endif
}

static inline SIMD_CFUNC simd_bool simd_all(simd_long2 x) {
#if defined __arm64__ || defined __aarch64__
    return (x.x & x.y) & 0x8000000000000000U;
#else
    return (x.x & x.y) & 0x8000000000000000U;
#endif
}

static inline SIMD_CFUNC simd_bool simd_all(simd_uint2 x) {
    return simd_all((simd_int2)x);
}

static inline SIMD_CFUNC simd_bool simd_all(simd_uint4 x) {
    return simd_all((simd_int4)x);
}

static simd_bool SIMD_CFUNC simd_equal(simd_float2x2 __x, simd_float2x2 __y) {
    return simd_all((__x.columns[0] == __y.columns[0]) &
                    (__x.columns[1] == __y.columns[1]));
}

static simd_bool SIMD_CFUNC simd_equal(simd_float2x4 __x, simd_float2x4 __y) {
    return simd_all((__x.columns[0] == __y.columns[0]) &
                    (__x.columns[1] == __y.columns[1]));
}
static simd_bool SIMD_CFUNC simd_equal(simd_float3x2 __x, simd_float3x2 __y) {
    return simd_all((__x.columns[0] == __y.columns[0]) &
                    (__x.columns[1] == __y.columns[1]) &
                    (__x.columns[2] == __y.columns[2]));
}

static simd_bool SIMD_CFUNC simd_equal(simd_float3x4 __x, simd_float3x4 __y) {
    return simd_all((__x.columns[0] == __y.columns[0]) &
                    (__x.columns[1] == __y.columns[1]) &
                    (__x.columns[2] == __y.columns[2]));
}
static simd_bool SIMD_CFUNC simd_equal(simd_float4x2 __x, simd_float4x2 __y) {
    return simd_all((__x.columns[0] == __y.columns[0]) &
                    (__x.columns[1] == __y.columns[1]) &
                    (__x.columns[2] == __y.columns[2]) &
                    (__x.columns[3] == __y.columns[3]));
}

static simd_bool SIMD_CFUNC simd_equal(simd_float4x4 __x, simd_float4x4 __y) {
    return simd_all((__x.columns[0] == __y.columns[0]) &
                    (__x.columns[1] == __y.columns[1]) &
                    (__x.columns[2] == __y.columns[2]) &
                    (__x.columns[3] == __y.columns[3]));
}
static simd_bool SIMD_CFUNC simd_equal(simd_double2x2 __x, simd_double2x2 __y) {
    return simd_all((__x.columns[0] == __y.columns[0]) &
                    (__x.columns[1] == __y.columns[1]));
}

SIMD_CFUNC simd_float2 __tg_fmax(simd_float2 a, simd_float2 b);
SIMD_CFUNC simd_float3 __tg_fmax(simd_float3 a, simd_float3 b);
SIMD_CFUNC simd_float4 __tg_fmax(simd_float4 a, simd_float4 b);

SIMD_CFUNC simd_double2 __tg_fmax(simd_double2 a, simd_double2 b);

SIMD_CFUNC float __tg_fmax(float a, float b);

static inline SIMD_CFUNC float simd_max(float x, float y) {
    return __tg_fmax(x,y);
}

static inline SIMD_CFUNC simd_float2 simd_max(simd_float2 x, simd_float2 y) {
    return __tg_fmax(x,y);
}

static inline SIMD_CFUNC simd_float3 simd_max(simd_float3 x, simd_float3 y) {
    return __tg_fmax(x,y);
}

static inline SIMD_CFUNC simd_float4 simd_max(simd_float4 x, simd_float4 y) {
    return __tg_fmax(x,y);
}

static inline SIMD_CFUNC simd_double2 simd_max(simd_double2 x, simd_double2 y) {
    return __tg_fmax(x,y);
}

SIMD_CFUNC simd_float2 __tg_fmin(simd_float2 a, simd_float2 b);
SIMD_CFUNC simd_float3 __tg_fmin(simd_float3 a, simd_float3 b);
SIMD_CFUNC simd_float4 __tg_fmin(simd_float4 a, simd_float4 b);

SIMD_CFUNC simd_double2 __tg_fmin(simd_double2 a, simd_double2 b);

SIMD_CFUNC float __tg_fmin(float a, float b);

static inline SIMD_CFUNC simd_float2 simd_min(simd_float2 x, simd_float2 y) {
    return __tg_fmin(x,y);
}

static inline SIMD_CFUNC simd_float3 simd_min(simd_float3 x, simd_float3 y) {
    return __tg_fmin(x,y);
}

static inline SIMD_CFUNC simd_float4 simd_min(simd_float4 x, simd_float4 y) {
    return __tg_fmin(x,y);
}

static inline SIMD_CFUNC simd_double2 simd_min(simd_double2 x, simd_double2 y) {
    return __tg_fmin(x,y);
}

static inline SIMD_CFUNC float simd_min(float x, float y) {
    return __tg_fmin(x,y);
}

static inline SIMD_CFUNC float simd_clamp(float x, float min, float max) {
    return (x < min) ? min : ((x > max) ? max : x);
}

static inline SIMD_CFUNC simd_float2 simd_clamp(simd_float2 x, simd_float2 min, simd_float2 max) {
  return simd_min(simd_max(x, min), max);
}

static inline SIMD_CFUNC simd_float3 simd_clamp(simd_float3 x, simd_float3 min, simd_float3 max) {
  return simd_min(simd_max(x, min), max);
}

static inline SIMD_CFUNC simd_float4 simd_clamp(simd_float4 x, simd_float4 min, simd_float4 max) {
  return simd_min(simd_max(x, min), max);
}

static inline SIMD_CFUNC simd_int2 simd_make_int2(int x, int y) {
  simd_int2 result;
  result.x = x;
  result.y = y;
  return result;
}

static inline SIMD_CFUNC simd_int3 simd_make_int3(int x, int y,int z) {
    simd_int3 result;
    result.x = x;
    result.y = y;
    result.z = z;
    return result;
}

static inline SIMD_CFUNC simd_int4 simd_make_int4(int x, int y,int z,int w) {
    simd_int4 result;
    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;
    return result;
}

static inline SIMD_CFUNC simd_float2 simd_make_float2(float other) {
    simd_float2 result = { other, other };
    return result;
}

static inline SIMD_CFUNC simd_float2 simd_make_float2(float x, float y) {
  simd_float2 result;
  result.x = x;
  result.y = y;
  return result;
}

static inline SIMD_CFUNC simd_float3 simd_make_float3(float other) {
    simd_float3 result = { other, other, other };
    return result;
}

static inline SIMD_CFUNC simd_float3 simd_make_float3(float x,float y,float z) {
    simd_float3 result = { x, y, z };
    return result;
}

static inline SIMD_CFUNC simd_float4 simd_make_float4(float other) {
  simd_float4 result = 0;
  result.x = other;
  return result;
}

static inline SIMD_CFUNC simd_float4 simd_make_float4(float x, float y, float z, float w) {
  simd_float4 result;
  result.x = x;
  result.y = y;
  result.z = z;
  result.w = w;
  return result;
}

static inline SIMD_CFUNC simd_double2 simd_make_double2(double x, double y) {
  simd_double2 result;
  result.x = x;
  result.y = y;
  return result;
}

//Smoothstep
static inline SIMD_CFUNC float simd_smoothstep(float edge0, float edge1, float x) {
  float t = simd_clamp((x - edge0)/(edge1 - edge0), 0, 1);
  return t*t*(3 - 2*t);
}

static inline SIMD_CFUNC simd_float4 simd_smoothstep(simd_float4 edge0, simd_float4 edge1, simd_float4 x) {
  simd_float4 t = simd_clamp((x - edge0)/(edge1 - edge0), 0, 1);
  return t*t*(3 - 2*t);
}

static float  SIMD_CFUNC simd_dot(simd_float2  __x, simd_float2  __y) { return simd_reduce_add(__x*__y); }
static float  SIMD_CFUNC simd_dot(simd_float3  __x, simd_float3  __y) { return simd_reduce_add(__x*__y); }
static float  SIMD_CFUNC simd_dot(simd_float4  __x, simd_float4  __y) { return simd_reduce_add(__x*__y); }
static double SIMD_CFUNC simd_dot(simd_double2 __x, simd_double2 __y) { return simd_reduce_add(__x*__y); }

/*! @abstract Do not call this function; instead use `fabs` in C and
 *  Objective-C, and `simd::fabs` in C++.                                     */
static inline SIMD_CFUNC simd_float2 __tg_fabs(simd_float2 x);
/*! @abstract Do not call this function; instead use `fabs` in C and
 *  Objective-C, and `simd::fabs` in C++.                                     */
static inline SIMD_CFUNC simd_float3 __tg_fabs(simd_float3 x);
/*! @abstract Do not call this function; instead use `fabs` in C and
 *  Objective-C, and `simd::fabs` in C++.                                     */
static inline SIMD_CFUNC simd_float4 __tg_fabs(simd_float4 x);
/*! @abstract Do not call this function; instead use `fabs` in C and
 *  Objective-C, and `simd::fabs` in C++.                                     */
static inline SIMD_CFUNC simd_double2 __tg_fabs(simd_double2 x);
/*! @abstract Do not call this function; instead use `fabs` in C and
 *  Objective-C, and `simd::fabs` in C++.                                     */





