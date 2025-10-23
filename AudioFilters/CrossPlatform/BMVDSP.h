//
// Created by hans anderson on 7/4/23.
//

#ifndef THERAPYFILTER_BMANDROIDVDSP_H
#define THERAPYFILTER_BMANDROIDVDSP_H

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include "../Constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *
 * @param initialValue init value
 * @param increment increment value
 * @param output output
 * @param stride stride
 * @param numSamples samplecount
 */
void bDSP_vramp(
        const float *initialValue,
        const float *increment,
        float       *output,
        size_t  stride,
        size_t  numSamples);

/*!
 *
 * @param V input array, length numSamples
 * @param strideV stride of v
 * @param S scalar multiplier
 * @param output output array, length numSamples
 * @param strideOutput stride of output
 * @param numSamples length of input and output arrays
 */
void bDSP_vsmul(const float *V,
                size_t strideV,
                const float *S,
                float *output,
                size_t strideOutput,
                size_t numSamples);

/*!
 *
 * @param A input array
 * @param strideA stride of A
 * @param B input array
 * @param strideB stride of B
 * @param C output array
 * @param strideC stride of C
 * @param numSamples sample count
 */
void bDSP_vmul(const float *A,
               size_t strideA,
               const float *B,
               size_t strideB,
               float *C,
               size_t strideC,
               size_t numSamples);


/*!
 *
 * @param A input array
 * @param strideA stride of A
 * @param B input array
 * @param strideB stride of B
 * @param C output array
 * @param strideC stride of C
 * @param numSamples sample count
 */
void bDSP_vadd(const float *A,
               size_t strideA,
               const float *B,
               size_t strideB,
               float *C,
               size_t strideC,
               size_t numSamples);

/*!
 *
 * @param A a scalar value
 * @param C an empty array of length numSamples * strideC, for output
 * @param strideC stride in C
 * @param numSamples length of C / strideC
 */
void bDSP_vfill(const float *A,
                float *C,
                size_t strideC,
                size_t numSamples);

/*!
 *
 * @param A a scalar value
 * @param IA stride of A
 * @param B a scalar value
 * @param C a scalar value
 * @param IC stride of C
 * @param D a scalar value
 * @param ID stride of D
 * @param N sample count
 */
void bDSP_vsma(const float *A,
               size_t  IA,
               const float *B,
               const float *C,
               size_t  IC,
               float       *D,
               size_t  ID,
               size_t  N);

void bDSP_vgathr(const float       *A,
                 const size_t *B,
                 size_t        IB,
                 float             *C,
                 size_t        IC,
                 size_t        N);

void bDSP_dotpr(const float *A,
                size_t  IA,
                const float *B,
                size_t  IB,
                float       *C,
                size_t  N);

void bDSP_vclr(
               float   *C,
               size_t  IC,
               size_t  N);

void bDSP_vsadd(
                const float *A,
                size_t  IA,
                const float *B,
                float       *C,
                size_t  IC,
                size_t  N);

void bDSP_vmma(const float *A,
                size_t  IA,
                const float *B,
                size_t  IB,
                const float *C,
                size_t  IC,
                const float *D,
                size_t  ID,
                float       *E,
                size_t  IE,
               size_t  N);

void bDSP_vma(
                const float *A,
                size_t  IA,
                const float *B,
                size_t  IB,
                const float *C,
                size_t  IC,
                float       *D,
                size_t  ID,
              size_t  N);

void bDSP_vthres(
                const float *A,
                size_t  IA,
                const float *B,
                float       *C,
                 size_t  IC,
                 size_t  N);

void bDSP_vthr(
                const float *A,
                size_t  IA,
                const float *B,
                float       *C,
               size_t  IC,
               size_t  N);

void bDSP_vabs(
                const float *A,
                size_t  IA,
                float       *C,
                size_t  IC,
               size_t  N);

void bDSP_vdbcon(
                const float *A,
                size_t  IA,
                const float *B,
                float       *C,
                size_t  IC,
                size_t  N,
                 size_t F);

void bDSP_maxv(
                const float *A,
                size_t  IA,
                float       *C,
               size_t  N);

void bDSP_conv(
                const float *A,  // Input signal.
                size_t  IA,
                const float *F,  // Filter.
               long  IF,
               float       *C,  // Output signal.
               size_t  IC,
               size_t  N,  // Output length.
               size_t  P);

void bDSP_vclip(
                const float *A,
                size_t  IA,
                const float *B,
                const float *C,
                float       *D,
                size_t  ID,
                size_t  N);

void bDSP_vlint(
                const float *A,
                const float *B,
                size_t  IB,
                float       *C,
                size_t  IC,
                size_t  N,
                size_t  M);

void bDSP_vpoly(
                const float *A,
                size_t  IA,
                const float *B,
                size_t  IB,
                float       *C,
                size_t  IC,
                size_t  N,
                size_t  P);

void bDSP_vneg(
                const float *A,
                size_t  IA,
                float       *C,
                size_t  IC,
               size_t  N);

bool bDSP_vacD(double* array1,
               double* array2,
               size_t length);

void bDSP_vmax(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
               size_t  N);

void bDSP_vsbsm(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                const float *C,
                float       *D,
                long  ID,
                size_t  N);

void bDSP_vasm(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                const float *C,
                float       *D,
                long  ID,
               size_t  N);

void bDSP_vswsum(
                const float *A,
                long  IA,
                float       *C,
                long  IC,
                size_t  N,
                 size_t  P);

void bDSP_vmsa(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                const float *C,
                float       *D,
                long  ID,
               size_t  N);

void bDSP_vdiv(
                const float *B,  // Caution:  A and B are swapped!
                long  IB,
                const float *A,  // Caution:  A and B are swapped!
                long  IA,
                float       *C,
                long  IC,
               size_t  N);

void bDSP_vsub(
            const float *B,  // Caution:  A and B are swapped!
            long  IB,
            const float *A,  // Caution:  A and B are swapped!
            long  IA,
            float       *C,
            long  IC,
               size_t  N);

void bDSP_vfix32(
                const float *A,
                long  IA,
                int         *C,
                long  IC,
                 size_t  N);

void bDSP_maxmgv(
                const float *A,
                long  IA,
                float       *C,
                 size_t  N);

void bDSP_vsaddi(
                const int   *A,
                long  IA,
                const int   *B,
                int         *C,
                long  IC,
                 size_t  N);

void bDSP_meanv(
                const float *A,
                long  IA,
                float       *C,
                size_t  N);

void bDSP_measqv(
                const float *A,
                long  IA,
                float       *C,
                 size_t  N);

void bDSP_svesq(
                const float *A,
                long  IA,
                float       *C,
                size_t  N);

void bDSP_maxmgvD(
                const double *A,
                long   IA,
                double       *C,
                  size_t   N);

void bDSP_vdivD(
                const double *B, // Caution:  A and B are swapped!
                long   IB,
                const double *A, // Caution:  A and B are swapped!
                long   IA,
                double       *C,
                long   IC,
                size_t   N);

void bDSP_vsdivD(
                const double *A,
                long   IA,
                const double *B,
                double       *C,
                long   IC,
                 size_t   N);
void bDSP_vsmulD(
                const double *A,
                long   IA,
                const double *B,
                double       *C,
                long   IC,
                 size_t   N);

void bDSP_vflt32D(
                const int   *A,
                long  IA,
                double      *C,
                long  IC,
                  size_t  N);

void bDSP_vsmsaD(
                const double *A,
                long   IA,
                const double *B,
                const double *C,
                double       *D,
                long   ID,
                 size_t   N);
void bDSP_vsaddD(
                const double *A,
                long   IA,
                const double *B,
                double       *C,
                long   IC,
                 size_t   N);

void bDSP_vdpsp(
                const double *A,
                long   IA,
                float        *C,
                long   IC,
                size_t   N);

void bDSP_vmin(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
               size_t  N);

void bDSP_vmaxmg(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                 size_t  N);

void bDSP_vminmg(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                 size_t  N);

void bDSP_vsmsma(
                const float *A,
                long  IA,
                const float *B,
                const float *C,
                long  IC,
                const float *D,
                float       *E,
                long  IE,
                 size_t  N);

void bDSP_sve(const float *A,
              long  Idx,
              float  *C,
              size_t  N);

void bDSP_vsmsa(
                const float *__A,
                long  __IA,
                const float *__B,
                const float *__C,
                float       *__D,
                long  __ID,
                size_t  __N);

void bDSP_vdist(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                size_t  N);

void bDSP_hamm_window(
                    float       *C,
                    size_t  N,
                      int          Flag);

void bDSP_desamp(
                const float *A,   // Input signal.
                long  DF,  // Decimation Factor.
                const float *F,   // Filter.
                float       *C,   // Output.
                size_t  N,   // Output length.
                 size_t  P);
void bDSP_svdiv(
                const float *A,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                size_t  N);

void bDSP_vsq(
                const float *A,
                long  IA,
                float       *C,
                long  IC,
              size_t  N);

void bDSP_vrsum(
                const float *A,
                long  IA,
                const float *S,
                float       *C,
                long  IC,
                size_t  N);

void bDSP_vflt16(
                const short *A,
                long  IA,
                float       *C,
                long  IC,
                 size_t  N);
void bDSP_minv(
                const float *A,
                long  IA,
                float       *C,
               size_t  N);

void bDSP_vgen(
                const float *__A,
                const float *__B,
                float       *__C,
                long  __IC,
               size_t  __N);

void bDSP_vqint(
                const float *A,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                size_t  N,
                size_t  M);

void bDSP_maxmgvi(
                const float *A,
                long  IA,
                float       *C,
                size_t *  Idx,
                  size_t  N);

void bDSP_vfltu32(
                  const unsigned int *A,
                  long         IA,
                  float              *C,
                  long         IC,
                  size_t         N);


void bDSP_vfixru8(
                const float   *A,
                long    IA,
                unsigned char *C,
                long    IC,
                  size_t    N);
    


void bDSP_vrvrs(
                float       *C,
                long  IC,
                size_t  N);

void bDSP_mtrans(
                const float *A,
                long  IA,
                float       *C,
                long  IC,
                size_t  M,
                 size_t  N);
       

void bDSP_blkman_window(
                        float       *C,
                        size_t  N,
                        int          Flag);
void bDSP_hann_window(
                    float       *C,
                    size_t  N,
                      int          Flag);

void bDSP_svemg(
               const float *A,
               long  IA,
               float       *C,
                size_t  N);



//MARK: Ramp

void bDSP_vrampmul2(const float *I0,
                    const float *I1,
                    long IS,
                    float *Start,
                    const float *Step,
                    float *O0,
                    float *O1,
                    long OS,
                    size_t N);
void bDSP_vrampmul(const float *Idx,
                   long IS,
                    float *Start,
                    const float *Step,
                    float *O,
                   long OS,
                   size_t N);

void bDSP_vneg(
        const float *A,
        size_t  IA,
        float       *C,
        size_t  IC,
        size_t  N);

//MARK: Vector
void bvtanf (float * y, const float * x, const int * n);

void bvrecf (float * y, const float * x, const int * n);

void bvexp2f (float * y, const float * x, const int * n);

void bvfloorf (float * y, const float * x, const int * n);

void bvlog2f (float * y, const float * x, const int * n);

void bvlog2 (double * y, const double * x, const int * n);

void bvpowsf (float * z , const float *  y, const float * x, const int * n);

void bvfabsf (float *  y, const float * x, const int * n);

void bvcopysignf (float * z, const float * y, const float * x, const int * n);

void bvsinpif (float * y, const float * x, const int * n);

void bvtanhf (float * y, const float * x, const int * n);
void bvfmodf (float * z, const float * y, const float * x, const int * n);
void bvexpf (float * y, const float * x, const int * n);
void bvceilf (float * y, const float *x, const int *n);

typedef struct BSPSplitComplex {
    float *  realp;
    float *  imagp;
} BSPSplitComplex;

typedef struct BSPComplex {
    float  real;
    float  imag;
} BSPComplex;

typedef struct BSPDoubleComplex {
    double  real;
    double  imag;
} BSPDoubleComplex;



void bDSP_ctoz(
               const BSPComplex      * C,
                long            IC,
               const BSPSplitComplex * Z,
                long            IZ,
               size_t            N);

void bDSP_ztoc(
               const BSPSplitComplex * Z,
                long            IZ,
               BSPComplex            * C,
                long            IC,
               size_t            N);

void bDSP_zrvmul(
                 const BSPSplitComplex * A,
                long            IA,
                 const float           * B,
                long            IB,
                 const BSPSplitComplex * C,
                long            IC,
                 size_t           N);
void bDSP_zvabs(
                const BSPSplitComplex * A,
                long            IA,
                float                 * C,
                long            IC,
           size_t            N);

void bDSP_itoc(
               const float * A,
                long            IA,
               BSPSplitComplex      * C,
                long            IC,
               size_t            N);
void bDSP_ctoi(
               const BSPSplitComplex * A,
                long            IA,
               float    * C,
                long            IC,
               size_t            N);

#ifdef __cplusplus
}
#endif

#endif //THERAPYFILTER_BMANDROIDVDSP_H
