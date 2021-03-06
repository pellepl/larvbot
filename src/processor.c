/*
 * processor.c
 *
 *  Created on: Sep 7, 2013
 *      Author: petera
 */
#include "system.h"
#include "gpio.h"
#include "spi_driver.h"

static void rcc_config(void)
{
  RCC_PCLK1Config(RCC_HCLK_Div2); // Clock APB1 with AHB clock / 2

  // SRAM
  RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC, ENABLE);

  // TIM
  RCC_APB1PeriphClockCmd(STM32_SYSTEM_TIMER_RCC, ENABLE);

#ifdef CONFIG_UART
#ifdef CONFIG_UART2
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
#endif
#ifdef CONFIG_UART4
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);
#endif
#endif

#ifdef CONFIG_I2C
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
#endif

#ifdef CONFIG_ADC
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
#endif

#ifdef CONFIG_SPI
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
#endif

#ifdef CONFIG_SVIDEO_TEST
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
#endif

}

static void nvic_config(void)
{
  // Configure the NVIC Preemption Priority Bits.
  // preempt prios 0..3, subpriorities  0..3
  uint32_t prioGrp = (8-__NVIC_PRIO_BITS) + (2-1); // priogroup 5 for stm32f4xx
  NVIC_SetPriorityGrouping(prioGrp);

  // enable TIM2 interrupt, supahigh
  NVIC_SetPriority(STM32_SYSTEM_TIMER_IRQn, NVIC_EncodePriority(prioGrp, 0, 1));
  NVIC_EnableIRQ(STM32_SYSTEM_TIMER_IRQn);

#ifdef CONFIG_OS
  NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(prioGrp, 0, 2));
  NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(prioGrp, 3, 3));
#endif

#ifdef CONFIG_USB_VCD
  NVIC_SetPriority(OTG_FS_IRQn, NVIC_EncodePriority(prioGrp, 2, 0));
#endif

#ifdef CONFIG_UART
#ifdef CONFIG_UART2
  NVIC_SetPriority(USART2_IRQn, NVIC_EncodePriority(prioGrp, 1, 3));
  NVIC_EnableIRQ(USART2_IRQn);
#endif
#ifdef CONFIG_UART4
  NVIC_SetPriority(UART4_IRQn, NVIC_EncodePriority(prioGrp, 3, 0));
  NVIC_EnableIRQ(UART4_IRQn);
#endif
#endif

#ifdef CONFIG_I2C
  NVIC_SetPriority(I2C1_EV_IRQn, NVIC_EncodePriority(prioGrp, 1, 1));
  NVIC_EnableIRQ(I2C1_EV_IRQn);
  NVIC_SetPriority(I2C1_ER_IRQn, NVIC_EncodePriority(prioGrp, 1, 1));
  NVIC_EnableIRQ(I2C1_ER_IRQn);
#endif

#ifdef CONFIG_ADC
  NVIC_SetPriority(ADC_IRQn, NVIC_EncodePriority(prioGrp, 1, 0));
  NVIC_EnableIRQ(ADC_IRQn);
#endif

#ifdef CONFIG_SPI
  NVIC_SetPriority(DMA1_Stream3_IRQn, NVIC_EncodePriority(prioGrp, 1, 2)); // rx
  NVIC_SetPriority(DMA1_Stream4_IRQn, NVIC_EncodePriority(prioGrp, 1, 2)); // tx
  NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  NVIC_EnableIRQ(DMA1_Stream4_IRQn);
#endif

#ifdef CONFIG_SVIDEO_TEST
  // enable TIM5 interrupt, supadupahigh
  NVIC_SetPriority(TIM5_IRQn, NVIC_EncodePriority(prioGrp, 0, 0)); // make sure this is the only preemptprio 0
  NVIC_EnableIRQ(TIM5_IRQn);
#endif


  NVIC_SetPriority(EXTI0_IRQn, NVIC_EncodePriority(prioGrp, 3, 3));
  NVIC_SetPriority(EXTI1_IRQn, NVIC_EncodePriority(prioGrp, 3, 3));
  NVIC_SetPriority(EXTI2_IRQn, NVIC_EncodePriority(prioGrp, 3, 3));
  NVIC_SetPriority(EXTI3_IRQn, NVIC_EncodePriority(prioGrp, 3, 3));
  NVIC_SetPriority(EXTI4_IRQn, NVIC_EncodePriority(prioGrp, 3, 3));
  NVIC_SetPriority(EXTI9_5_IRQn, NVIC_EncodePriority(prioGrp, 3, 3));
  NVIC_SetPriority(EXTI15_10_IRQn, NVIC_EncodePriority(prioGrp, 3, 3));
}

static void dbgmcu_config(void)
{
  // keep jtag clock running during WFI instruction -> STANDBY_MODE
  DBGMCU_Config(DBGMCU_STANDBY, ENABLE);
  DBGMCU_Config(DBGMCU_SLEEP | DBGMCU_STOP, DISABLE);
}

static void sram_config() {
  FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
  FSMC_NORSRAMTimingInitTypeDef  p;

/*-- GPIOs Configuration -----------------------------------------------------*/
/*
 +-------------------+--------------------+------------------+------------------+
 | PD0  <-> FSMC_D2  | PE0  <-> FSMC_NBL0 | PF0 <-> FSMC_A0  | PG0 <-> FSMC_A10 |
 | PD1  <-> FSMC_D3  | PE1  <-> FSMC_NBL1 | PF1 <-> FSMC_A1  | PG1 <-> FSMC_A11 |
 | PD4  <-> FSMC_NOE | PE2  <-> FSMC_A23  | PF2 <-> FSMC_A2  | PG2 <-> FSMC_A12 |
 | PD5  <-> FSMC_NWE | PE3  <-> FSMC_A19  | PF3 <-> FSMC_A3  | PG3 <-> FSMC_A13 |
 | PD8  <-> FSMC_D13 | PE4  <-> FSMC_A20  | PF4 <-> FSMC_A4  | PG4 <-> FSMC_A14 |
 | PD9  <-> FSMC_D14 | PE5  <-> FSMC_A21  | PF5 <-> FSMC_A5  | PG5 <-> FSMC_A15 |
 | PD10 <-> FSMC_D15 | PE6  <-> FSMC_A22  | PF12 <-> FSMC_A6 | PG9 <-> FSMC_NE2 |
 | PD11 <-> FSMC_A16 | PE7  <-> FSMC_D4   | PF13 <-> FSMC_A7 |------------------+
 | PD12 <-> FSMC_A17 | PE8  <-> FSMC_D5   | PF14 <-> FSMC_A8 |
 | PD13 <-> FSMC_A18 | PE9  <-> FSMC_D6   | PF15 <-> FSMC_A9 |
 | PD14 <-> FSMC_D0  | PE10 <-> FSMC_D7   |------------------+
 | PD15 <-> FSMC_D1  | PE11 <-> FSMC_D8   |
 +-------------------| PE12 <-> FSMC_D9   |
                     | PE13 <-> FSMC_D10  |
                     | PE14 <-> FSMC_D11  |
                     | PE15 <-> FSMC_D12  |
                     +--------------------+
*/


  // AF12 = GPIO_AF_FSMC
  gpio_config(PORTD, PIN0, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN1, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN4, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN5, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN8, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN9, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN10, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN11, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN12, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN13, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN14, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTD, PIN15, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);

  gpio_config(PORTE, PIN0, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN1, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN2, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN3, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN4, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN5, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN6, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN7, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN8, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN9, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN10, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN11, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN12, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN13, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN14, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTE, PIN15, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);

  gpio_config(PORTF, PIN0, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTF, PIN1, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTF, PIN2, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTF, PIN3, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTF, PIN4, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTF, PIN5, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTF, PIN12, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTF, PIN13, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTF, PIN14, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTF, PIN15, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);

  gpio_config(PORTG, PIN0, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTG, PIN1, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTG, PIN2, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTG, PIN3, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTG, PIN4, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTG, PIN5, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);
  gpio_config(PORTG, PIN9, CLK_100MHZ, AF, AF12, PUSHPULL, NOPULL);

  // 1 / 168MHz ~= 6ns
  // Maximum FSMC_CLK frequency for synchronous accesses is 60 MHz.

  p.FSMC_AddressSetupTime = 0x3;      // 0x0..0xf
  p.FSMC_AddressHoldTime = 0x0;       // 0x0..0xf
  p.FSMC_DataSetupTime = 0x06;        // 0x00..0xff
  p.FSMC_BusTurnAroundDuration = 0x1; // 0x0..0xf

  p.FSMC_CLKDivision = 0;             // not used in sram
  p.FSMC_DataLatency = 0;             // not used in sram
  p.FSMC_AccessMode = FSMC_AccessMode_A;

  FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM2;
  FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
  FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
  FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
  FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
  FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
  FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
  FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
  FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
  FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p;

  FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);

  // Enable FSMC Bank1_SRAM1 Bank
  FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM2, ENABLE);
}

static void usb_config(void) {
#ifdef CONFIG_USB_VCD
  // AF10 = GPIO_AF_OTG_FS
  gpio_config_in(PORTA, PIN9, CLK_25MHZ);
  gpio_config(PORTA, PIN10, CLK_100MHZ, AF, AF10, PUSHPULL, NOPULL);
  gpio_config(PORTA, PIN11, CLK_100MHZ, AF, AF10, PUSHPULL, NOPULL);
  gpio_config(PORTA, PIN12, CLK_100MHZ, AF, AF10, PUSHPULL, NOPULL);
#endif
}

static void uart_config(void) {
#ifdef CONFIG_UART
#ifdef CONFIG_UART2
  // AF7 = GPIO_AF_USART2
  // tx
  gpio_config(PORTA, PIN2, CLK_50MHZ, AF, AF7, PUSHPULL, NOPULL);
  // rx
  gpio_config(PORTA, PIN3, CLK_50MHZ, AF, AF7, PUSHPULL, NOPULL);
#endif
#ifdef CONFIG_UART4
  // AF8 = GPIO_AF_UART4
  // tx
  gpio_config(PORTA, PIN0, CLK_50MHZ, AF, AF8, PUSHPULL, NOPULL);
  // rx
  gpio_config(PORTA, PIN1, CLK_50MHZ, AF, AF8, PUSHPULL, NOPULL);
#endif
#endif
}

static void i2c_config(void) {
#ifdef CONFIG_I2C
  // AF4 = GPIO_AF_I2C1
  // scl
  gpio_config(PORTB, PIN6, CLK_50MHZ, AF, AF4, OPENDRAIN, NOPULL);
  // sda
  gpio_config(PORTB, PIN7, CLK_50MHZ, AF, AF4, OPENDRAIN, NOPULL);
#endif
}

static void wifi_config(void) {
#ifdef CONFIG_WIFI232
  // reset - pg11
  gpio_config(PORTG, PIN11, CLK_2MHZ, OUT, AF0, PUSHPULL, PULLUP);
  // nlink - pg12
  gpio_config(PORTG, PIN12, CLK_2MHZ, IN, AF0, OPENDRAIN, NOPULL);
  // nready - pg13
  gpio_config(PORTG, PIN13, CLK_2MHZ, IN, AF0, OPENDRAIN, NOPULL);
  // nreload - pg14
  gpio_config(PORTG, PIN14, CLK_2MHZ, OUT, AF0, PUSHPULL, PULLUP);
#endif
}

static void tim_config(void) {
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

  u16_t prescaler = 0;

  // Time base configuration
  TIM_TimeBaseStructure.TIM_Period = SYS_CPU_FREQ/SYS_MAIN_TIMER_FREQ;
  TIM_TimeBaseStructure.TIM_Prescaler = 0;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(STM32_SYSTEM_TIMER, &TIM_TimeBaseStructure);

  // Prescaler configuration
  TIM_PrescalerConfig(STM32_SYSTEM_TIMER, prescaler, TIM_PSCReloadMode_Immediate);

  // TIM IT enable
  TIM_ITConfig(STM32_SYSTEM_TIMER, TIM_IT_Update, ENABLE);

  // TIM enable counter
  TIM_Cmd(STM32_SYSTEM_TIMER, ENABLE);
}

static void adc_config(void) {
#ifdef CONFIG_ADC
  ADC_InitTypeDef adc;

  // channel 0 / ADC12_IN6
  gpio_config(PORTA, PIN6, CLK_50MHZ, ANALOG, AF0, OPENDRAIN, NOPULL);
  // channel 1 / ADC12_IN7
  gpio_config(PORTA, PIN7, CLK_50MHZ, ANALOG, AF0, OPENDRAIN, NOPULL);
  // channel 2 / ADC12_IN8
  gpio_config(PORTB, PIN0, CLK_50MHZ, ANALOG, AF0, OPENDRAIN, NOPULL);

  adc.ADC_Resolution = ADC_Resolution_12b;
  adc.ADC_ScanConvMode = ENABLE;
  adc.ADC_ContinuousConvMode = DISABLE;
  adc.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
  adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
  adc.ADC_DataAlign = ADC_DataAlign_Right;
  adc.ADC_NbrOfConversion = 1;
  ADC_Init(ADC1, &adc);
  ADC_Init(ADC2, &adc);

  ADC_Cmd(ADC1, ENABLE);
  ADC_Cmd(ADC2, ENABLE);
#endif
}

static void spi_config(void) {
#ifdef CONFIG_SPI
  DMA_InitTypeDef  DMA_InitStructure;

  // AF5 = GPIO_AF_SPI2
  // spi2.clk   B13
  gpio_config(PORTB, PIN13, CLK_100MHZ, AF, AF5, PUSHPULL, NOPULL);
  // spi2.miso  B14
  gpio_config(PORTB, PIN14, CLK_100MHZ, AF, AF5, PUSHPULL, NOPULL);
  // spi2.mosi  B15
  gpio_config(PORTB, PIN15, CLK_100MHZ, AF, AF5, PUSHPULL, NOPULL);

  // Configure SPIA DMA common
  DMA_Cmd(DMA1_Stream3, DISABLE);
  DMA_Cmd(DMA1_Stream4, DISABLE);
  DMA_InitStructure.DMA_Channel = DMA_Channel_0;
  DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(SPI2->DR);
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
  DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
  DMA_InitStructure.DMA_Priority = DMA_Priority_High;
  DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
  DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
  DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
  DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
  // Configure SPIA DMA rx
  DMA_DeInit(DMA1_Stream3);
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
  DMA_InitStructure.DMA_BufferSize = 0;
  DMA_Init(DMA1_Stream3, &DMA_InitStructure);
  // Configure SPIA DMA tx
  DMA_DeInit(DMA1_Stream4);
  DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
  DMA_InitStructure.DMA_BufferSize = 0;
  DMA_Init(DMA1_Stream4, &DMA_InitStructure);

  // Enable DMA SPI RX channel transfer complete and errors interrupt
  DMA_ITConfig(DMA1_Stream3, DMA_IT_TC | DMA_IT_TE | DMA_IT_DME, ENABLE);
  // Disable DMA SPI TX channel transfer complete interrupt
  // Always use tx/rx transfers and only await DMA RX finished irq
  // When only txing, we receive into a dummy byte , no autoinc
  DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, DISABLE);
  // Enable DMA SPI TX channel errors interrupt
  DMA_ITConfig(DMA1_Stream4, DMA_IT_TE | DMA_IT_DME, ENABLE);

  // Enable SPI_MASTER DMA Rx/Tx request
  SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx , ENABLE);

#endif
}

// bootloader settings

static void SPI_config_bootloader() {
#ifdef CONFIG_SPI
  int i;
  for (i = 0; i < SPI_MAX_ID; i++) {
    // Abort all DMA transfers
    /* Disable DMA SPI RX channel transfer complete interrupt */
    DMA_ITConfig(_SPI_BUS(i)->dma_rx_stream, DMA_IT_TC | DMA_IT_TE | DMA_IT_DME, DISABLE);

    /* Disable SPI_MASTER DMA Rx/Tx request */
    SPI_I2S_DMACmd(_SPI_BUS(i)->hw, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx , DISABLE);
  }
#endif // CONFIG_SPI
}

#ifdef CONFIG_SVIDEO_TEST
static void svideo_test_config(void) {
  gpio_config(PORTA, PIN4, CLK_100MHZ, ANALOG, AF0, PUSHPULL, NOPULL);

  DAC_InitTypeDef DAC_InitStructure;
  DAC_InitStructure.DAC_Trigger = DAC_Trigger_None;
  DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
  DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
  DAC_Init(DAC_Channel_1, &DAC_InitStructure);
  DAC_Cmd(DAC_Channel_1, ENABLE);

  // Time base configuration
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  TIM_TimeBaseStructure.TIM_Period = SYS_CPU_FREQ/2000000-1;
  TIM_TimeBaseStructure.TIM_Prescaler = 0;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);
  // Prescaler configuration
  TIM_PrescalerConfig(TIM5, 0, TIM_PSCReloadMode_Immediate);

  // Instant update of period please
  TIM_ARRPreloadConfig(TIM5, DISABLE);

  // TIM IT enable
  TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);

  // TIM5 enable counter
  TIM_Cmd(TIM5, ENABLE);
}
#endif


void PROC_init() {
  rcc_config();
  nvic_config();
  dbgmcu_config();
  gpio_init();
  sram_config();
  usb_config();
  uart_config();
  i2c_config();
  tim_config();
  wifi_config();
  adc_config();
  spi_config();
#ifdef CONFIG_SVIDEO_TEST
  svideo_test_config();
#endif

  // this would be the led, yes?
  gpio_config(PORTF, PIN6, CLK_50MHZ, OUT, AF0, PUSHPULL, NOPULL);
}

void PROC_periph_init_bootloader() {
  SPI_config_bootloader();
}

