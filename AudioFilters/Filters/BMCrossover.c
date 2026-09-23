//
//  BMCrossover.c
//  BMAudioFilters
//
//  Created by Hans on 17/2/17.
//
//  This file may be used, distributed and modified freely by anyone,
//  for any purpose, without restrictions.
//

#ifdef __cplusplus
extern "C" {
#endif
    
#include "../AudioFilter.h"
#include "BMCrossover.h"
#include <assert.h>
    
    /*!
     *BMCrossover_init
     * @abstract This function must be called prior to use
     *
     * @param cutoff      - cutoff frequency in hz
     * @param sampleRate  - sample rate in hz
     * @param fourthOrder - true: 4th order, false: 2nd order
     * @param stereo      - true: stereo, false: mono
     */
    void BMCrossover_init(BMCrossover *This,
                          float cutoff,
                          float sampleRate,
                          bool fourthOrder,
                          bool stereo){
        
        This->fourthOrder = fourthOrder;
        This->stereo      = stereo;
        
        // One state-variable filter section produces both the lowpass and the
        // highpass output of the first stage from the same state. The 4th
        // order crossover cascades a second Butterworth section on each band.
        BMMultiLevelSVF_init(&This->split, 1, sampleRate, stereo);
        if(fourthOrder){
            BMMultiLevelSVF_init(&This->lp2, 1, sampleRate, stereo);
            BMMultiLevelSVF_init(&This->hp2, 1, sampleRate, stereo);
        }
        
        // filters used only for plotting the transfer functions
        size_t numLevels = fourthOrder ? 2 : 1;
        BMMultiLevelBiquad_init(&This->plotFilters[0], numLevels, sampleRate, false, false, false);
        BMMultiLevelBiquad_init(&This->plotFilters[1], numLevels, sampleRate, false, false, false);
        
        BMCrossover_setCutoff(This, cutoff);
    }
    
    
    
    
    
    
    /*
     * Free memory used by the filters
     */
    void BMCrossover_free(BMCrossover *This){
        BMMultiLevelSVF_free(&This->split);
        if(This->fourthOrder){
            BMMultiLevelSVF_free(&This->lp2);
            BMMultiLevelSVF_free(&This->hp2);
        }
        for(size_t i=0; i<2; i++)
            BMMultiLevelBiquad_free(&This->plotFilters[i]);
    }
    
    
    
    
    
    
    /*
     * @param cutoff - cutoff frequency in Hz
     */
    void BMCrossover_setCutoff(BMCrossover *This, float cutoff){
        if(This->fourthOrder){
            // Linkwitz-Riley 4th order: two cascaded 2nd order Butterworth
            // sections (Q = 1/sqrt(2)) per band. The first section is shared
            // by the two bands; only its fc and Q matter for the split.
            BMMultiLevelSVF_setLowpass12dBwithQ(&This->split, cutoff, M_SQRT1_2, 0);
            BMMultiLevelSVF_setLowpass12dBwithQ(&This->lp2, cutoff, M_SQRT1_2, 0);
            BMMultiLevelSVF_setHighpass12dBwithQ(&This->hp2, cutoff, M_SQRT1_2, 0);
            
            BMMultiLevelBiquad_setLinkwitzRileyLP4thOrder(&This->plotFilters[0], cutoff, 0);
            BMMultiLevelBiquad_setLinkwitzRileyHP4thOrder(&This->plotFilters[1], cutoff, 0);
        } else {
            // Linkwitz-Riley 2nd order: one section with Q = 1/2. The
            // highpass output is inverted at process time (see
            // BMCrossover_highGain) so that lowpass + highpass is an allpass.
            BMMultiLevelSVF_setLinkwitzRileyLP(&This->split, cutoff, 0);
            
            BMMultiLevelBiquad_setLinkwitzRileyLP(&This->plotFilters[0], cutoff, 0);
            BMMultiLevelBiquad_setLinkwitzRileyHP(&This->plotFilters[1], cutoff, 0);
        }
    }
    
    
    
    
    /*
     * Sign of the highpass output. The 2nd order Linkwitz-Riley highpass is
     * inverted (as BMMultiLevelSVF_setLinkwitzRileyHP does) so that the sum
     * of the two bands is an allpass; the 4th order needs no inversion.
     */
    static inline float BMCrossover_highGain(const BMCrossover *This){
        return This->fourthOrder ? 1.0f : -1.0f;
    }
    
    
    
    
    // stereo crossover with an explicit sign for the highpass output
    static void BMCrossover_processStereoGain(BMCrossover *This,
                                              const float* inL, const float* inR,
                                              float* lowpassL, float* lowpassR,
                                              float* highpassL, float* highpassR,
                                              float highGain,
                                              size_t numSamples){
        // one filter, two outputs
        BMMultiLevelSVF_processBufferStereoSplit(&This->split,
                                                 inL, inR,
                                                 lowpassL, lowpassR,
                                                 highpassL, highpassR,
                                                 highGain,
                                                 numSamples);
        // second Butterworth section on each band
        if(This->fourthOrder){
            BMMultiLevelSVF_processBufferStereo(&This->lp2, lowpassL, lowpassR, lowpassL, lowpassR, numSamples);
            BMMultiLevelSVF_processBufferStereo(&This->hp2, highpassL, highpassR, highpassL, highpassR, numSamples);
        }
    }
    
    
    
    
    // mono crossover with an explicit sign for the highpass output
    static void BMCrossover_processMonoGain(BMCrossover *This,
                                            const float* input,
                                            float* lowpass,
                                            float* highpass,
                                            float highGain,
                                            size_t numSamples){
        BMMultiLevelSVF_processBufferMonoSplit(&This->split,
                                               input,
                                               lowpass, highpass,
                                               highGain,
                                               numSamples);
        if(This->fourthOrder){
            BMMultiLevelSVF_processBufferMono(&This->lp2, lowpass, lowpass, numSamples);
            BMMultiLevelSVF_processBufferMono(&This->hp2, highpass, highpass, numSamples);
        }
    }
    
    
    
    /*
     * Apply the crossover to stereo audio
     *
     * @param inL        - array of left channel audio input
     * @param inR        - array of right channel audio input
     * @param lowpassL   - lowpass output left
     * @param lowpassR   - lowpass output right
     * @param highpassL  - highpass output left
     * @param highpassR  - highpass output right
     * @param numSamples - number of samples to process. all arrays must have at least this length
	 */
	void BMCrossover_processStereo(BMCrossover *This,
								   const float* inL, const float* inR,
								   float* lowpassL, float* lowpassR,
								   float* highpassL, float* highpassR,
								   size_t numSamples){
		assert(This->stereo);
		
		BMCrossover_processStereoGain(This, inL, inR,
									  lowpassL, lowpassR, highpassL, highpassR,
									  BMCrossover_highGain(This), numSamples);
	}
    
    
    
    
    
    
    
    /*
     * Apply the crossover to mono audio
     *
     * @param input      - array of audio input
     * @param lowpass    - lowpass output
     * @param highpass   - highpass output
     * @param numSamples - number of samples to process. all arrays must have at least this length
     */
    void BMCrossover_processMono(BMCrossover *This,
                                 const float* input,
                                 float* lowpass,
                                 float* highpass,
                                 size_t numSamples){
        assert(!This->stereo);
        
        // Same sign convention as the stereo function: the 2nd order highpass
        // is inverted so that lowpass + highpass is an allpass. (The original
        // code negated the 2nd order highpass output a second time here with
        // vDSP_vneg, which put a notch at the cutoff in the sum of the bands;
        // fixed 2026-09-12.)
        BMCrossover_processMonoGain(This, input, lowpass, highpass,
                                    BMCrossover_highGain(This), numSamples);
    }
    
    void BMCrossover_recombine(const float* lpL, const float* lpR,
                                   const float* hpL, const float* hpR,
                                   float* outL, float* outR,
                                   size_t numSamples){
        vDSP_vadd(lpL, 1, hpL, 1, outL, 1, numSamples);
        vDSP_vadd(lpR, 1, hpR, 1, outR, 1, numSamples);
    }
    
    
    
    /*!
     *BMCrossover_tfMagVectors
     *
     * @abstract get points to plot a graph of each of the two bands
     *
     * @param This        pointer to an initialized struct
     * @param frequencies input vector containing the x-axis coordinates of the plot
     * @param magLow      output vector containing the y-coordinates for low band
     * @param magHigh     output vector containing the y-coordinates for high band
     * @param length      length of the input and output vectors
     */
    void BMCrossover_tfMagVectors(BMCrossover *This,
                                      const float* frequencies,
                                      float* magLow,
                                      float* magHigh,
                                  size_t length){
        BMMultiLevelBiquad_tfMagVector(&This->plotFilters[0], frequencies, magLow, length);
        BMMultiLevelBiquad_tfMagVector(&This->plotFilters[1], frequencies, magHigh, length);
    }
    
    
    
    
    
    
    /*!
     *BMCrossover3way_init
     * @abstract This function must be called prior to use
     *
     * @param cutoff1     - cutoff frequency between bands 1 and 2 in hz
     * @param cutoff2     - cutoff frequency between bands 2 and 3 in hz
     * @param sampleRate  - sample rate in hz
     * @param fourthOrder - true: 4th order, false: 2nd order
     * @param stereo      - true: stereo, false: mono
     */
    void BMCrossover3way_init(BMCrossover3way *This,
                              float cutoff1,
                              float cutoff2,
                              float sampleRate,
                              bool fourthOrder,
                              bool stereo){
        
        This->fourthOrder = fourthOrder;
        This->stereo      = stereo;
        
        // we need one filter section for 2nd order; two for 4th
        size_t levelsPerFilter = fourthOrder ? 2:1;
        
        // two 2-way crossovers: the first splits the low band off, the
        // second splits what remains into mid and high
        BMCrossover_init(&This->xo1, cutoff1, sampleRate, fourthOrder, stereo);
        BMCrossover_init(&This->xo2, cutoff2, sampleRate, fourthOrder, stereo);
        
        // lowpass at cutoff2 for the low band, so that it gets the same phase
        // shift as the mid and high bands and the bands sum back correctly
        BMMultiLevelSVF_init(&This->lowPhase, levelsPerFilter, sampleRate, stereo);
        
        // init filters for plotting
        BMMultiLevelBiquad_init(&This->plotFilters[0],
                                levelsPerFilter,
                                sampleRate,
                                false, false, false);
        BMMultiLevelBiquad_init(&This->plotFilters[1],
                                2*levelsPerFilter,
                                sampleRate,
                                false, false, false);
        BMMultiLevelBiquad_init(&This->plotFilters[2],
                                levelsPerFilter,
                                sampleRate,
                                false, false, false);
        
        BMCrossover3way_setCutoff1(This, cutoff1);
        BMCrossover3way_setCutoff2(This, cutoff2);
    }
    
    
    
    /*!
     *BMCrossover3way_free
     */
    void BMCrossover3way_free(BMCrossover3way *This){
        // audio filters
        BMCrossover_free(&This->xo1);
        BMCrossover_free(&This->xo2);
        BMMultiLevelSVF_free(&This->lowPhase);
        
        // plot filters
        for(size_t i=0; i<3; i++)
            BMMultiLevelBiquad_free(&This->plotFilters[i]);
    }
    
    
    
    
    void BMCrossover3way_setCutoff1(BMCrossover3way *This, float fc){
        // for audio
        BMCrossover_setCutoff(&This->xo1, fc);
        
        // for plotting
        if(This->fourthOrder){
            BMMultiLevelBiquad_setLinkwitzRileyLP4thOrder(&This->plotFilters[0], fc, 0);
            BMMultiLevelBiquad_setLinkwitzRileyHP4thOrder(&This->plotFilters[1], fc, 0);
        }
        else {
            BMMultiLevelBiquad_setLinkwitzRileyLP(&This->plotFilters[0], fc, 0);
            BMMultiLevelBiquad_setLinkwitzRileyHP(&This->plotFilters[1], fc, 0);
        }
    }
    
    
	
	
    void BMCrossover3way_setCutoff2(BMCrossover3way *This, float fc){
        // here we set the lowpass filter on both the mid and the low
        // frequencies so that the lowpass filter will have the same phase shift
        // as the mid and high frequencies when we add it all back together
        BMCrossover_setCutoff(&This->xo2, fc);
        
        if(This->fourthOrder){
            // for audio
            BMMultiLevelSVF_setLinkwitzRileyLP4thOrder(&This->lowPhase, fc, 0);
            
            // for plotting
            BMMultiLevelBiquad_setLinkwitzRileyLP4thOrder(&This->plotFilters[1], fc, 2);
            BMMultiLevelBiquad_setLinkwitzRileyHP4thOrder(&This->plotFilters[2], fc, 0);
        }
        else {
            // for audio
            BMMultiLevelSVF_setLinkwitzRileyLP(&This->lowPhase, fc, 0);
            
            // for plotting
            BMMultiLevelBiquad_setLinkwitzRileyLP(&This->plotFilters[1], fc, 1);
            BMMultiLevelBiquad_setLinkwitzRileyHP(&This->plotFilters[2], fc, 0);
        }
    }
    
    
    void BMCrossover3way_processStereo(BMCrossover3way *This,
                                       const float* inL, const float* inR,
                                       float* lowL, float* lowR,
                                       float* midL, float* midR,
                                       float* highL, float* highR,
                                       size_t numSamples){
        assert(This->stereo);
        
        float highGain = BMCrossover_highGain(&This->xo1);
        
        // split the low band off; the mid buffer receives mid + high
        BMCrossover_processStereoGain(&This->xo1, inL, inR,
                                      lowL, lowR, midL, midR,
                                      highGain, numSamples);
        
        // split mid + high into mid (in place) and high
        BMCrossover_processStereoGain(&This->xo2, midL, midR,
                                      midL, midR, highL, highR,
                                      highGain, numSamples);
        
        // lowpass the low band at cutoff2 too, to preserve phase
        BMMultiLevelSVF_processBufferStereo(&This->lowPhase,
                                            lowL, lowR,
                                            lowL, lowR,
                                            numSamples);
    }
	
	
	
	
	void BMCrossover3way_processMono(BMCrossover3way *This,
                                       const float* inL,
                                       float* lowL,
                                       float* midL,
                                       float* highL,
                                       size_t numSamples){
        assert(!This->stereo);
        
        float highGain = BMCrossover_highGain(&This->xo1);
        
        // split the low band off; the mid buffer receives mid + high
        BMCrossover_processMonoGain(&This->xo1, inL, lowL, midL, highGain, numSamples);
        
        // split mid + high into mid (in place) and high
        BMCrossover_processMonoGain(&This->xo2, midL, midL, highL, highGain, numSamples);
        
        // lowpass the low band at cutoff2 too, to preserve phase
        BMMultiLevelSVF_processBufferMono(&This->lowPhase, lowL, lowL, numSamples);
    }
    
    
    
    
    
    /*!
     *BMCrossover3way_tfMagVectors
     *
     * @abstract get points to plot a graph of each of the four bands
     *
     * @param This        pointer to an initialized struct
     * @param frequencies input vector containing the x-axis coordinates of the plot
     * @param magLow      output vector containing the y-coordinates for low band
     * @param magMid      output vector containing the y-coordinates for mid band
     * @param magHigh     output vector containing the y-coordinates for high band
     * @param length      length of the input and output vectors
     */
    void BMCrossover3way_tfMagVectors(BMCrossover3way *This,
                                      const float* frequencies,
                                      float* magLow,
                                      float* magMid,
                                      float* magHigh,
                                      size_t length){
        BMMultiLevelBiquad_tfMagVector(&This->plotFilters[0], frequencies, magLow, length);
        BMMultiLevelBiquad_tfMagVector(&This->plotFilters[1], frequencies, magMid, length);
        BMMultiLevelBiquad_tfMagVector(&This->plotFilters[2], frequencies, magHigh, length);
    }
    
    
    
    
    
    /*!
     *BMCrossover4way_init
     * @abstract This function must be called prior to use
     *
     * @param cutoff1     - cutoff frequency between bands 1 and 2 in hz
     * @param cutoff2     - cutoff frequency between bands 2 and 3 in hz
     * @param cutoff3     - cutoff frequency between bands 2 and 3 in hz
     * @param sampleRate  - sample rate in hz
     * @param fourthOrder - true: 4th order, false: 2nd order
     * @param stereo      - true: stereo, false: mono
     */
    void BMCrossover4way_init(BMCrossover4way *This,
                              float cutoff1,
                              float cutoff2,
                              float cutoff3,
                              float sampleRate,
                              bool fourthOrder,
                              bool stereo){
        
        This->fourthOrder = fourthOrder;
        This->stereo      = stereo;
        
        // we need one filter section for 2nd order; two for 4th
        size_t levelsPerFilter = fourthOrder ? 2:1;
        
        // three 2-way crossovers in a chain
        BMCrossover_init(&This->xo1, cutoff1, sampleRate, fourthOrder, stereo);
        BMCrossover_init(&This->xo2, cutoff2, sampleRate, fourthOrder, stereo);
        BMCrossover_init(&This->xo3, cutoff3, sampleRate, fourthOrder, stereo);
        
        // phase-matching lowpasses: band 1 also passes through lowpasses at
        // cutoff2 and cutoff3, band 2 through a lowpass at cutoff3
        BMMultiLevelSVF_init(&This->band1Phase, 2*levelsPerFilter, sampleRate, stereo);
        BMMultiLevelSVF_init(&This->band2Phase, levelsPerFilter, sampleRate, stereo);
        
        // init filters for plotting
        BMMultiLevelBiquad_init(&This->plotFilters[0],
                                levelsPerFilter,
                                sampleRate,
                                false, false, false);
        BMMultiLevelBiquad_init(&This->plotFilters[1],
                                2*levelsPerFilter,
                                sampleRate,
                                false, false, false);
        BMMultiLevelBiquad_init(&This->plotFilters[2],
                                2*levelsPerFilter,
                                sampleRate,
                                false, false, false);
        BMMultiLevelBiquad_init(&This->plotFilters[3],
                                levelsPerFilter,
                                sampleRate,
                                false, false, false);
        
        BMCrossover4way_setCutoff1(This, cutoff1);
        BMCrossover4way_setCutoff2(This, cutoff2);
        BMCrossover4way_setCutoff3(This, cutoff3);
    }
    
    
    /*!
     *BMCrossover4way_free
     */
    void BMCrossover4way_free(BMCrossover4way *This){
        BMCrossover_free(&This->xo1);
        BMCrossover_free(&This->xo2);
        BMCrossover_free(&This->xo3);
        BMMultiLevelSVF_free(&This->band1Phase);
        BMMultiLevelSVF_free(&This->band2Phase);
        
        for(size_t i=0; i<4; i++)
            BMMultiLevelBiquad_free(&This->plotFilters[i]);
    }
    
    
    void BMCrossover4way_setCutoff1(BMCrossover4way *This, float fc){
        // for audio
        BMCrossover_setCutoff(&This->xo1, fc);
        
        // for plotting
        if(This->fourthOrder){
            BMMultiLevelBiquad_setLinkwitzRileyLP4thOrder(&This->plotFilters[0], fc, 0);
            BMMultiLevelBiquad_setLinkwitzRileyHP4thOrder(&This->plotFilters[1], fc, 0);
        }
        else {
            BMMultiLevelBiquad_setLinkwitzRileyLP(&This->plotFilters[0], fc, 0);
            BMMultiLevelBiquad_setLinkwitzRileyHP(&This->plotFilters[1], fc, 0);
        }
    }
    
    
    
    void BMCrossover4way_setCutoff2(BMCrossover4way *This, float fc){
        // here we set the lowpass filter on both the mid and the low
        // frequencies so that the lowpass filter will have the same phase shift
        // as the mid and high frequencies when we add it all back together
        BMCrossover_setCutoff(&This->xo2, fc);
        
        if(This->fourthOrder){
            // for audio
            BMMultiLevelSVF_setLinkwitzRileyLP4thOrder(&This->band1Phase, fc, 0);
            
            // for plotting
            BMMultiLevelBiquad_setLinkwitzRileyLP4thOrder(&This->plotFilters[1], fc, 2);
            BMMultiLevelBiquad_setLinkwitzRileyHP4thOrder(&This->plotFilters[2], fc, 0);
        }
        else {
            // for audio
            BMMultiLevelSVF_setLinkwitzRileyLP(&This->band1Phase, fc, 0);
            
            // for plotting
            BMMultiLevelBiquad_setLinkwitzRileyLP(&This->plotFilters[1], fc, 1);
            BMMultiLevelBiquad_setLinkwitzRileyHP(&This->plotFilters[2], fc, 0);
        }
    }
    
    
    
    void BMCrossover4way_setCutoff3(BMCrossover4way *This, float fc){
        // here we set the lowpass filter on both the mid and the low
        // frequencies so that the lowpass filter will have the same phase shift
        // as the mid and high frequencies when we add it all back together
        BMCrossover_setCutoff(&This->xo3, fc);
        
        if(This->fourthOrder){
            // for audio
            BMMultiLevelSVF_setLinkwitzRileyLP4thOrder(&This->band2Phase, fc, 0);
            BMMultiLevelSVF_setLinkwitzRileyLP4thOrder(&This->band1Phase, fc, 2);
            
            // for plotting
            BMMultiLevelBiquad_setLinkwitzRileyLP4thOrder(&This->plotFilters[2], fc, 2);
            BMMultiLevelBiquad_setLinkwitzRileyHP4thOrder(&This->plotFilters[3], fc, 0);
        }
        else {
            // for audio
            BMMultiLevelSVF_setLinkwitzRileyLP(&This->band2Phase, fc, 0);
            BMMultiLevelSVF_setLinkwitzRileyLP(&This->band1Phase, fc, 1);
            
            // for plotting
            BMMultiLevelBiquad_setLinkwitzRileyLP(&This->plotFilters[2], fc, 1);
            BMMultiLevelBiquad_setLinkwitzRileyHP(&This->plotFilters[3], fc, 0);
        }
    }
    
    
    
    
    
    void BMCrossover4way_processStereo(BMCrossover4way *This,
                                       const float* inL, const float* inR,
                                       float* band1L, float* band1R,
                                       float* band2L, float* band2R,
                                       float* band3L, float* band3R,
                                       float* band4L, float* band4R,
                                       size_t numSamples){
        assert(This->stereo);
        
        float highGain = BMCrossover_highGain(&This->xo1);
        
        // split band 1 off; band 2 receives bands 2-4
        BMCrossover_processStereoGain(&This->xo1, inL, inR,
                                      band1L, band1R, band2L, band2R,
                                      highGain, numSamples);
        // split bands 2-4 into band 2 (in place) and bands 3-4
        BMCrossover_processStereoGain(&This->xo2, band2L, band2R,
                                      band2L, band2R, band3L, band3R,
                                      highGain, numSamples);
        // split bands 3-4 into band 3 (in place) and band 4
        BMCrossover_processStereoGain(&This->xo3, band3L, band3R,
                                      band3L, band3R, band4L, band4R,
                                      highGain, numSamples);
        
        // phase-matching lowpasses on bands 1 and 2
        BMMultiLevelSVF_processBufferStereo(&This->band2Phase, band2L, band2R, band2L, band2R, numSamples);
        BMMultiLevelSVF_processBufferStereo(&This->band1Phase, band1L, band1R, band1L, band1R, numSamples);
    }
	
	
	
	
	
	void BMCrossover4way_processMono(BMCrossover4way *This,
                                       const float* in,
                                       float* band1,
                                       float* band2,
                                       float* band3,
                                       float* band4,
                                       size_t numSamples){
        assert(!This->stereo);
        
        float highGain = BMCrossover_highGain(&This->xo1);
        
        // split band 1 off; band 2 receives bands 2-4
        BMCrossover_processMonoGain(&This->xo1, in, band1, band2, highGain, numSamples);
        // split bands 2-4 into band 2 (in place) and bands 3-4
        BMCrossover_processMonoGain(&This->xo2, band2, band2, band3, highGain, numSamples);
        // split bands 3-4 into band 3 (in place) and band 4
        BMCrossover_processMonoGain(&This->xo3, band3, band3, band4, highGain, numSamples);
        
        // phase-matching lowpasses on bands 1 and 2
        BMMultiLevelSVF_processBufferMono(&This->band2Phase, band2, band2, numSamples);
        BMMultiLevelSVF_processBufferMono(&This->band1Phase, band1, band1, numSamples);
    }
	
	
	
	
	

	/*!
	 *BMCrossover3way_recombine
	 */
	void BMCrossover3way_recombine(const float* bassL, const float* bassR,
								   const float* midL, const float* midR,
								   const float* trebleL, const float* trebleR,
								   float* outL, float* outR,
								   size_t numSamples){
		vDSP_vadd(bassL, 1, midL, 1, outL, 1, numSamples);
		vDSP_vadd(bassR, 1, midR, 1, outR, 1, numSamples);
		vDSP_vadd(trebleL,1,outL,1,outL,1,numSamples);
		vDSP_vadd(trebleR,1,outR,1,outR,1,numSamples);
	}
	
	
	
	

	/*!
	 *BMCrossover4way_recombine
	 */
	void BMCrossover4way_recombine(const float* band1L, const float* band1R,
								   const float* band2L, const float* band2R,
								   const float* band3L, const float* band3R,
								   const float* band4L, const float* band4R,
								   float* outL, 		float* outR,
								   size_t numSamples){
		
		vDSP_vadd(band1L,1,band2L,1,outL,1,numSamples);
		vDSP_vadd(band3L,1,outL,1,outL,1,numSamples);
		vDSP_vadd(band4L,1,outL,1,outL,1,numSamples);
        vDSP_vadd(band1R,1,band2R,1,outR,1,numSamples);
        vDSP_vadd(band3R,1,outR,1,outR,1,numSamples);
        vDSP_vadd(band4R,1,outR,1,outR,1,numSamples);

	}
	
	
	
	
	/*!
	 *BMCrossover4way_recombineMono
	 */
	void BMCrossover4way_recombineMono(const float* band1,
									   const float* band2,
									   const float* band3,
									   const float* band4,
									   float* out,
									   size_t numSamples){
		
		vDSP_vadd(band1,1,band2,1,out,1,numSamples);
		vDSP_vadd(band3,1,out,1,out,1,numSamples);
		vDSP_vadd(band4,1,out,1,out,1,numSamples);

	}
    
    
    
    
    

    //void BMMultiLevelBiquad_tfMagVector(BMMultiLevelBiquad* This, const float *frequency, float *magnitude, size_t length);
    /*!
     *BMCrossover4way_tfMagVectors
     *
     * @abstract get points to plot a graph of each of the four bands
     *
     * @param This        pointer to an initialized struct
     * @param frequencies input vector containing the x-axis coordinates of the plot
     * @param magBand1    output vector containing the y-coordinates for band 1
     * @param magBand2    output vector containing the y-coordinates for band 2
     * @param magBand3    output vector containing the y-coordinates for band 3
     * @param magBand4    output vector containing the y-coordinates for band 4
     * @param length      length of the input and output vectors
     */
    void BMCrossover4way_tfMagVectors(BMCrossover4way *This,
                                      const float* frequencies,
                                      float* magBand1,
                                      float* magBand2,
                                      float* magBand3,
                                      float* magBand4,
                                      size_t length){
        BMMultiLevelBiquad_tfMagVector(&This->plotFilters[0], frequencies, magBand1, length);
        BMMultiLevelBiquad_tfMagVector(&This->plotFilters[1], frequencies, magBand2, length);
        BMMultiLevelBiquad_tfMagVector(&This->plotFilters[2], frequencies, magBand3, length);
        BMMultiLevelBiquad_tfMagVector(&This->plotFilters[3], frequencies, magBand4, length);
    }

    
    

	
	/*!
	 *BMCrossover_impulseResponse
	 */
	void BMCrossover_impulseResponse(BMCrossover *This, float* IRL, float* IRR, size_t numSamples);

	/*!
	 *BMCrossover3way_impulseResponse
	 */
	void BMCrossover3way_impulseResponse(BMCrossover3way *This, float* IRL, float* IRR, size_t numSamples);

	/*!
	 *BMCrossover4way_impulseResponse
	 */
	void BMCrossover4way_impulseResponse(BMCrossover4way *This, float* IRL, float* IRR, size_t numSamples);

	
    
#ifdef __cplusplus
}
#endif
