#include "system.h"
#include "processor.h"
#include "usb_vcd_impl.h"
#include "miniutils.h"
#include "linker_symaccess.h"
#include "shared_mem.h"
#include "cli.h"
#include "heap.h"

#ifdef CONFIG_UART
#include "uart_driver.h"
#endif
#ifdef CONFIG_LSM303
#include "lsm303_driver.h"
#endif
#include "io.h"
#ifdef CONFIG_TASK_QUEUE
#include "taskq.h"
#endif
#ifdef CONFIG_BOOTLOADER
#include "bl_exec.h"
#endif
#ifdef CONFIG_USB_VCD
#include "usb_serial.h"
#endif
#ifdef CONFIG_OS
#include "os.h"
#endif
#ifdef CONFIG_WIFI232
#include "wifi_impl.h"
#endif
#ifdef CONFIG_ADC
#include "adc_driver.h"
#endif

int main(void) {

  __disable_irq();
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

  HEAP_init();

#ifdef CONFIG_OS
  OS_init();
#endif
  __enable_irq();

  IO_define(IOSTD, io_usb, 0);
  IO_define(IOWIFI, io_uart, 0);
  IO_define(IODBG, io_uart, 1);

#ifdef CONFIG_WIFI232
  WIFI_IMPL_init();
#endif
#ifdef CONFIG_ADC
  ADC_init();
#endif

  CLI_init();

  ioprint(IODBG, "\n"APP_NAME" debug channel open\n");

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
    print("Peripheral init for bootloader\n");
    PROC_periph_init_bootloader();
    print("Bootloader execute\n");
    ioprint(IODBG, "\n\nDebug channel open\n\n");
    bootloader_execute();
  }

  print("Stack 0x%08x -- 0x%08x: %iK\n", STACK_START, STACK_END, (STACK_END - STACK_START)/1024);

  print("Subsystem initialization done\n");

  UART_assure_tx(_UART(0), TRUE);

  SYS_dbg_mask_disable(D_I2C);

  IO_assure_tx(IOSTD, TRUE);

  while (1) {
    while (TASK_tick());
    TASK_wait();
  }
}

#if USER_HARDFAULT

void **HARDFAULT_PSP;
//register void *stack_pointer asm("sp");
volatile void *stack_pointer;
volatile unsigned int stacked_r0;
volatile unsigned int stacked_r1;
volatile unsigned int stacked_r2;
volatile unsigned int stacked_r3;
volatile unsigned int stacked_r12;
volatile unsigned int stacked_lr;
volatile unsigned int stacked_pc;
volatile unsigned int stacked_psr;

void hard_fault_handler_c (unsigned int * hardfault_args)
{
  SHMEM_set_reboot_reason(REBOOT_CRASH);

  stacked_r0 = ((unsigned long) hardfault_args[0]);
  stacked_r1 = ((unsigned long) hardfault_args[1]);
  stacked_r2 = ((unsigned long) hardfault_args[2]);
  stacked_r3 = ((unsigned long) hardfault_args[3]);

  stacked_r12 = ((unsigned long) hardfault_args[4]);
  stacked_lr = ((unsigned long) hardfault_args[5]);
  stacked_pc = ((unsigned long) hardfault_args[6]);
  stacked_psr = ((unsigned long) hardfault_args[7]);

  u32_t bfar = SCB->BFAR;
  u32_t cfsr = SCB->CFSR;
  u32_t hfsr = SCB->HFSR;
  u32_t dfsr = SCB->DFSR;
  u32_t afsr = SCB->AFSR;

  // Hijack the process stack pointer to make backtrace work
  asm("mrs %0, psp" : "=r"(HARDFAULT_PSP) : :);
  stack_pointer = HARDFAULT_PSP;

  u8_t io = IODBG;

  IO_blocking_tx(io, TRUE);

  IO_tx_flush(io);

  ioprint(io, TEXT_BAD("\n!!! HARDFAULT !!!\n\n"));
  ioprint(io, "Stacked registers:\n");
  ioprint(io, "  pc:   0x%08x\n", stacked_pc);
  ioprint(io, "  lr:   0x%08x\n", stacked_lr);
  ioprint(io, "  psr:  0x%08x\n", stacked_psr);
  ioprint(io, "  sp:   0x%08x\n", stack_pointer);
  ioprint(io, "  r0:   0x%08x\n", stacked_r0);
  ioprint(io, "  r1:   0x%08x\n", stacked_r1);
  ioprint(io, "  r2:   0x%08x\n", stacked_r2);
  ioprint(io, "  r3:   0x%08x\n", stacked_r3);
  ioprint(io, "  r12:  0x%08x\n", stacked_r12);
  ioprint(io, "\nFault status registers:\n");
  ioprint(io, "  BFAR: 0x%08x\n", bfar);
  ioprint(io, "  CFSR: 0x%08x\n", cfsr);
  ioprint(io, "  HFSR: 0x%08x\n", hfsr);
  ioprint(io, "  DFSR: 0x%08x\n", dfsr);
  ioprint(io, "  AFSR: 0x%08x\n", afsr);
  ioprint(io, "\n");
  if (cfsr & (1<<(7+0))) {
    ioprint(io, "MMARVALID: MemMan 0x%08x\n", SCB->MMFAR);
  }
  if (cfsr & (1<<(4+0))) {
    ioprint(io, "MSTKERR: MemMan error during stacking\n");
  }
  if (cfsr & (1<<(3+0))) {
    ioprint(io, "MUNSTKERR: MemMan error during unstacking\n");
  }
  if (cfsr & (1<<(1+0))) {
    ioprint(io, "DACCVIOL: MemMan memory access violation, data\n");
  }
  if (cfsr & (1<<(0+0))) {
    ioprint(io, "IACCVIOL: MemMan memory access violation, instr\n");
  }

  if (cfsr & (1<<(7+8))) {
    ioprint(io, "BFARVALID: BusFlt 0x%08x\n", SCB->BFAR);
  }
  if (cfsr & (1<<(4+8))) {
    ioprint(io, "STKERR: BusFlt error during stacking\n");
  }
  if (cfsr & (1<<(3+8))) {
    ioprint(io, "UNSTKERR: BusFlt error during unstacking\n");
  }
  if (cfsr & (1<<(2+8))) {
    ioprint(io, "IMPRECISERR: BusFlt error during data access\n");
  }
  if (cfsr & (1<<(1+8))) {
    ioprint(io, "PRECISERR: BusFlt error during data access\n");
  }
  if (cfsr & (1<<(0+8))) {
    ioprint(io, "IBUSERR: BusFlt bus error\n");
  }

  if (cfsr & (1<<(9+16))) {
    ioprint(io, "DIVBYZERO: UsaFlt division by zero\n");
  }
  if (cfsr & (1<<(8+16))) {
    ioprint(io, "UNALIGNED: UsaFlt unaligned access\n");
  }
  if (cfsr & (1<<(3+16))) {
    ioprint(io, "NOCP: UsaFlt execute coprocessor instr\n");
  }
  if (cfsr & (1<<(2+16))) {
    ioprint(io, "INVPC: UsaFlt general\n");
  }
  if (cfsr & (1<<(1+16))) {
    ioprint(io, "INVSTATE: UsaFlt execute ARM instr\n");
  }
  if (cfsr & (1<<(0+16))) {
    ioprint(io, "UNDEFINSTR: UsaFlt execute bad instr\n");
  }

  if (hfsr & (1<<31)) {
    ioprint(io, "DEBUGEVF: HardFlt debug event\n");
  }
  if (hfsr & (1<<30)) {
    ioprint(io, "FORCED: HardFlt SVC/BKPT within SVC\n");
  }
  if (hfsr & (1<<1)) {
    ioprint(io, "VECTBL: HardFlt vector fetch failed\n");
  }

  TASK_dump(IODBG);

  SYS_dump_trace(IODBG);

  while(1);
}
#endif

