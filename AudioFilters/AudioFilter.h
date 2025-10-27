
//  AudioFilter.h
//  UnyteAudioFilterTest
//
//  Created by Nguyen Minh Tien on 16/10/2025.
//

#ifndef AudioFilter_h
#define AudioFilter_h

#ifdef __APPLE__
    #define USE_ACCELERATE 1
    //For testing purpose only
    #define USE_NEON 1
#else
    #define USE_ACCELERATE 0
    //Never change this
    #define USE_NEON 1
#endif

#if USE_ACCELERATE
    #include <Accelerate/Accelerate.h>
    #include <arm_neon.h>

    #define my_float64x2_t float64x2_t
    #define my_float64x2_t float64x2_t
    #define my_vdupq_n_f64 vdupq_n_f64
    #define my_vfmaq_f64 vfmaq_f64
    #define my_vaddvq_f64 vaddvq_f64
    #define my_vgetq_lane_f64 vgetq_lane_f64
#else
    #include "../AudioFilters/CrossPlatform/BMVDSP.h"  // your custom vDSP replacement

#if defined(__aarch64__) && USE_NEON
    #include <arm_neon.h>
    
    #define my_float64x2_t float64x2_t
    #define my_float64x2_t float64x2_t
    #define my_vdupq_n_f64 vdupq_n_f64
    #define my_vfmaq_f64 vfmaq_f64
    #define my_vaddvq_f64 vaddvq_f64
    #define my_vgetq_lane_f64 vgetq_lane_f64

#else
    typedef struct { double val[2]; } my_float64x2_t;
    static inline my_float64x2_t my_vdupq_n_f64(double x) {
        my_float64x2_t v = { { x, x } };
        return v;
    }

    static inline my_float64x2_t my_vfmaq_f64(my_float64x2_t a, my_float64x2_t b, my_float64x2_t c) {
        my_float64x2_t v;
        v.val[0] = a.val[0] + b.val[0] * c.val[0];
        v.val[1] = a.val[1] + b.val[1] * c.val[1];
        return v;
    }

    static inline double my_vaddvq_f64(my_float64x2_t v) {
        return v.val[0] + v.val[1];
    }

    static inline double my_vgetq_lane_f64(my_float64x2_t v, int lane) {
        return v.val[lane];
    }

#endif

    #define vDSP_vsmul bDSP_vsmul
    #define vDSP_vdbcon bDSP_vdbcon
    #define vDSP_vramp bDSP_vramp
    #define vDSP_vmul bDSP_vmul
    #define vDSP_vadd bDSP_vadd
    #define vDSP_vfill bDSP_vfill
    #define vDSP_vsma bDSP_vsma
    #define vDSP_vgathr bDSP_vgathr
    #define vDSP_dotpr bDSP_dotpr
    #define vDSP_vclr bDSP_vclr
    #define vDSP_vsadd bDSP_vsadd
    #define vDSP_vmma bDSP_vmma
    #define vDSP_vma bDSP_vma
    #define vDSP_vthres bDSP_vthres
    #define vDSP_vthr bDSP_vthr
    #define vDSP_vabs bDSP_vabs
    #define vDSP_vdbcon bDSP_vdbcon
    #define vDSP_maxv bDSP_maxv
    #define vDSP_conv bDSP_conv
    #define vDSP_vclip bDSP_vclip
    #define vDSP_vlint bDSP_vlint
    #define vDSP_vpoly bDSP_vpoly
    #define vDSP_vneg bDSP_vneg
    #define vDSP_vacD bDSP_vacD
    #define vDSP_vmax bDSP_vmax
    #define vDSP_vsbsm bDSP_vsbsm
    #define vDSP_vasm bDSP_vasm
    #define vDSP_vswsum bDSP_vswsum
    #define vDSP_vmsa bDSP_vmsa
    #define vDSP_vdiv bDSP_vdiv
    #define vDSP_vsub bDSP_vsub
    #define vDSP_vfix32 bDSP_vfix32
    #define vDSP_maxmgv bDSP_maxmgv
    #define vDSP_vsaddi bDSP_vsaddi
    #define vDSP_meanv bDSP_meanv
    #define vDSP_measqv bDSP_measqv
    #define vDSP_svesq bDSP_svesq
    #define vDSP_maxmgvD bDSP_maxmgvD
    #define vDSP_vdivD bDSP_vdivD
    #define vDSP_vsdivD bDSP_vsdivD
    #define vDSP_vsmulD bDSP_vsmulD
    #define vDSP_vflt32D bDSP_vflt32D
    #define vDSP_vsmsaD bDSP_vsmsaD
    #define vDSP_vsaddD bDSP_vsaddD
    #define vDSP_vdpsp bDSP_vdpsp
    #define vDSP_vmin bDSP_vmin
    #define vDSP_vmaxmg bDSP_vmaxmg
    #define vDSP_vminmg bDSP_vminmg
    #define vDSP_vsmsma bDSP_vsmsma
    #define vDSP_sve bDSP_sve
    #define vDSP_vsmsa bDSP_vsmsa
    #define vDSP_vdist bDSP_vdist
    #define vDSP_hamm_window bDSP_hamm_window
    #define vDSP_desamp bDSP_desamp
    #define vDSP_svdiv bDSP_svdiv
    #define vDSP_vsq bDSP_vsq
    #define vDSP_vrsum bDSP_vrsum
    #define vDSP_vflt16 bDSP_vflt16
    #define vDSP_minv bDSP_minv
    #define vDSP_vgen bDSP_vgen
    #define vDSP_vqint bDSP_vqint
    #define vDSP_maxmgvi bDSP_maxmgvi
    #define vDSP_vfltu32 bDSP_vfltu32
    #define vDSP_vfixru8 bDSP_vfixru8
    #define vDSP_vrvrs bDSP_vrvrs
    #define vDSP_mtrans bDSP_mtrans
    #define vDSP_blkman_window bDSP_blkman_window
    #define vDSP_hann_window bDSP_hann_window
    #define vDSP_svemg bDSP_svemg
    #define vDSP_vrampmul2 bDSP_vrampmul2
    #define vDSP_vrampmul bDSP_vrampmul
    #define vDSP_vneg bDSP_vneg

    #define vvtanf bvtanf
    #define vvrecf bvrecf
    #define vvexp2f bvexp2f
    #define vvfloorf bvfloorf
    #define vvlog2f bvlog2f
    #define vvlog2 bvlog2
    #define vvpowsf bvpowsf
    #define vvfabsf bvfabsf
    #define vvcopysignf bvcopysignf
    #define vvsinpif bvsinpif
    #define vvtanhf bvtanhf
    #define vvfmodf bvfmodf
    #define vvexpf bvexpf
    #define vvceilf bvceilf

    #define DSPSplitComplex BSPSplitComplex
    #define DSPComplex BSPComplex
    #define DSPDoubleComplex BSPDoubleComplex

#endif

#endif /* AudioFilter_h */
