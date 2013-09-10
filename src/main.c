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
#ifdef CONFIG_USB_VCD
#include "usb_serial.h"
#endif

#define Bank1_SRAM2_ADDR  ((uint32_t)0x64000000)


static lsm303_dev lsm;


static task_timer ticker_timer;
static int action = 0;
static void lsm_cb(lsm303_dev *d, int res) {
  print("lsm_cb, res:%i\n", res);
  if (action == 10 || action == 11) {
    print("lsm heading:%04x\n", lsm_get_heading(&lsm));
  }
}
void ticker_task(u32_t arg, void* arg_p) {
  ioprint(IODBG, "tick %i\n", SYS_get_time_ms());
  action++;
  switch (action) {
  case 3:
    print("lsm init\n");
    lsm_open(&lsm, _I2C_BUS(0), FALSE, lsm_cb);
    break;
  case 4:
    print("lsm config default\n");
    lsm_config_default(&lsm);
    break;
  case 11:
  case 5:
    print("lsm get both\n");
    lsm_read_both(&lsm);
    action = 10;
    break;
  }
}

static void io_cb(u8_t io, void *arg, u16_t available) {
  print("rx cb on io %i, bytes available %i\n", io, available);
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

  IO_define(0, io_usb, 0);
  IO_define(1, io_uart, 0);
  IO_define(2, io_uart, 1);

  IO_set_callback(0, io_cb, 10);
  IO_set_callback(1, io_cb, 20);
  IO_set_callback(2, io_cb, 30);

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

  static u32_t i = 0;

  static volatile u32_t *sram = (u32_t*)Bank1_SRAM2_ADDR;


  UART_assure_tx(_UART(0), TRUE);
  UART_set_callback(_UART(0), 0, 0);

  task *ticker = TASK_create(ticker_task, TASK_STATIC);
  TASK_start_timer(ticker, &ticker_timer, 0, 0, 0, 1000, "ticker");

  SYS_dbg_mask_disable(D_I2C);

  while (1) {
    while (TASK_tick());
    //TASK_wait(); // why the hell wont this work??
    __WFI();
  }
}


