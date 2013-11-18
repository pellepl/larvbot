#include "signal_yin_est.h"

/* W = yin window (longest recognizable period in samples)
   spl = sample buffer
*/

// Step 2 Difference function:
// diff_corr[tau] = SUM{j=1..W}(spl[j]-spl[j+tau])^2; tau = 1..W
//
// Instead of recalculating full sum of each diff_corr entry each new sample (O(n2)),
// we add the new product and subtract the product from the sum which falls out (O(n)):
//
// for each new sample j:
//   diff_corr[tau] += (spl[j-W]-spl[j+tau-W])^2
//   diff_corr[tau] -= (spl[j-2*W]-spl[j+tau-2*W])^2
//   for all tau = 1..W
//
static void yin_diff_corr(yin *y, s32_t spl_ix) {
  yin_sample *spl_buf = y->sample_buf;
  u32_t spl_buf_len = y->sample_buf_len;
  u32_t window = y->window;

  spl_ix -= window;
  if (spl_ix < 0) {
    spl_ix += spl_buf_len;
  }
  s32_t spl_discard_ix = spl_ix - window;
  if (spl_discard_ix < 0) {
    spl_discard_ix += spl_buf_len;
  }

  s32_t tau;
  for (tau = 0; tau < window; tau++) {
    s32_t spl_tau_ix = spl_ix + tau + 1;
    if (spl_tau_ix >= spl_buf_len) {
      spl_tau_ix -= spl_buf_len;
    }
    s32_t spl_tau_discard_ix = spl_discard_ix + tau + 1;
    if (spl_tau_discard_ix >= spl_buf_len) {
      spl_tau_discard_ix -= spl_buf_len;
    }

    yin_corr d_new = (yin_corr)(spl_buf[spl_tau_ix] - spl_buf[spl_ix]);
    d_new *= d_new;

    yin_corr d_discard = (yin_corr)(spl_buf[spl_tau_discard_ix] - spl_buf[spl_discard_ix]);
    d_discard *= d_discard;

    y->corr_buf[tau] -= d_discard;
    y->corr_buf[tau] += d_new;
  }
}

// Step 3 Cumulative mean normalized difference function
// diff_corr_prim[tau] = 1 for tau = 0
//                       corr_prim[tau] / ( (1/tau)*(SUM{j=1..tau}(diff_corr[j])
//
static void yin_diff_corr_prim(yin *y) {
  yin_prim *prim_buf = y->prim_buf;
  u32_t window = y->window;

  prim_buf[0] = (yin_prim)YIN_ACC;
  s32_t tau;
  yin_corr cumu = 0;
  for (tau = 1; tau < window; tau++) {
    cumu += y->corr_buf[tau];
    if (cumu == 0) {
      prim_buf[tau] = (yin_prim)YIN_ACC;
    } else {
      prim_buf[tau] = (yin_prim)((YIN_ACC * tau * y->corr_buf[tau]) / cumu);
    }
  }
}

u32_t yin_detect(yin *y, s32_t spl_ix) {
  yin_diff_corr(y, spl_ix);
  yin_diff_corr_prim(y);

  return 0;
}

void yin_init(yin *y,
    yin_sample *sample_buf, u32_t sample_buf_len,
    u32_t window,
    yin_corr *corr_buf, yin_prim *prim_buf) {
  y->sample_buf = sample_buf;
  y->sample_buf_len = sample_buf_len;

  y->window = window;
  y->corr_buf = corr_buf;
  y->prim_buf = prim_buf;

  memset(corr_buf, 0, sizeof(yin_corr)*window);
  memset(prim_buf, 0, sizeof(yin_prim)*window);
}

