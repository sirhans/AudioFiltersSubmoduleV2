//
// Created by hans anderson on 7/4/23.
//

    
#include "BMVDSP.h"
#include <math.h>
#include "BMOSSystem.h"
#include "../AudioFilter.h"
    
void bDSP_vramp(const float *initialValue, const float *increment, float *output, size_t stride,
                size_t numSamples) {
    float v = *initialValue;

    size_t idx = 0;
    for(size_t c=0; c < numSamples; c++) {
        output[idx] = *initialValue + *increment * c;
        idx += stride;
    }
}

void bDSP_vsmul(const float *V,
                size_t strideV,
                const float *S,
                float *output,
                size_t strideOutput,
                size_t numSamples){
    size_t outputI = 0;
    size_t VI = 0;

    for (size_t c=0; c<numSamples; c++){
        output[outputI] = V[VI] * *S;
        outputI += strideOutput;
        VI += strideV;
    }
}

//void bDSP_vsmul(const float * _Nonnull A, size_t IA, const float* _Nonnull  B,  float * _Nonnull C, size_t IC, size_t N){
//    // only accept strides of 1
//    assert(IA == 1);
//    assert(IC == 1);
//
//    // copy the scalar B into a vector of 4 32 bit floats
//    const float32x4_t B4 = vdupq_n_f32(*B);
//
//    // multiply in groups of 4
//    size_t endSIMD = N - (N % 4);
//    size_t i=0;
//    for(; i < endSIMD; i += 4){
//        // load the next 4 floats from A
//        float32x4_t A4i = vld1q_f32(A + i);
//
//        // multiply them by B
//        float32x4_t C4i = vmulq_f32(A4i,B4);
//
//        // write out to C
//        vst1q_f32(C + i, C4i);
//    }
//
//    // if there are some elements left back because N isn't divisible by 4, finish them up
//    for(; i < N; i++)
//        C[i] = A[i] * *B;
//}

void bDSP_vmul(const float *A,
               size_t strideA,
               const float *B,
               size_t strideB,
               float *C,
               size_t strideC,
               size_t numSamples){
    size_t ai, bi, ci;
    ai = bi = ci = 0;

    for(size_t k=0; k<numSamples; k++){
        C[ci] = A[ai] * B[bi];
        ci += strideC;
        bi += strideB;
        ai += strideA;
    }
}



void bDSP_vadd(const float *A,
               size_t strideA,
               const float *B,
               size_t strideB,
               float *C,
               size_t strideC,
               size_t numSamples){
    size_t ai, bi, ci;
    ai = bi = ci = 0;

    for(size_t k=0; k<numSamples; k++){
        C[ci] = A[ai] + B[bi];
        ci += strideC;
        bi += strideB;
        ai += strideA;
    }
}



void bDSP_vfill(const float *A,
                float *C,
                size_t strideC,
                size_t numSamples){

    size_t ci = 0;

    for(size_t k=0; k<numSamples; k++){
        C[ci] = *A;
        ci += strideC;
    }
}

void bDSP_vsma(const float *A,
               size_t  IA,
               const float *B,
               const float *C,
               size_t  IC,
               float       *D,
               size_t  ID,
               size_t  N){
    size_t ai = 0;
    size_t ci = 0;
    size_t di = 0;
    for(size_t n=0;n<N;n++){
        D[di] = A[ai] * B[0] + C[ci];
        ai += IA;
        ci += IC;
        di += ID;
    }
}

void bDSP_vgathr(const float       *A,
                 const size_t *B,
                 size_t        IB,
                 float             *C,
                 size_t        IC,
                 size_t        N){
    size_t ib = 0;
    size_t ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[B[ib] - 1];
        ib += IB;
        ic += IC;
    }
}

void bDSP_dotpr(const float *A,
                size_t  IA,
                const float *B,
                size_t  IB,
                float       *C,
                size_t  N){
    size_t ia = 0;
    size_t ib = 0;
    C[0] = 0;
    for(size_t n = 0; n < N; n++){
        C[0] += A[ia] * B[ib];
        ia += IA;
        ib += IB;
    }
    
    /*  Maps:  The default maps are used.

        These compute:

            C[0] = sum(A[n] * B[n], 0 <= n < N);
    */
}

void bDSP_vclr(
               float   *C,
               size_t  IC,
               size_t  N){
    for(size_t n=0 ; n<N; n++){
        C[n] = 0;
    }
}

void bDSP_vsadd(
                const float *A,
                size_t  IA,
                const float *B,
                float       *C,
                size_t  IC,
                size_t  N){
    size_t ia = 0;
    size_t ic = 0;
    for(size_t n = 0; n<N ; n++){
        C[ic] = A[ia] + B[0];
        ia += IA;
        ic += IC;
    }
}

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
            size_t  N){
    size_t ia = 0;
    size_t ib = 0;
    size_t ic = 0;
    size_t id = 0;
    size_t ie = 0;
    for(size_t n = 0; n < N; n++){
        E[ie] = A[ia] * B[ib] + C[ic] * D[id];
        ia += IA;
        ib += IB;
        ic += IC;
        id += ID;
        ie += IE;
    }
}

void bDSP_vma(
            const float *A,
            size_t  IA,
            const float *B,
            size_t  IB,
            const float *C,
            size_t  IC,
            float       *D,
            size_t  ID,
            size_t  N){
    size_t ia = 0;
    size_t ib = 0;
    size_t ic = 0;
    size_t id = 0;
    for (size_t n = 0; n < N; n++){
        D[id] = A[ia] * B[ib] + C[ic];
        ia += IA;
        ib += IB;
        ic += IC;
        id += ID;
    }
}

void bDSP_vthres(
                const float *A,
                size_t  IA,
                const float *B,
                float       *C,
                 size_t  IC,
                 size_t  N){
    size_t ia = 0;
    size_t ic = 0;
    for (size_t n = 0; n < N; n++){
        if (B[0] <= A[ia])
            C[ic] = A[ia];
        else
            C[ic] = 0;
        ia += IA;
        ic += IC;
    }
}

void bDSP_vthr(
                const float *A,
                size_t  IA,
                const float *B,
                float       *C,
               size_t  IC,
               size_t  N){
    size_t ia = 0;
    size_t ic = 0;
    for (size_t n = 0; n < N; n++){
        if (B[0] <= A[ia])
            C[ic] = A[ia];
        else
            C[ic] = B[0];
        ia += IA;
        ic += IC;
    }
}

void bDSP_maxv(
                const float *A,
                size_t  IA,
                float       *C,
               size_t  N){
    size_t ia = 0;
    float maxV = FLT_MIN;
    for (size_t n=0; n<N; n++) {
        if(maxV < A[ia]){
            maxV = A[ia];   
        }
        ia += IA;
    }
    C[0] = maxV;
}

void bDSP_vabs(
            const float *A,
            size_t  IA,
            float       *C,
            size_t  IC,
            size_t  N){
    size_t ia = 0;
    size_t ic = 0;
    for (size_t n = 0; n < N; n++){
        C[ic] = fabsf(A[ia]);
        ia += IA;
        ic += IC;
    }
}

void bDSP_vdbcon(
                const float *A,
                size_t  IA,
                const float *B,
                float       *C,
                size_t  IC,
                size_t  N,
                 size_t F){
    size_t ia = 0;
    size_t ic = 0;
    
    float alpha = F == 0 ? 10 : 20;
    for (size_t n = 0; n < N; n++){
        C[ic] = alpha * log10(A[ia] / B[0]);
        ia += IA;
        ic += IC;
    }
}

void bDSP_conv(
                const float *A,  // Input signal.
                size_t  IA,
                const float *F,  // Filter.
               long  IF,
               float       *C,  // Output signal.
               size_t  IC,
               size_t  N,  // Output length.
               size_t  P){
//    Commonly, this is called correlation if IF is positive and convolution
//    if IF is negative.
    /*
     C[n] = sum(A[n+p] * F[p], 0 <= p < P);
     */
    
    size_t ia = 0;
    size_t ic = 0;
    float sum;
    for (size_t n = 0; n < N; n++){
        sum = 0;
        for(size_t p =0 ; p <P;p+= IF){
            sum += A[ia + p] * F[p];
        }
        C[ic] = sum;
        ia += IA;
        ic += IC;
    }
}

void bDSP_vclip(
                const float *A,
                size_t  IA,
                const float *B,
                const float *C,
                float       *D,
                size_t  ID,
                size_t  N){
    size_t ia = 0;
    size_t id = 0;
    for (size_t n = 0; n < N; n++)
    {
        D[id] = A[ia];
        if (D[id] < B[0]) D[id] = B[0];
        if (C[0] < D[id]) D[id] = C[0];
        
        ia += IA;
        id += ID;
    }
}

void bDSP_vlint(
                const float *A,
                const float *B,
                size_t  IB,
                float       *C,
                size_t  IC,
                size_t  N,
                size_t  M){
    size_t ib = 0;
    size_t ic = 0;
    for (size_t n = 0; n < N; n++)
    {
        size_t b = truncf(B[ib]);
        float a = B[ib] - b;
        C[ic] = A[b] + a * (A[b+1] - A[b]);
        ib += IB;
        ic += IC;
    }
}

void bDSP_vpoly(
                const float *A,
                size_t  IA,
                const float *B,
                size_t  IB,
                float       *C,
                size_t  IC,
                size_t  N,
                size_t  P){
    /*for (n = 0; n < N; ++n)
        C[n] = sum(A[P-p] * B[n]**p, 0 <= p <= P);
     */
//    size_t ia = 0;
    size_t ib = 0;
    size_t ic = 0;
    float sum;
    for (size_t n = 0; n < N; n++){
        sum = 0;
        for(size_t p =0; p<=P;p++){
            sum += A[P - p] * powf(B[ib],p);
        }
        C[ic] = sum;
        ib += IB;
        ic += IC;
    }
}

void bDSP_vneg(
                const float *A,
                size_t  IA,
                float       *C,
                size_t  IC,
                size_t  N){
    size_t ia = 0;
    size_t ic = 0;
    for (size_t n = 0; n < N; n++){
        C[ic] = -A[ia];
        ia += IA;
        ic += IC;
    }
}

void bDSP_vmax(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
               size_t  N){
    long ia = 0;
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = B[ib] <= A[ia] ? A[ia] : B[ib];
        ia += IA;
        ib += IB;
        ic += IC;
    }
    
}

void bDSP_vsbsm(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                const float *C,
                float       *D,
                long  ID,
                size_t  N){
    long ia = 0;
    long ib = 0;
    long id = 0;
    for(size_t n = 0; n<N; n++){
        D[id] = (A[ia] - B[ib]) * C[0];
        ia += IA;
        ib += IB;
        id += ID;
    }
    
}

void bDSP_vasm(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                const float *C,
                float       *D,
                long  ID,
               size_t  N){
    long ia = 0;
    long ib = 0;
    long id = 0;
    for(size_t n = 0; n<N; n++){
        D[id] = (A[ia] + B[ib]) * C[0];
        ia += IA;
        ib += IB;
        id += ID;
    }
}

void bDSP_vswsum(
                const float *A,
                long  IA,
                float       *C,
                long  IC,
                size_t  N,
                 size_t  P){
    /*  Maps:  The default maps are used.
        
        These compute:

            for (n = 0; n < N; ++n)
                C[n] = sum(A[n+p], 0 <= p < P);

        Note that A must contain N+P-1 elements.
    */
    
    long ia = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        float sum = 0;
        for(size_t p = 0;p<P;p++){
            sum += A[ia+p];
        }
        C[ic] = sum;
        ia += IA;
        ic += IC;
    }
}

void bDSP_vmsa(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                const float *C,
                float       *D,
                long  ID,
               size_t  N){
    long ia = 0;
    long ib = 0;
    long id = 0;
    for(size_t n = 0; n<N; n++){
        D[id] = A[ia]*B[ib] + C[0];
        ia += IA;
        ib += IB;
        id += ID;
    }
}

void bDSP_vdiv(
                const float *B,  // Caution:  A and B are swapped!
                long  IB,
                const float *A,  // Caution:  A and B are swapped!
                long  IA,
                float       *C,
                long  IC,
               size_t  N){
    long ia = 0;
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia] / B[ib];
        ia += IA;
        ib += IB;
        ic += IC;
    }
}

void bDSP_vsub(
            const float *B,  // Caution:  A and B are swapped!
            long  IB,
            const float *A,  // Caution:  A and B are swapped!
            long  IA,
            float       *C,
            long  IC,
               size_t  N){
    long ia = 0;
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia] - B[ib];
        ia += IA;
        ib += IB;
        ic += IC;
    }
}

void bDSP_vfix32(
                const float *A,
                long  IA,
                int         *C,
                long  IC,
                 size_t  N){
    long ia = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = trunc(A[ia]);
        ia += IA;
        ic += IC;
    }
}
    
void bDSP_maxmgv(
                const float *A,
                long  IA,
                float       *C,
                 size_t  N){
    long ia = 0;
    float greatest = 0;
    for(size_t n = 0; n<N; n++){
        float absA = fabsf(A[ia]);
        if(greatest < absA){
            greatest = absA;
        }
        ia += IA;
    }
    C[0] = greatest;
}

void bDSP_vsaddi(
                const int   *A,
                long  IA,
                const int   *B,
                int         *C,
                long  IC,
                 size_t  N){
    long ia = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia] + B[0];
        ia += IA;
        ic += IC;
    }
}

void bDSP_meanv(
                const float *A,
                long  IA,
                float       *C,
                size_t  N){
   
    long ia = 0;
    float sumA = 0;
    for(size_t n = 0; n<N; n++){
        sumA += A[ia];
        ia += IA;
    }
    C[0] = sumA / N;
}

void bDSP_measqv(
                const float *A,
                long  IA,
                float       *C,
                 size_t  N){
    /*  Maps:  The default maps are used.

        These compute:

            C[0] = sum(A[n]**2, 0 <= n < N) / N;
    */
    long ia = 0;
    float sumSqA = 0;
    for(size_t n = 0; n<N; n++){
        sumSqA += powf(A[ia], 2);
        ia += IA;
    }
    C[0] = sumSqA / N;
}

void bDSP_svesq(
                const float *A,
                long  IA,
                float       *C,
                size_t  N){
    /*  Maps:  The default maps are used.

        These compute:

            C[0] = sum(A[n] ** 2, 0 <= n < N);
    */
    long ia = 0;
    float sumSqA = 0;
    for(size_t n = 0; n<N; n++){
        sumSqA += powf(A[ia], 2);
        ia += IA;
    }
    C[0] = sumSqA;
}

void bDSP_vdivD(
                const double *B, // Caution:  A and B are swapped!
                long   IB,
                const double *A, // Caution:  A and B are swapped!
                long   IA,
                double       *C,
                long   IC,
                size_t   N){
    long ia = 0;
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia] / B[ib];
        ia += IA;
        ib += IB;
        ic += IC;
    }
}

void bDSP_vsdivD(
                const double *A,
                long   IA,
                const double *B,
                double       *C,
                long   IC,
                 size_t   N){
    long ia = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia] / B[0];
        ia += IA;
        ic += IC;
    }
}

void bDSP_vsmulD(
                const double *A,
                long   IA,
                const double *B,
                double       *C,
                long   IC,
                 size_t   N){
    long ia = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia] * B[0];
        ia += IA;
        ic += IC;
    }
}

    

void bDSP_maxmgvD(
                const double *A,
                long   IA,
                double       *C,
                  size_t   N){
    long ia = 0;
    double greatest = 0;
    for(size_t n = 0; n<N; n++){
        double absA = fabs(A[ia]);
        if(greatest < absA){
            greatest = absA;
        }
        ia += IA;
    }
    C[0] = greatest;
}

void bDSP_vflt32D(
                const int   *A,
                long  IA,
                double      *C,
                long  IC,
                  size_t  N){
    long ia = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia];
        ia += IA;
        ic += IC;
    }
}

void bDSP_vsmsaD(
                const double *A,
                long   IA,
                const double *B,
                const double *C,
                double       *D,
                long   ID,
                 size_t   N){
    long ia = 0;
    long id = 0;
    for(size_t n = 0; n<N; n++){
        D[id] = A[ia]*B[0] + C[0];
        ia += IA;
        id += ID;
    }
}

void bDSP_vsaddD(
                const double *A,
                long   IA,
                const double *B,
                double       *C,
                long   IC,
                 size_t   N){
    long ia = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia] + B[0];
        ia += IA;
        ic += IC;
    }
}

void bDSP_vdpsp(
                const double *A,
                long   IA,
                float        *C,
                long   IC,
                size_t   N){
    /*  Maps:  The default maps are used.

        These compute:

            for (n = 0; n < N; ++n)
                C[n] = A[n];
    */
    long ia = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia];
        ia += IA;
        ic += IC;
    }
}

void bDSP_vmin(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
               size_t  N){
    long ia = 0;
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia] <= B[ib] ? A[ia] : B[ib];
        ia += IA;
        ib += IB;
        ic += IC;
    }
}

void bDSP_vmaxmg(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                 size_t  N){
    long ia = 0;
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        float absA = fabsf(A[ia]);
        float absB = fabsf(B[ib]);
        C[ic] = absB <= absA ? absA : absB;
        ia += IA;
        ib += IB;
        ic += IC;
    }
}

void bDSP_vminmg(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                 size_t  N){
    long ia = 0;
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        float absA = fabsf(A[ia]);
        float absB = fabsf(B[ib]);
        C[ic] = absA <= absB ? absA : absB;
        ia += IA;
        ib += IB;
        ic += IC;
    }
}

void bDSP_vsmsma(
                const float *A,
                long  IA,
                const float *B,
                const float *C,
                long  IC,
                const float *D,
                float       *E,
                long  IE,
                 size_t  N){
    long ia = 0;
    long ic = 0;
    long ie = 0;
    for(size_t n = 0; n<N; n++){
        E[ie] = A[ia]*B[0] + C[ic]*D[0];
        ia += IA;
        ic += IC;
        ie += IE;
    }
}

void bDSP_sve(
            const float *A,
            long  IA,
            float       *C,
              size_t  N){
    /*  Maps:  The default maps are used.

        These compute:

            C[0] = sum(A[n], 0 <= n < N);
    */
    long ia = 0;
    float sumA = 0;
    for(size_t n = 0; n<N; n++){
        sumA += A[ia];
        ia += IA;
    }
    C[0] = sumA;
}
     
void bDSP_vsmsa(
                const float *A,
                long  IA,
                const float *B,
                const float *C,
                float       *D,
                long  ID,
                size_t  N){
    long ia = 0;
    long id = 0;
    for(size_t n = 0; n<N; n++){
        D[id] = A[ia]*B[0] + C[0];
        ia += IA;
        id += ID;
    }
}

void bDSP_vdist(
                const float *A,
                long  IA,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                size_t  N){
    long ia = 0;
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = sqrt(powf(A[ia], 2) + powf(B[ib], 2));
        ia += IA;
        ib += IB;
        ic += IC;
    }
}

void bDSP_hamm_window(
                    float       *C,
                    size_t  N,
                      int          Flag){
    /*  Maps:

            No strides are used; the array maps directly to memory.

        These compute:

            If Flag & vDSP_HALF_WINDOW:
                Length = (N+1)/2;
            Else
                Length = N;

            for (n = 0; n < Length; ++n)
                C[n] = .54 - .46 * cos(2*pi*n/N);
    */
    
    for(size_t n = 0; n<N; n++){
        C[n] = 0.54 - (0.46 * cos( 2 * M_PI * n / N ) );
    }
}

void bDSP_desamp(
                const float *A,   // Input signal.
                long  DF,  // Decimation Factor.
                const float *F,   // Filter.
                float       *C,   // Output.
                size_t  N,   // Output length.
                 size_t  P){
    /*  Maps:

            No strides are used; arrays map directly to memory.  DF specifies
            the decimation factor.

        These compute:

            for (n = 0; n < N; ++n)
                C[n] = sum(A[n*DF+p] * F[p], 0 <= p < P);
    */
    float sumA;
    for(size_t n = 0; n<N; n++){
        sumA = 0;
        for(size_t p = 0;p<P;p++){
            sumA += A[n*DF + p] * F[p];
        }
        C[n] = sumA;
    }
}

void bDSP_svdiv(
                const float *A,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                size_t  N){
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[0] / B[ib];
        ib += IB;
        ic += IC;
    }
}

void bDSP_vsq(
                const float *A,
                long  IA,
                float       *C,
                long  IC,
              size_t  N){
    /*  Maps:  The default maps are used.

        These compute:

            for (n = 0; n < N; ++n)
                C[n] = A[n]**2;
    */
    long ia = 0;
    long ic = 0;
    //Pointer = C; pointer ++
    //C[i] = C + i
    for(size_t n = 0; n<N; n++){
        C[ic] = powf(A[ia], 2);
        ia += IA;
        ic += IC;
    }
}

void bDSP_vrsum(
                const float *A,
                long  IA,
                const float *S,
                float       *C,
                long  IC,
                size_t  N){
    /*  Maps:  The default maps are used.
        
        These compute:

            for (n = 0; n < N; ++n)
                C[n] = S[0] * sum(A[j], 0 < j <= n);

        Observe that C[0] is set to 0, and A[0] is not used.
    */
    long ic = 0;
    float sumA;
    for(size_t n = 0; n<N; n++){
        sumA = 0;
        for(int j = 1;j <= n ; j+= IA){
            sumA += A[j];
        }
        C[ic] = S[0] * sumA;
        ic += IC;
    }
}

void bDSP_vflt16(
                const short *A,
                long  IA,
                float       *C,
                long  IC,
                 size_t  N){
    long ia = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[ia];
        ia += IA;
        ic += IC;
    }
}

void bDSP_vgen(
                const float *A,
                const float *B,
                float       *C,
                long  IC,
               size_t  N){
    /*  Maps:  The default maps are used.

        These compute:

            for (n = 0; n < N; ++n)
                C[n] = A[0] + (B[0] - A[0]) * n/(N-1);
    */
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        C[ic] = A[0] + (B[0] - A[0]) * n/(N-1);
        ic += IC;
    }
}

void bDSP_vqint(
                const float *A,
                const float *B,
                long  IB,
                float       *C,
                long  IC,
                size_t  N,
                size_t  M){
    /*  Maps:  The default maps are used.
        
        These compute:

            for (n = 0; n < N; ++n)
            {
                b = max(trunc(B[n]), 1);
                a = B[n] - b;
                C[n] = (A[b-1]*(a**2-a) + A[b]*(2-2*a**2) + A[b+1]*(a**2+a))
                    / 2;
            }
    */
    long ib = 0;
    long ic = 0;
    for(size_t n = 0; n<N; n++){
        long b = BM_MAX(truncf(B[ib]), 1);
        float a = B[ib] - b;
        size_t a2idx = BM_MIN(M-1,b+1);
        C[ic] = (A[b-1]*(powf(a, 2)-a) + A[b]*(2-2* powf(a, 2)) + A[a2idx]*(powf(a, 2)+a)) / 2;
        ib += IB;
        ic += IC;
    }
}
       


void bDSP_maxmgvi(
                const float *A,
                long  IA,
                float       *C,
                size_t *  Idx,
                  size_t  N){
    /*  Maps:  The default maps are used.

        C[0] is set to the greatest value of |A[n]| for 0 <= n < N.
        I[0] is set to the least i*IA such that |A[i]| has the value in C[0].
    */
    long ia = 0;
    C[0] = 0;
    Idx[0] = 0;
    for(size_t i=0; i < N; i++){
        float absA = fabsf(A[ia]);
        if(C[0] < absA){
            C[0] = absA;
            Idx[0] = i;
        }
        ia += IA;
    }
    
}

void bDSP_vfltu32(
                  const unsigned int *A,
                  long         IA,
                  float              *C,
                  long         IC,
                  size_t         N){
    
    long ia = 0;
    long ic = 0;
    for (size_t n = 0; n < N; ++n){
        C[ic] = A[ia];
        ia += IA;
        ic += IC;
    }
}


void bDSP_vfixru8(
                const float   *A,
                long    IA,
                unsigned char *C,
                long    IC,
                  size_t    N){
    long ia = 0;
    long ic = 0;
    for (size_t n = 0; n < N; ++n){
        C[ic] = rint(A[ia]);
        ia += IA;
        ic += IC;
    }
}
    
void bDSP_vswF(float* a, float* b)
{
    //Swap a / b
    float temp = *a;
    *a = *b;
    *b = temp;
}

void bDSP_vrvrs(
                float       *C,
                long  IC,
                size_t  N){
    /*  Maps:  The default maps are used.
        These compute:

            Let A contain a copy of C.
            for (n = 0; n < N; ++n)
                C[n] = A[N-1-n];
    */
    
    // pointer1 pointing at the beginning of the array
    float *pointer1 = C;
     
    // pointer2 pointing at end of the array
    float *pointer2 = C + N - 1;
    while (pointer1 < pointer2) {
        bDSP_vswF(pointer1, pointer2);
        pointer1++;
        pointer2--;
    }
}

void bDSP_mtrans(
                const float *A,
                long  IA,
                float       *C,
                long  IC,
                size_t  M,
                 size_t  N){
    /*  Maps:
            A is regarded as a two-dimensional matrix with dimemnsions
            [N][M] and stride IA.  C is regarded as a two-dimensional matrix
            with dimemnsions [M][N] and stride IC:

            Pseudocode:     Memory:
            A[n][m]         A[(n*M + m)*IA]
            C[m][n]         C[(m*N + n)*IC]

        These compute:

            for (m = 0; m < M; ++m)
            for (n = 0; n < N; ++n)
                C[m][n] = A[n][m];
    */
    
    for(size_t row = 0 ; row < M; row++){//Column
        for(size_t col = 0; col < N; col++){ //rows
            C[col * M + row] = A[row * N + col];
            
        }
    }
}
       

void bDSP_blkman_window(
                        float       *C,
                        size_t  N,
                        int          Flag){
    /*  Maps:

            No strides are used; the array maps directly to memory.

        These compute:

            If Flag & vDSP_HALF_WINDOW:
                Length = (N+1)/2;
            Else
                Length = N;

            for (n = 0; n < Length; ++n)
            {
                angle = 2*pi*n/N;
                C[n] = .42 - .5 * cos(angle) + .08 * cos(2*angle);
            }
    */
    for(size_t i = 0; i < N ; i++){
        float angle = 2 * M_PI * i/N;
        C[i] = .42 - .5 * cos(angle) + .08 * cos(2*angle);
    }
    
}

void bDSP_hann_window(
                    float       *C,
                    size_t  N,
                      int          Flag){
    /*  Maps:

            No strides are used; the array maps directly to memory.

        These compute:

            If Flag & vDSP_HALF_WINDOW:
                Length = (N+1)/2;
            Else
                Length = N;

            If Flag & vDSP_HANN_NORM:
                W = .816496580927726;
            Else
                W = .5;

            for (n = 0; n < Length; ++n)
                C[n] = W * (1 - cos(2*pi*n/N));
    */
    float W = 0.5;
    if(Flag){
        W = 0.816496580927726;
    }
    
    for(size_t i=0; i< N; i++){
        C[i] = W * (1 - cos(2*M_PI*i/N));
    }
    
}

void bDSP_svemg(
               const float *A,
               long  IA,
               float       *C,
                size_t  N){
    /*  Maps:  The default maps are used.

        These compute:

            C[0] = sum(|A[n]|, 0 <= n < N);
    */
    long ia = 0;
    float sumA = 0;
    for(size_t i = 0;i < N; i++){
        sumA += fabsf(A[ia]);
        ia += IA;
    }
    C[0] = sumA;
}



void bDSP_minv(
                const float *A,
                long  IA,
                float       *C,
               size_t  N){
    /*  Maps:  The default maps are used.

        C[0] is set to the least value of A[n] for 0 <= n < N.
    */
    long ia = 0;
    C[0] = FLT_MAX;
    for(size_t i= 0; i< N; i++){
        if(C[0] > A[ia]){
            C[0] = A[ia];
        }
        ia += IA;
    }
}

//MARK: DSPComplex
void bDSP_ctoz(
                const BSPComplex      *C,
                long            IC,
                const BSPSplitComplex *Z,
                long            IZ,
               size_t            N){
    /*  Map:

            Pseudocode:     Memory:
            C[n]            C[n*IC/2].real + i * C[n*IC/2].imag
            Z[n]            Z->realp[n*IZ] + i * Z->imagp[n*IZ]

        These compute:

            for (n = 0; n < N; ++n)
                Z[n] = C[n];
    */
    
    long ic = 0;
    long iz = 0;
    for (size_t n = 0; n < N; ++n){
        Z->realp[iz] = C[ic].real;
        Z->imagp[iz] = C[ic].imag;
        ic += IC;
        iz += IZ;
    }
}

void bDSP_ztoc(
                const BSPSplitComplex *Z,
                long            IZ,
                BSPComplex            *C,
                long            IC,
               size_t            N){
    /*  Map:

            Pseudocode:     Memory:
            Z[n]            Z->realp[n*IZ] + i * Z->imagp[n*IZ]
            C[n]            C[n*IC/2].real + i * C[n*IC/2].imag

        These compute:

            for (n = 0; n < N; ++n)
                C[n] = Z[n];
    */
    long ic = 0;
    long iz = 0;
    for (size_t n = 0; n < N; ++n){
        C[ic].real = Z->realp[iz];
        C[ic].imag = Z->imagp[iz];
        ic += IC;
        iz += IZ;
    }
}

void bDSP_zrvmul(
                const BSPSplitComplex *A,
                long            IA,
                const float           *B,
                long            IB,
                const BSPSplitComplex *C,
                long            IC,
                 size_t           N){
    /*  Maps:  The default maps are used.

        These compute:

            for (n = 0; n < N; ++n)
                C[n] = A[n] * B[n];
    */
    
    long ia = 0;
    long ib = 0;
    long ic = 0;
    for (size_t n = 0; n < N; ++n){
        C->realp[ic] = B[ib] * A->realp[ia];
        C->imagp[ic] = B[ib] * A->imagp[ia];
        ia += IA;
        ib += IB;
        ic += IC;
    }
    
}

void bDSP_zvabs(
                const BSPSplitComplex *A,
                long            IA,
                float                 *C,
                long            IC,
                size_t            N){
    /*  Maps:  The default maps are used.

        These compute:

            for (n = 0; n < N; ++n)
                C[n] = |A[n]|;
    */
    
    long ia = 0;
    long ic = 0;
    for(size_t i =0; i< N; i++){
        C[ic] = sqrtf(A->realp[ia] * A->realp[ia] + A->imagp[ia] * A->imagp[ia]);
        ia += IA;
        ic += IC;
    }
    
}

//interleave to complex
void bDSP_itoc(
                const float *A,
                long            IA,
               BSPSplitComplex      *C,
                long            IC,
                size_t            N){
    /*  Maps:  The default maps are used.

        These compute:

            for (n = 0; n < N; ++n)
                C[n] = |A[n]|;
    */
    
    long ia = 0;
    long ic = 0;
    for(size_t i =0; i< N; i++){
        C->realp[ic] = A[ia * 2];
        C->imagp[ic] = A[ia * 2 + 1];
        ia += IA;
        ic += IC;
    }
    
}

//complex to interleave
void bDSP_ctoi(
                const BSPSplitComplex *A,
                long            IA,
                float    *C,
                long            IC,
                size_t            N){
    /*  Maps:  The default maps are used.

        These compute:

            for (n = 0; n < N; ++n)
                C[n] = |A[n]|;
    */
    
    long ia = 0;
    long ic = 0;
    for(size_t i =0; i< N; i++){
        C[ic*2] = A->realp[ia];
        C[ic*2 + 1] = A->imagp[ia];
        ia += IA;
        ic += IC;
    }
    
}

//MARK: Rampmul

void bDSP_vrampmul(
                    const float *Id,
                   long IS,
                    float *Start,
                    const float *Step,
                    float *O,
                   long OS,
                   size_t N){
    long is = 0;
    long os = 0;
    for(size_t n = 0; n<N; n++){
        O[os] = *Start * Id[is];
        *Start += *Step;
        is += IS;
        os += OS;
    }
}

void bDSP_vrampmul2(
                    const float *I0,
                    const float *I1,
                    long IS,
                    float *Start,
                    const float *Step,
                    float *O0,
                    float *O1,
                    long OS,
                    size_t N){
    long is = 0;
    long os = 0;
    for(size_t n = 0; n<N; n++){
        O0[os] = *Start * I0[is];
        O1[os] = *Start * I1[is];
        *Start += *Step;
        is += IS;
        os += OS;
    }
}

//MARK: Vector
    
void bvtanf (float * y, const float * x, const int * n) {
    for(int i=0;i< *n;i++){
        y[i] = tanf(x[i]);
    }
}

void bvrecf (float * y, const float * x, const int * n){
    for(int i=0;i< *n;i++){
        y[i] = 1.0f / x[i];
    }
}

void bvexpf (float * y, const float * x, const int * n){
    /* y (output) Output vector of size *n. y[i] is set to exp(x[i]).*/
    for(int i=0;i< *n;i++){
        y[i] = expf(x[i]);
    }
}

void bvexp2f (float * y, const float * x, const int * n){
    for(int i=0;i< *n;i++){
        y[i] = exp2f(x[i]);
        
    }
}
    
void bvfloorf (float * y, const float * x, const int * n){
    for(int i=0;i< *n;i++){
        y[i] = floorf(x[i]);
    }
}

void bvlog2f (float * y, const float * x, const int * n){
    for(int i=0;i< *n;i++){
        y[i] = log2f(x[i]);
    }
}

void bvlog2 (double * y, const double * x, const int * n){
    for(int i=0;i< *n;i++){
        y[i] = log2(x[i]);
    }
}


void bvpowsf (float * z , const float *  y, const float * x, const int * n){
    /*  @param z (output) Output vector of size *n. z[i] is set to pow(x[i], y).*/
    for(int i=0;i< *n;i++){
        z[i] = powf(x[i], y[0]);
    }
}

void bvfabsf (float *  y, const float * x, const int * n){
    for(int i=0;i< *n;i++){
        y[i] = fabsf(x[i]);
    }
}

void bvcopysignf (float * z, const float * y, const float * x, const int * n){
    /*@param z (output) Output vector of size *n.
                        z[i] is set to copysign(y[i], x[i]).*/
    for(int i=0;i< *n;i++){
        z[i] = copysignf(y[i], x[i]);
    }
}

void bvsinpif (float * y, const float * x, const int * n){
    /* @param y (output) Output vector of size *n. y[i] is set to sin(x[i]*PI).*/
    for(int i=0;i< *n;i++){
        y[i] = sinf(x[i]* M_PI);
    }
}

void bvtanhf (float * y, const float * x, const int * n){
    /**  @discussion
     *  If x[i] is +/-0, y[i] preserves the signed zero.
     *  If x[i] is +/-inf, y[i] is set to +/-1.
     *
     *  y (output) Output vector of size *n. y[i] is set to tanh(x[i]). */
    for(int i=0;i< *n;i++){
        y[i] = tanhf(x[i]);
    }
}

void bvfmodf (float * z, const float * y, const float * x, const int * n){
    /* z (output) Output vector of size *n. z[i] is set to fmod(y[i], x[i]). */
    for(int i=0;i< *n;i++){
        z[i] = fmodf(y[i], x[i]);
    }
}

void bvceilf (float * y, const float *x, const int *n){
    for(int i=0;i< *n; i++){
        y[i] = ceilf(x[i]);
    }
}

//MARK: Custom function
/* Array compare*/
bool bDSP_vacD(double* array1, double* array2, size_t length){
    for(int i=0;i< length; i++){
        if(array1[i] != array2[i])
            return false;
    }
    return true;
}

        
extern vDSP_biquadm_Setup vDSP_biquadm_CreateSetup(const double * _Nonnull coeffs,
                                                              vDSP_Length numLevels,
                                                              vDSP_Length numChannels){
    vDSP_biquadm_Setup This = malloc(sizeof(vDSP_biquadm_SetupStruct));
    
    This->numLevels = numLevels;
    This->numChannels = numChannels;
    
    This->coefficients = malloc(sizeof(double)*numLevels*numChannels*5);
    This->targets = malloc(sizeof(double)*numLevels*numChannels*5);
    This->delays = malloc(sizeof(double)*numLevels*numChannels*4);
    This->activeLevels = malloc(sizeof(bool)*numLevels);
    
    // set all the filters active
    for(size_t i=0; i<numLevels; i++) This->activeLevels[i] = true;
    
    // set the delays to zero
    for(size_t i=0; i<numLevels*numChannels*4; i++)
        This->delays[i] = 0.0;
    
    // copy the filter coefficients
    memcpy(This->coefficients, coeffs, sizeof(double)*numLevels*numChannels*5);
    
    return This;
}


extern void vDSP_biquadm_DestroySetup(vDSP_biquadm_Setup _Nonnull __setup){
    free(__setup->activeLevels);
    free(__setup->delays);
    free(__setup->targets);
    free(__setup->coefficients);
    free(__setup);
}


extern void vDSP_biquadm_ResetState(vDSP_biquadm_Setup _Nonnull __setup){
    // set the delays to zero
    for(size_t i=0; i<__setup->numLevels*__setup->numChannels*4; i++)
        __setup->delays[i] = 0.0;
}


extern void vDSP_biquadm_SetCoefficientsDouble(vDSP_biquadm_Setup _Nonnull __setup,
                                               const double * _Nonnull __coeffs,
                                               vDSP_Length __start_sec,
                                               vDSP_Length __start_chn,
                                               vDSP_Length __nsec,
                                               vDSP_Length __nchn){
    // we force the user to update all the coefficients rather than just one
    // filter at a time. We can change this later if necessary.
    assert(__nchn == __setup->numChannels);
    assert(__nsec == __setup->numLevels);
    assert(__start_chn == 0);
    assert(__start_sec == 0);
    
    // copy all the coefficients
    memcpy(__setup->coefficients, __coeffs, sizeof(double)*__nsec*__nchn*5);
}



extern void vDSP_biquadm_SetTargetsDouble(vDSP_biquadm_Setup _Nonnull __setup,
                                          const double * _Nonnull __targets,
                                          float __interp_rate,
                                          float __interp_threshold,
                                          vDSP_Length __start_sec,
                                          vDSP_Length __start_chn,
                                          vDSP_Length __nsec,
                                          vDSP_Length __nchn){
    // we're not done implementing this yet. please don't call it.
    assert(false);
    
    // we force the user to update all the coefficients rather than just one
    // filter at a time. We can change this later if necessary.
    assert(__nchn == __setup->numChannels);
    assert(__nsec == __setup->numLevels);
    assert(__start_chn == 0);
    assert(__start_sec == 0);
    
    // copy all the coefficient targets
    memcpy(__setup->targets, __targets, sizeof(double)*__nsec*__nchn*5);
}


/*
    vDSP_biquadm_SetActiveFilters will set the overall active/inactive filter
    state of a valid vDSP_biquadm_Setup object.
 */
extern void vDSP_biquadm_SetActiveFilters(vDSP_biquadm_Setup _Nonnull __setup,
                                          const bool * _Nonnull __filter_states){
    memcpy(__setup->activeLevels, __filter_states, sizeof(bool)*__setup->numLevels);
}



void biquadSingleChannel(const float *input, float *output, double *coefficients, double *delays, size_t numSamples){
    // we wrote this function using standard c but then re-wrote it using arm
    // neon intrinsics, leaving the standard c code in the comments.
    
    double b0 = coefficients[0];
//    double b1 = coefficients[1];
//    double b2 = coefficients[2];
//    double a1 = coefficients[3];
//    double a2 = coefficients[4];
    // arm neon SIMD version of the commented lines above
    float64x2_t b12 = {coefficients[1],coefficients[2]};
    float64x2_t a12 = {-coefficients[3],-coefficients[4]};
    
    // init delays
//    double zb1 = delays[0];
//    double zb2 = delays[1];
//    double za1 = delays[2];
//    double za2 = delays[3];
    // arm neon SIMD version of the commented lines above
    float64x2_t zb12 = {delays[0], delays[1]};
    float64x2_t za12 = {delays[2], delays[3]};

    // process the filter
    for (size_t n = 0; n < numSamples; n++){
        double zb0 = input[n];
        
//        za0 = + b0 * zb0
//              + b1 * zb1
//              + b2 * zb2
//              - a1 * za1
//              - a2 * za2;
        // arm neon SIMD version of the commented lines above
        float64x2_t acc = vdupq_n_f64(0.0); // acc = {0,0}
        acc = vfmaq_f64(acc, b12, zb12); // acc[0] += b12[0] * zb12[0]; acc[1] += b12[1] * zb12[1];
        acc = vfmaq_f64(acc, a12, za12); // acc[0] += a12[0] * ab12[0]; acc[1] += a12[1] * ab12[1];
        double za0 = vaddvq_f64(acc) + (zb0 * b0); // za0 = acc[0] + acc[1] + (zb0 * b0);
        
        output[n] = (float)za0;
        
//        zb2 = zb1;
//        zb1 = zb0;
//        za2 = za1;
//        za1 = za0;
        // arm neon SIMD version of the commented lines above
        zb12 = (float64x2_t){zb0, vgetq_lane_f64(zb12, 0)};
        za12 = (float64x2_t){za0, vgetq_lane_f64(za12, 0)};
    }
    
    // save delay data for next time
//    delays[0] = zb1;
//    delays[1] = zb2;
//    delays[2] = za1;
//    delays[3] = za2;
    // arm neon SIMD version of the commented lines above
    delays[0] = vgetq_lane_f64(zb12,0);
    delays[1] = vgetq_lane_f64(zb12,1);
    delays[2] = vgetq_lane_f64(za12,0);
    delays[3] = vgetq_lane_f64(za12,1);
}



/*  vDSP_biquadm applies a
    multi-channel biquadm IIR filter created with vDSP_biquadm_CreateSetup.
 */
extern void vDSP_biquadm(vDSP_biquadm_Setup _Nonnull       __Setup,
    const float * _Nonnull * _Nonnull __X, vDSP_Stride __IX,
    float       * _Nonnull * _Nonnull __Y, vDSP_Stride __IY,
                         vDSP_Length              __N){
    // force the strides to be 1
    assert(__IX == 1);
    assert(__IY == 1);
    
    // for each channel
    for(size_t cnl = 0; cnl < __Setup->numChannels; cnl++){
        const float *input  = __X[cnl];
        float *output = __Y[cnl];
        bool firstActiveLevel = true;
        
        // process each level of the filter
        for (size_t lvl=0; lvl < __Setup->numLevels; lvl++){
            // get simple pointers to the data we need to pass on to the biquad process function
            double *coefficients = __Setup->coefficients + lvl*__Setup->numChannels*5 + cnl*5;
            double *delays       = __Setup->delays       + lvl*__Setup->numChannels*4 + cnl*4;
            
            // if the filter on this level is active
            if(__Setup->activeLevels[lvl]){
                // if this is the first active level on this channel, process from input to output
                if (firstActiveLevel) {
                    biquadSingleChannel(input, output, coefficients, delays, __N);
                    firstActiveLevel = false;
                }
                // if this is not the first active level, process output in place
                else
                    biquadSingleChannel(output, output, coefficients, delays, __N);
            }
            
            // if none of the filters was active, copy input to output without filtering
            if (firstActiveLevel)
                memcpy(output, input, sizeof(float)*__N);
        }
    }
}




        
    
