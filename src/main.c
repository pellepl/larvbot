#include "system.h"
#include "processor.h"
#include "usb_vcd_impl.h"
#include "miniutils.h"
#include "linker_symaccess.h"
#include "shared_mem.h"
#include "uart_driver.h"
#include "lsm303_driver.h"
#include "io.h"
#include "taskq.h"
#include "cli.h"
#ifdef CONFIG_USB_VCD
#include "usb_serial.h"
#endif

#define Bank1_SRAM2_ADDR  ((uint32_t)0x64000000)

static task_timer ticker_timer;

void ticker_task(u32_t arg, void* arg_p) {
  ioprint(IODBG, "tick %i\n", SYS_get_time_ms());
}

int main(void) {

  PROC_init();
  SYS_init();
#ifdef CONFIG_USB_VCD
  usb_serial_init();
#endif
#ifdef CONFIG_I2C
  I2C_init();
#endif
#ifdef CONFIG_UART
  UART_init();
#endif
#ifdef CONFIG_TASK_QUEUE
  TASK_init();
#endif

  IO_define(IOSTD, io_usb, 0);
  IO_define(IOWIFI, io_uart, 0);
  IO_define(IODBG, io_uart, 1);

  CLI_init();

  print("\n\n\nHardware initialization done\n");

  print("Shared memory on 0x%08x\n", SHARED_MEMORY_ADDRESS);
  bool shmem_resetted = SHMEM_validate();
  if (!shmem_resetted) {
    print("Shared memory reset\n");
  }
  enum reboot_reason_e rr = SHMEM_get()->reboot_reason;
  SHMEM_set_reboot_reason(REBOOT_UNKONWN);
  print("Reboot reason: %i\n", rr);
  if (rr == REBOOT_EXEC_BOOTLOADER) {
//    print("Peripheral init for bootloader\n");
//    PROC_periph_init_bootloader();
//    print("Bootloader execute\n");
//    bootloader_execute();
  }

  print("Stack 0x%08x -- 0x%08x: %iK\n", STACK_START, STACK_END, (STACK_END - STACK_START)/1024);

  print("Subsystem initialization done\n");

  UART_assure_tx(_UART(0), TRUE);
  UART_set_callback(_UART(0), 0, 0);

  task *ticker = TASK_create(ticker_task, TASK_STATIC);
  TASK_start_timer(ticker, &ticker_timer, 0, 0, 0, 1000, "ticker");

  SYS_dbg_mask_disable(D_I2C);

  IO_assure_tx(IOSTD, TRUE);

  while (1) {
    while (TASK_tick());
    //TASK_wait(); // why the hell wont this work??
    __WFI();
  }
}


