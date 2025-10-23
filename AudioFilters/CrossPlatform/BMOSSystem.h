//
//  BMOSSystem.h
//  BMJuceAudioFilter
//
//  Created by Nguyen Minh Tien on 10/10/2024.
//

#ifndef BMOSSystem_h
#define BMOSSystem_h

#include <stdio.h>

//#ifdef __cplusplus
//extern "C" {
//#endif

/**
 * Determination a platform of an operation system
 * Fully supported supported only GNU GCC/G++, partially on Clang/LLVM
 */
// Windows ||           Windows ||        Windows (Cygwin POSIX under Microsoft Window)
#if defined(_WIN32) || defined(_WIN64) || (defined(__CYGWIN__) && !defined(_WIN32)) //|| defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_NAME "windows" // Windows
    // 256 bit vectors
    typedef unsigned vUint32_8;
    typedef float vFloat32_8;
    typedef int vSInt32_8;

    // larger vectors
    typedef float vFloat32_32;
    typedef int vSInt32_32;
    typedef unsigned vUInt32_32;
    typedef double              Float64;
    typedef int long;

    typedef unsigned char          vUInt8;
    typedef signed char            vSInt8;
    typedef unsigned short         vUInt16;
    typedef signed short           vSInt16;
    typedef unsigned int           vUInt32;
    typedef signed int             vSInt32;
    typedef unsigned long long     vUInt64;
    typedef long long              vSInt64;
    typedef float                  vFloat;
    typedef double                 vDouble;
    typedef unsigned int           vBool32;

    typedef float                  vFloat;
    static inline float vfabsf(vFloat v){
        return fabsf(v);
    }
    
    //BMSimd
    #include "../CrossPlatform/BMSimd.h"

    #define M_E         2.71828182845904523536028747135266250   /* e              */
    #define M_LOG2E     1.44269504088896340735992468100189214   /* log2(e)        */
    #define M_LOG10E    0.434294481903251827651128918916605082  /* log10(e)       */
    #define M_LN2       0.693147180559945309417232121458176568  /* loge(2)        */
    #define M_LN10      2.30258509299404568401799145468436421   /* loge(10)       */
    #define M_PI        3.14159265358979323846264338327950288   /* pi             */
    #define M_PI_2      1.57079632679489661923132169163975144   /* pi/2           */
    #define M_PI_4      0.785398163397448309615660845819875721  /* pi/4           */
    #define M_1_PI      0.318309886183790671537767526745028724  /* 1/pi           */
    #define M_2_PI      0.636619772367581343075535053490057448  /* 2/pi           */
    #define M_2_SQRTPI  1.12837916709551257389615890312154517   /* 2/sqrt(pi)     */
    #define M_SQRT2     1.41421356237309504880168872420969808   /* sqrt(2)        */
    #define M_SQRT1_2   0.707106781186547524400844362104849039  /* 1/sqrt(2)      */

    #define ConvertV(A,B) (B) A
#elif defined(__ANDROID__)
    #define PLATFORM_NAME "android" // Android (implies Linux, so it must come first)
    // 256 bit vectors
    typedef unsigned vUint32_8;
    typedef float vFloat32_8;
    typedef int vSInt32_8;

    // larger vectors
    typedef float vFloat32_32;
    typedef int vSInt32_32;
    typedef unsigned vUInt32_32;
    typedef double              Float64;

    typedef unsigned char          vUInt8;
    typedef signed char            vSInt8;
    typedef unsigned short         vUInt16;
    typedef signed short           vSInt16;
    typedef unsigned int           vUInt32;
    typedef signed int             vSInt32;
    typedef unsigned long long     vUInt64;
    typedef long long              vSInt64;
    typedef float                  vFloat;
    typedef double                 vDouble;
    typedef unsigned int           vBool32;

    typedef float                  vFloat;
    static inline float vfabsf(vFloat v){
        return fabsf(v);
    }

    //BMSimd
//    #include "../CrossPlatform/BMSimd.h"

    #define ConvertV(A,B) (B) A

#elif defined(__APPLE__) && defined(__MACH__) // Apple OSX and iOS (Darwin)
    #include <TargetConditionals.h>
    #include <Accelerate/Accelerate.h>
    #include <simd/simd.h>
    
    // 256 bit vectors
    typedef unsigned vUint32_8 __attribute__((ext_vector_type(8),aligned(4)));
    typedef float vFloat32_8 __attribute__((ext_vector_type(8),aligned(4)));
    typedef int vSInt32_8 __attribute__((ext_vector_type(8),aligned(4)));

    // larger vectors
    typedef float vFloat32_32 __attribute__((ext_vector_type(32),aligned(4)));
    typedef int vSInt32_32 __attribute__((ext_vector_type(32),aligned(4)));
    typedef unsigned vUInt32_32 __attribute__((ext_vector_type(32),aligned(4)));

    typedef unsigned vUint64_2 __attribute__((ext_vector_type(2),aligned(4)));
    typedef signed vSint32_2 __attribute__((ext_vector_type(2),aligned(4)));

    #define ConvertV(A,B) __builtin_convertvector(A,B)

    #if TARGET_IPHONE_SIMULATOR == 1
        #define PLATFORM_NAME "ios" // Apple iOS
    #elif TARGET_OS_IPHONE == 1
        #define PLATFORM_NAME "ios" // Apple iOS
    #elif TARGET_OS_MAC == 1
        #define PLATFORM_NAME "osx" // Apple OSX
    #endif
#else
    #define PLATFORM_NAME NULL
#endif

//#ifdef __cplusplus
//}
//#endif

#endif /* BMOSSystem_h */
