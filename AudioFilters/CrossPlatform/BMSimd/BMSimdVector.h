//
// Created by Nguyen Minh Tien on 30/10/2025.
//

#ifndef UNYTE_ILS_ANDROID_BMSIMDVECTOR_H
#define UNYTE_ILS_ANDROID_BMSIMDVECTOR_H

/******************
 *    TYPEDEFS from vectortypes.h   *
 ******************/
typedef unsigned int simd_uint1;
typedef __attribute__((__ext_vector_type__(2))) unsigned int simd_uint2;
typedef __attribute__((__ext_vector_type__(3))) unsigned int simd_uint3;
typedef __attribute__((__ext_vector_type__(4))) unsigned int simd_uint4;
typedef __attribute__((__ext_vector_type__(8),__aligned__(16))) unsigned int simd_uint8;
typedef __attribute__((__ext_vector_type__(16),__aligned__(16))) unsigned int simd_uint16;
typedef __attribute__((__ext_vector_type__(2))) unsigned long long simd_ull2;

typedef float simd_float1;
typedef __attribute__((__ext_vector_type__(2))) float simd_float2;
typedef __attribute__((__ext_vector_type__(3))) float simd_float3;
typedef __attribute__((__ext_vector_type__(4))) float simd_float4;
typedef __attribute__((__ext_vector_type__(8))) float simd_float8;

typedef double simd_double1;
typedef __attribute__((__ext_vector_type__(2))) double simd_double2;
typedef __attribute__((__ext_vector_type__(3),__aligned__(16))) double simd_double3;
typedef __attribute__((__ext_vector_type__(4),__aligned__(16))) double simd_double4;
typedef __attribute__((__ext_vector_type__(8),__aligned__(16))) double simd_double8;

typedef __attribute__((__ext_vector_type__(2))) int simd_int2;
typedef __attribute__((__ext_vector_type__(3))) int simd_int3;
typedef __attribute__((__ext_vector_type__(4))) int simd_int4;

#if defined __LP64__
typedef long simd_long1;
#else
typedef long long simd_long1;
#endif

typedef __attribute__((__ext_vector_type__(2))) simd_long1 simd_long2;

typedef struct { simd_float2 columns[2]; } simd_float2x2;

/*! @abstract A matrix with 2 rows and 3 columns.                             */
typedef struct { simd_float2 columns[3]; } simd_float3x2;

/*! @abstract A matrix with 2 rows and 4 columns.                             */
typedef struct { simd_float2 columns[4]; } simd_float4x2;

/*! @abstract A matrix with 3 rows and 2 columns.                             */
typedef struct { simd_float3 columns[2]; } simd_float2x3;

/*! @abstract A matrix with 3 rows and 3 columns.                             */
typedef struct { simd_float3 columns[3]; } simd_float3x3;

/*! @abstract A matrix with 3 rows and 4 columns.                             */
typedef struct { simd_float3 columns[4]; } simd_float4x3;

/*! @abstract A matrix with 4 rows and 2 columns.                             */
typedef struct { simd_float4 columns[2]; } simd_float2x4;

/*! @abstract A matrix with 4 rows and 3 columns.                             */
typedef struct { simd_float4 columns[3]; } simd_float3x4;

/*! @abstract A matrix with 4 rows and 4 columns.                             */
typedef struct { simd_float4 columns[4]; } simd_float4x4;

/*! @abstract A matrix with 2 rows and 2 columns.                             */
typedef struct { simd_double2 columns[2]; } simd_double2x2;



#endif //UNYTE_ILS_ANDROID_BMSIMDVECTOR_H
