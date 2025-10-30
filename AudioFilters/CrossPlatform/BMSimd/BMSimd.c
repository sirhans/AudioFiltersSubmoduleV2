// bmsimd.c
#include "BMSimd.h"
#include <stdint.h>
#include <string.h> // for __builtin_memcpy (Clang provides builtin, falls back to memcpy)


/*****************************************
 *    SIMD FUNCTIONS FROM simd/common.h  *
 *****************************************/

// simd_abs
inline SIMD_CFUNC simd_float2 simd_abs(simd_float2 x) {
    return __tg_fabs(x);
}

inline SIMD_CFUNC simd_float3 simd_abs(simd_float3 x) {
    return __tg_fabs(x);
}

inline SIMD_CFUNC simd_float4 simd_abs(simd_float4 x) {
    return __tg_fabs(x);
}

inline SIMD_CFUNC simd_double2 simd_abs(simd_double2 x) {
    return __tg_fabs(x);
}


/*****************************************
 *  Helpers: bitcast between vector types
 *****************************************/

static inline simd_uint2 bitcast_u32x2_from_f32x2(simd_float2 x) {
    simd_uint2 r;
    __builtin_memcpy(&r, &x, sizeof(r));
    return r;
}
static inline simd_float2 bitcast_f32x2_from_u32x2(simd_uint2 x) {
    simd_float2 r;
    __builtin_memcpy(&r, &x, sizeof(r));
    return r;
}

static inline simd_uint3 bitcast_u32x3_from_f32x3(simd_float3 x) {
    simd_uint3 r;
    __builtin_memcpy(&r, &x, sizeof(r));
    return r;
}
static inline simd_float3 bitcast_f32x3_from_u32x3(simd_uint3 x) {
    simd_float3 r;
    __builtin_memcpy(&r, &x, sizeof(r));
    return r;
}

static inline simd_uint4 bitcast_u32x4_from_f32x4(simd_float4 x) {
    simd_uint4 r;
    __builtin_memcpy(&r, &x, sizeof(r));
    return r;
}
static inline simd_float4 bitcast_f32x4_from_u32x4(simd_uint4 x) {
    simd_float4 r;
    __builtin_memcpy(&r, &x, sizeof(r));
    return r;
}

/* Internal 64-bit unsigned vector type for double bit-masking. */
typedef __attribute__((__ext_vector_type__(2))) unsigned long long simd_ull2;

static inline simd_ull2 bitcast_u64x2_from_f64x2(simd_double2 x) {
    simd_ull2 r;
    __builtin_memcpy(&r, &x, sizeof(r));
    return r;
}
static inline simd_double2 bitcast_f64x2_from_u64x2(simd_ull2 x) {
    simd_double2 r;
    __builtin_memcpy(&r, &x, sizeof(r));
    return r;
}

/*****************************************
 *  __tg_fabs implementations (bit-mask)
 *****************************************/

/* Clear sign bit for 32-bit floats: mask = 0x7FFFFFFF per lane */
SIMD_CFUNC simd_float2 __tg_fabs(simd_float2 x) {
    const simd_uint2 mask = (simd_uint2){ 0x7FFFFFFFu, 0x7FFFFFFFu };
    simd_uint2 ux = bitcast_u32x2_from_f32x2(x);
    ux &= mask;
    return bitcast_f32x2_from_u32x2(ux);
}

SIMD_CFUNC simd_float3 __tg_fabs(simd_float3 x) {
    const simd_uint3 mask = (simd_uint3){ 0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu };
    simd_uint3 ux = bitcast_u32x3_from_f32x3(x);
    ux &= mask;
    return bitcast_f32x3_from_u32x3(ux);
}

SIMD_CFUNC simd_float4 __tg_fabs(simd_float4 x) {
    const simd_uint4 mask = (simd_uint4){ 0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu };
    simd_uint4 ux = bitcast_u32x4_from_f32x4(x);
    ux &= mask;
    return bitcast_f32x4_from_u32x4(ux);
}

/* Clear sign bit for 64-bit doubles: mask = 0x7FFF...FFFF per lane */
SIMD_CFUNC simd_double2 __tg_fabs(simd_double2 x) {
    const simd_ull2 mask = (simd_ull2){ 0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL };
    simd_ull2 ux = bitcast_u64x2_from_f64x2(x);
    ux &= mask;
    return bitcast_f64x2_from_u64x2(ux);
}

SIMD_CFUNC simd_float2 __tg_fmax(simd_float2 a, simd_float2 b) {
#if BM_HAS_NEON && (defined(__aarch64__) || defined(__arm64__) || defined(__ARM_NEON__))
    // NEON supports 2-lane float vectors directly
    return vmax_f32(a, b);
#else
    // Fallback for architectures without NEON
    simd_float2 r;
    r[0] = (a[0] > b[0]) ? a[0] : b[0];
    r[1] = (a[1] > b[1]) ? a[1] : b[1];
    return r;
#endif
}

SIMD_CFUNC simd_float3 __tg_fmax(simd_float3 a, simd_float3 b) {
#if BM_HAS_NEON && (defined(__aarch64__) || defined(__arm64__) || defined(__ARM_NEON__))
    // NEON doesn't have a 3-lane vector type, use 4 lanes and ignore the last
    float32x4_t va = vld1q_f32((const float*)&a);
    float32x4_t vb = vld1q_f32((const float*)&b);
    float32x4_t vr = vmaxq_f32(va, vb);
    simd_float3 r;
    vst1q_f32((float*)&r, vr);
    return r;
#else
    simd_float3 r;
    r[0] = (a[0] > b[0]) ? a[0] : b[0];
    r[1] = (a[1] > b[1]) ? a[1] : b[1];
    r[2] = (a[2] > b[2]) ? a[2] : b[2];
    return r;
#endif
}

SIMD_CFUNC simd_float4 __tg_fmax(simd_float4 a, simd_float4 b) {
#if BM_HAS_NEON && (defined(__aarch64__) || defined(__arm64__) || defined(__ARM_NEON__))
    float32x4_t va = vld1q_f32((const float*)&a);
    float32x4_t vb = vld1q_f32((const float*)&b);
    float32x4_t vr = vmaxq_f32(va, vb);
    simd_float4 r;
    vst1q_f32((float*)&r, vr);
    return r;
#else
    simd_float4 r;
    r[0] = (a[0] > b[0]) ? a[0] : b[0];
    r[1] = (a[1] > b[1]) ? a[1] : b[1];
    r[2] = (a[2] > b[2]) ? a[2] : b[2];
    r[3] = (a[3] > b[3]) ? a[3] : b[3];
    return r;
#endif
}

SIMD_CFUNC simd_double2 __tg_fmax(simd_double2 a, simd_double2 b) {
#if BM_HAS_NEON && (defined(__aarch64__) || defined(__arm64__))
    // ARMv8 supports 2-lane double vectors directly
    return vmaxq_f64(a, b);
#else
    // Fallback for 32-bit ARM (no NEON f64 support)
    simd_double2 r;
    r[0] = (a[0] > b[0]) ? a[0] : b[0];
    r[1] = (a[1] > b[1]) ? a[1] : b[1];
    return r;
#endif
}

SIMD_CFUNC float __tg_fmax(float a, float b) {
    return (a > b) ? a : b;
}

SIMD_CFUNC simd_float2 __tg_fmin(simd_float2 a, simd_float2 b) {
#if BM_HAS_NEON && (defined(__aarch64__) || defined(__arm64__) || defined(__ARM_NEON__))
    // NEON supports 2-lane float vectors directly
    return vmin_f32(a, b);
#else
    // Fallback for architectures without NEON
    simd_float2 r;
    r[0] = (a[0] < b[0]) ? a[0] : b[0];
    r[1] = (a[1] < b[1]) ? a[1] : b[1];
    return r;
#endif
}

SIMD_CFUNC simd_float3 __tg_fmin(simd_float3 a, simd_float3 b) {
#if BM_HAS_NEON && (defined(__aarch64__) || defined(__arm64__) || defined(__ARM_NEON__))
    // NEON doesn't have a native 3-lane vector type, so use 4 lanes and ignore the last
    float32x4_t va = vld1q_f32((const float*)&a);
    float32x4_t vb = vld1q_f32((const float*)&b);
    float32x4_t vr = vminq_f32(va, vb);
    simd_float3 r;
    vst1q_f32((float*)&r, vr);
    return r;
#else
    simd_float3 r;
    r[0] = (a[0] < b[0]) ? a[0] : b[0];
    r[1] = (a[1] < b[1]) ? a[1] : b[1];
    r[2] = (a[2] < b[2]) ? a[2] : b[2];
    return r;
#endif
}

SIMD_CFUNC simd_float4 __tg_fmin(simd_float4 a, simd_float4 b) {
#if BM_HAS_NEON && (defined(__aarch64__) || defined(__arm64__) || defined(__ARM_NEON__))
    float32x4_t va = vld1q_f32((const float*)&a);
    float32x4_t vb = vld1q_f32((const float*)&b);
    float32x4_t vr = vminq_f32(va, vb);
    simd_float4 r;
    vst1q_f32((float*)&r, vr);
    return r;
#else
    simd_float4 r;
    r[0] = (a[0] < b[0]) ? a[0] : b[0];
    r[1] = (a[1] < b[1]) ? a[1] : b[1];
    r[2] = (a[2] < b[2]) ? a[2] : b[2];
    r[3] = (a[3] < b[3]) ? a[3] : b[3];
    return r;
#endif
}


SIMD_CFUNC simd_double2 __tg_fmin(simd_double2 a, simd_double2 b) {
#if BM_HAS_NEON && (defined(__aarch64__) || defined(__arm64__))
    // ARMv8 NEON supports float64x2_t directly
    return vminq_f64(a, b);
#else
    // Fallback for 32-bit ARM (no NEON float64 support)
    simd_double2 r;
    r[0] = (a[0] < b[0]) ? a[0] : b[0];
    r[1] = (a[1] < b[1]) ? a[1] : b[1];
    return r;
#endif
}

SIMD_CFUNC float __tg_fmin(float a, float b) {
    return (a < b) ? a : b;
}

