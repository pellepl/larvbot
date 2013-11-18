/*
 * svideo_test.c
 *
 *  Created on: Nov 9, 2013
 *      Author: petera
 */

#include "svideo_test.h"
#include "trig_q.h"

#define VMAX    77-5
#define V1_0    VMAX
#define V0_3    23-5
#define V0      0


#if 0
#define FACT    1
#define FREQ    FACT*2000000
#define HCOUNT  FACT*127             // 63.5 uS = 15748 Hz
#define LEN_HSYNC         FACT*9     //4.5us (4.7us) -0.2
#define LEN_BACK_PORCH    FACT*12    //6us (5.9us)   +0.1
#define LEN_ACTIVE        FACT*103   //51.5us
#define LEN_FRONT_PORCH   FACT*3     //1.5us (1.4us) +0.1
static int vline = 0;
static int hline = 0;
static bool even = FALSE;
#else

// http://www.batsocks.co.uk/readme/video_timing.htm

#define SCANLINE_NS       64000
#define HSYNC_NS          4700
#define BACKPORCH_NS      5700
#define DISP_NS           51950
#define DISP_TEST_NS      20000
#define FRONTPORCH_NS     1650
#define GAP_NS            4700
#define SHORT_PULSE_NS    4000//2350

#define CLK_MHZ           168
#define NS_TO_TICKS(x)    ((x)*CLK_MHZ/1000)

static int state = 0;
static int scanline = 0;
#endif


#define out(x)      DAC_SetChannel1Data(DAC_Align_8b_R, (x))
#define next(x)     TIM_SetAutoreload(TIM5, (x)-1)
#define next_ns(x)  next(NS_TO_TICKS((x)))

void SVIDEO_irq(void) {
  if (TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET) {
    TIM_ClearITPendingBit(TIM5, TIM_IT_Update);
//    state++;
//    if (state & 1) {
//      next_ns(SHORT_PULSE_NS);
//      out(V0);
//    } else {
//      next_ns(SCANLINE_NS-SHORT_PULSE_NS);
//      out(V1_0);
//    }
//
//    return;

    switch (state) {
    // vsync scanlines 1-5
    case 0:
    case 2:
    case 4:
    case 6:
    case 8:
      next_ns(SCANLINE_NS/2-GAP_NS);
      out(V0);
      break;
    case 1:
    case 3:
    case 5:
    case 7:
    case 9:
      next_ns(GAP_NS);
      out(V0_3);
      break;

    case 10:
    case 12:
    case 14:
    case 16:
    case 18:
      next_ns(SHORT_PULSE_NS);
      out(V0);
      break;
    case 11:
    case 13:
    case 15:
    case 17:
    case 19:
      next_ns(SCANLINE_NS/2-SHORT_PULSE_NS);
      out(V0_3);
      scanline = 6;
      break;

      // empty top
    case 20:
      next_ns(HSYNC_NS);
      out(V0);
      break;
    case 21:
      next_ns(SCANLINE_NS-HSYNC_NS-FRONTPORCH_NS);
      out(V0_3);
      break;
    case 22:
      next_ns(FRONTPORCH_NS);
      out(V0);
      scanline++;
      if (scanline < 24) state = 20-1;
      break;

      // gfx
    case 23:
      next_ns(HSYNC_NS);
      out(V0);
      break;
    case 24:
      next_ns(BACKPORCH_NS);
      out(V0_3);
      break;
    case 25:
      next_ns(DISP_TEST_NS);
      if ((scanline & 0x7f) < 0x40)
        out(V1_0);
      else
        out(V0_3);
      break;
    case 26:
      next_ns(SCANLINE_NS-DISP_TEST_NS-HSYNC_NS-BACKPORCH_NS-FRONTPORCH_NS);
      if ((scanline & 0x7f) >= 0x40)
        out(V1_0);
      else
        out(V0_3);
      break;
    case 27:
      next_ns(FRONTPORCH_NS);
      out(V0);
      scanline++;
      if (scanline < 310) state = 23-1;
      break;

    case 28:
    case 30:
    case 32:
    case 34:
    case 36:
    case 38:
      next_ns(SHORT_PULSE_NS);
      out(V0);
      break;
    case 29:
    case 31:
    case 33:
    case 35:
    case 37:
    case 39:
      next_ns(SCANLINE_NS/2-SHORT_PULSE_NS);
      out(V0_3);
      break;
    }

    state++;
    if (state >= 40) {
      state = 0;
      scanline = 1;
    }

#if 0
    if (vline >= 243) {
      // vsync

//      if (hline < LEN_HSYNC) {
//        out(even ? V0_3 : V0);
//      } else {
//        out(even ? V0 : V0_3);
//      }

      if (hline == 0) {
        out(even ? V0_3 : V0);
      } else if (hline == LEN_HSYNC) {
        out(even ? V0 : V0_3);
      }

    } else {

      // hsync
//      if (hline < LEN_HSYNC) {
//        out(V0);
//      } else if (hline < LEN_BACK_PORCH + LEN_HSYNC) {
//        out(V0_3);
//      } else if (hline < LEN_ACTIVE + LEN_BACK_PORCH + LEN_HSYNC) {
//        // data
//        //out((hline & 1) ? V0_3 : V1_0);
//        out(V1_0);
//      } else {
//        // front porch
//        out(V0_3);
//      }
      if (hline == 0) {
        out(V0);
      } else if (hline == LEN_HSYNC) {
        out(V0_3);
      } else if (hline > LEN_HSYNC + LEN_BACK_PORCH && hline < LEN_ACTIVE + LEN_BACK_PORCH + LEN_HSYNC) {
        // data
        //out((hline & 1) ? V0_3 : V1_0);
        out(V1_0);
      } else {
        // front porch
        out(V0_3);
      }

    }
    if (hline == 0) {
      out(V0);
    } else if (hline == HCOUNT / 2) {
      out(V1_0);
    }

    hline++;
    if (hline >= HCOUNT) {
      hline = 0;
      out(V0_3);
      vline++;
      if (vline >= 262) {
        vline = 0;
        even = !even;
      }
    }
#endif
  }
}
