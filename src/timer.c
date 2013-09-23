/*
 * timer.c
 *
 *  Created on: Jun 23, 2012
 *      Author: petera
 */

#include "timer.h"
#include "system.h"
#include "miniutils.h"
#include "gpio.h"
#ifdef CONFIG_OS
#include "os.h"
#endif

static u32_t q;
void TIMER_irq() {
  if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    q++;
    if ((q & 0xffff) < 0x1ff) {
      gpio_enable(PORTF, PIN6);
    } else {
      gpio_disable(PORTF, PIN6);
    }
    bool ms_update = SYS_timer();
    if (ms_update) {
      TRACE_MS_TICK(SYS_get_time_ms() & 0xff);
    }
#ifdef CONFIG_TASK_QUEUE
    TASK_timer();
#endif
#ifdef CONFIG_OS
    if (ms_update) {
      __os_time_tick(SYS_get_time_ms());
    }
#endif
  }
}
