//
// Created by Nguyen Minh Tien on 30/10/2025.
//

#ifndef UNYTE_ILS_ANDROID_BMSIMDBASE_H
#define UNYTE_ILS_ANDROID_BMSIMDBASE_H

#if defined(__ARM_NEON) && (defined(__ARM_PCS_VFP) || defined(__aarch64__))
#include <arm_neon.h>
#define BM_HAS_NEON 1
#else
#define BM_HAS_NEON 0
#endif

/*  Define a number of function attributes used by the simd functions.        */
#  if __has_attribute(__always_inline__)
#   define SIMD_INLINE  __attribute__((__always_inline__))
#  else
#   define SIMD_INLINE  inline
#  endif

#  if __has_attribute(__const__)
#   define SIMD_CONST   __attribute__((__const__))
#  else
#   define SIMD_CONST   /* nothing */
#  endif

#  if __has_attribute(__nodebug__)
#   define SIMD_NODEBUG __attribute__((__nodebug__))
#  else
#   define SIMD_NODEBUG /* nothing */
#  endif

#  if __has_attribute(__deprecated__)
#   define SIMD_DEPRECATED(message) __attribute__((__deprecated__(message)))
#  else
#   define SIMD_DEPRECATED(message) /* nothing */
#  endif

#define SIMD_OVERLOAD __attribute__((__overloadable__))
#define SIMD_CPPFUNC  SIMD_INLINE SIMD_CONST SIMD_NODEBUG
#define SIMD_CFUNC    SIMD_CPPFUNC SIMD_OVERLOAD
#define SIMD_NOINLINE SIMD_CONST SIMD_NODEBUG SIMD_OVERLOAD
#define SIMD_NONCONST SIMD_INLINE SIMD_NODEBUG SIMD_OVERLOAD
#define __SIMD_INLINE__     SIMD_CPPFUNC
#define __SIMD_ATTRIBUTES__ SIMD_CFUNC
#define __SIMD_OVERLOAD__   SIMD_OVERLOAD

#if defined __cplusplus
/*! @abstract A boolean scalar.                                               */
typedef  bool simd_bool;
#else
/*! @abstract A boolean scalar.                                               */
typedef _Bool simd_bool;
#endif
/*! @abstract A boolean scalar.
 *  @discussion This type is deprecated; In C or Objective-C sources, use
 *  `_Bool` instead. In C++ sources, use `bool`.                              */
typedef simd_bool __SIMD_BOOLEAN_TYPE__;

#endif //UNYTE_ILS_ANDROID_BMSIMDBASE_H
