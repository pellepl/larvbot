/*
 * signal_yin_est.h
 *
 * YIN pitch detection, somewhat modified for speed on embedded targets,
 * designed for realtime analysis.
 *
 * Assumes a circular sample buffer. Is fed each time a new sample occurs.
 *
 *  Created on: Nov 11, 2013
 *      Author: petera
 */

#ifndef SIGNAL_YIN_EST_H_
#define SIGNAL_YIN_EST_H_

#include "system.h"

/* bitdefinition of a sample */
typedef u8_t yin_sample;
/* bitdefinition of a correlation, must hold max value MAX(yin_sample)^2 * window * YIN_ACC */
typedef s32_t yin_corr;
/* bitdefinition of a normalization, must hold max value YIN_ACC * MAX(yin_sample) * window */
typedef s32_t yin_prim;

#define YIN_ACC           32

typedef struct {
  yin_sample *sample_buf;
  u32_t sample_buf_len;
  u32_t sample_buf_ix;

  u32_t window;
  yin_corr *corr_buf;
  yin_prim *prim_buf;

  u32_t last_period;
  s32_t period_lifetime;
} yin;

/*
 * Initiates a yin pitch detector
 * @param y               the pitch detector metainfo
 * @param sample_buf      address of the circular sample buffer
 * @param sample_buf_len  length of the sample buffer in samples
 * @param window          defines the longest recognizable period
 * @param corr_buf        address of memory used for correlation,
 *                        must be sizeof(yin_corr)*window bytes
 * @param prim_buf        address of memory used for calculation,
 *                        must be sizeof(yin_prim)*window bytes
 */
void yin_init(yin *y,
    yin_sample *sample_buf, u32_t sample_buf_len,
    u32_t window,
    yin_corr *corr_buf, yin_corr *prim_buf);

/*
 * Detects the period for given sample index
 * @return  0 if no period is found, else the period
 */
u32_t yin_detect(yin *y, s32_t spl_ix);

#endif /* SIGNAL_YIN_EST_H_ */
