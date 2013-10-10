/*
 * system_config.h
 *
 *  Created on: Jul 24, 2012
 *      Author: petera
 */

#ifndef SYSTEM_CONFIG_H_
#define SYSTEM_CONFIG_H_

#include "config_header.h"

#include "stm32f4xx.h"
#include "core_cm4.h"

// damn that blasted eclipse indexer!
// from FunctionalState typedef enum in stm32f4xx.h
#define DISABLE (0)
#define ENABLE  (!DISABLE)
// from FlagStatus, ITStatus typedef enum in stm32f4xx.h
#define RESET   (0)
#define SET     (!RESET)

#include "types.h"

#define APP_NAME "larvbot"


/****************************************/
/***** Functionality block settings *****/
/****************************************/


#ifdef CONFIG_UART
#define CONFIG_UART2
#define CONFIG_UART4
// update according to enabled uarts
#define CONFIG_UART_CNT   2
#endif

#define IOSTD             0 // usb serial
#define IOWIFI            1 // uart2
#define IODBG             2 // uart4

#ifdef CONFIG_USB_VCD
#define CONFIG_IO_MAX     (CONFIG_UART_CNT + 1)
#else
#define CONFIG_IO_MAX     (CONFIG_UART_CNT)
#endif

/*********************************************/
/***** Hardware build time configuration *****/
/*********************************************/

/** Processor specifics **/

#ifndef USER_HARDFAULT
// enable user hardfault handler
#define USER_HARDFAULT 1
#endif

// hardware debug (blinking leds etc)
#define HW_DEBUG


/** General **/
// internal flash start address
#define FLASH_START       FLASH_BASE
// internal flash page erase size
#define FLASH_PAGE_SIZE   0x800 // hd
// internal flash protection/unprotection for firmware
#define FLASH_PROTECT     FLASH_WRProt_AllPages
// internal flash total size in bytes
#define FLASH_TOTAL_SIZE  (1024*1024) // stm32f407zgt

// firmware upgrade placement on spi flash
// TODO PETER
#if 0
#define FIRMWARE_SPIF_ADDRESS_BASE   \
  (FLASH_M25P16_SIZE_TOTAL - \
      ((FLASH_TOTAL_SIZE+sizeof(fw_upgrade_info))/FLASH_M25P16_SIZE_SECTOR_ERASE)*FLASH_M25P16_SIZE_SECTOR_ERASE - \
      FLASH_M25P16_SIZE_SECTOR_ERASE)
#endif
#define FIRMWARE_SPIF_ADDRESS_BASE 0

#define FIRMWARE_SPIF_ADDRESS_META (FIRMWARE_SPIF_ADDRESS_BASE)
#define FIRMWARE_SPIF_ADDRESS_DATA (FIRMWARE_SPIF_ADDRESS_BASE + sizeof(fw_upgrade_info))

/** SPI **/

#ifdef CONFIG_SPI

// make SPI driver use polling method, otherwise DMA requests are used
// warning - polling method should only be used for debugging and may be
// unstable. Do nod sent multitudes of data using this config
//#define CONFIG_SPI_POLL

#define CONFIG_SPI_CNT             1
/** SPI FLASH **/

// spi flash chip select port and pin
#define SPI_FLASH_GPIO_PORT          GPIOA
#define SPI_FLASH_GPIO_PIN           GPIO_Pin_4

#endif // CONFIG_SPI

/** I2C **/

#ifdef CONFIG_I2C

#define I2C1_PORT                     I2C1
#define I2C_MAX_ID                    1

#endif

/** ADC **/

#define CONFIG_ADC

/** LED **/

#ifdef CONFIG_LED

#define LED12_GPIO_PORT       GPIOD
#define LED12_APBPeriph_GPIO  RCC_APB2Periph_GPIOD
#define LED34_GPIO_PORT       GPIOC
#define LED34_APBPeriph_GPIO  RCC_APB2Periph_GPIOC
#define LED1_GPIO             GPIO_Pin_6
#define LED2_GPIO             GPIO_Pin_13
#define LED3_GPIO             GPIO_Pin_7
#define LED4_GPIO             GPIO_Pin_6

#endif

/** USR232 WIFI **/

#define WIFI_GPIO_PORT        GPIOG
#define WIFI_GPIO_RESET_PIN   GPIO_Pin_11
#define WIFI_GPIO_LINK_PIN    GPIO_Pin_12
#define WIFI_GPIO_READY_PIN   GPIO_Pin_13
#define WIFI_GPIO_RELOAD_PIN  GPIO_Pin_14
#define WIFI_UART             WIFIIN
#define WIFI_UART_BAUD        57600

// miniutils

#define USE_COLOR_CODING


/****************************************************/
/******** Application build time configuration ******/
/****************************************************/

/** TICKER **/

// system timer frequency
#define SYS_MAIN_TIMER_FREQ   10000
// system tick frequency
#define SYS_TIMER_TICK_FREQ   1000

/** UART **/

// usart2
#define WIFIIN      0
#define WIFIOUT     0

// uart4
#define UARTSTDIN   1
#define UARTSTDOUT  1

#define UART1_SPEED 115200
#define UART2_SPEED WIFI_UART_BAUD
#define UART3_SPEED 115200
#define UART4_SPEED 912600

/** OS **/

// ctx switch frequency in hertz
#define CONFIG_OS_PREEMPT_FREQ  2000
// if enabled, signalled threads will be executed in next ctx switch
#define CONFIG_OS_BUMP          1

#define XRAM_BEGIN              ((void*)0x64000000)
#define XRAM_SIZE               (128*1024)
#define XRAM_END                ((void*)(XRAM_BEGIN + XRAM_SIZE))

#define VALID_RAM(x) \
  (((void*)(x) >= RAM_BEGIN && (void*)(x) < RAM_END) || ((void*)(x) >= XRAM_BEGIN && (void*)(x) < XRAM_END))

#define VALID_FLASH(x) \
  ((void*)(x) >= (void*)FLASH_BEGIN && (void*)(x) < (void*)(FLASH_END))

#define VALID_DATA(x) \
  (VALID_RAM(x) || VALID_FLASH(x))

#define OS_DBG_BADR(x) \
    (!VALID_RAM(x))


/** WIFI **/

#define CONFIG_WIFI

#define CONFIG_WIFI_RX_MAX_LEN  2048
#define CONFIG_WIFI_TX_MAX_LEN  2048


/** DEBUG **/

// disable all asserts
//#define ASSERT_OFF

// disable all debug output
//#define DBG_OFF

#define CONFIG_DEFAULT_DEBUG_MASK     (0)

// enable or disable tracing
#define DBG_TRACE_MON
#define TRACE_SIZE                    (512)

// enable debug monitor for os
#define OS_DBG_MON                    1

// enable stack boundary checks
#define OS_STACK_CHECK                1

// enable stack usage checks
#define OS_STACK_USAGE_CHECK          1

// enable os scheduler validity
#define OS_RUNTIME_VALIDITY_CHECK     0


#endif /* SYSTEM_CONFIG_H_ */
