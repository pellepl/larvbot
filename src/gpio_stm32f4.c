/*
 * gpio_stm32f4.c
 *
 *  Created on: Sep 9, 2013
 *      Author: petera
 */

#include "gpio.h"

const GPIO_TypeDef *io_ports[] = {
    GPIOA,
    GPIOB,
    GPIOC,
    GPIOD,
    GPIOE,
    GPIOF,
    GPIOG,
    GPIOH,
    GPIOI
};

const u32_t io_rcc[] = {
    RCC_AHB1Periph_GPIOA,
    RCC_AHB1Periph_GPIOB,
    RCC_AHB1Periph_GPIOC,
    RCC_AHB1Periph_GPIOD,
    RCC_AHB1Periph_GPIOE,
    RCC_AHB1Periph_GPIOF,
    RCC_AHB1Periph_GPIOG,
    RCC_AHB1Periph_GPIOH,
    RCC_AHB1Periph_GPIOI
};

const u8_t io_pinsources[] = {
    GPIO_PinSource0,
    GPIO_PinSource1,
    GPIO_PinSource2,
    GPIO_PinSource3,
    GPIO_PinSource4,
    GPIO_PinSource5,
    GPIO_PinSource6,
    GPIO_PinSource7,
    GPIO_PinSource8,
    GPIO_PinSource9,
    GPIO_PinSource10,
    GPIO_PinSource11,
    GPIO_PinSource12,
    GPIO_PinSource13,
    GPIO_PinSource14,
    GPIO_PinSource15,
};

const u16_t io_pins[] = {
    GPIO_Pin_0,
    GPIO_Pin_1,
    GPIO_Pin_2,
    GPIO_Pin_3,
    GPIO_Pin_4,
    GPIO_Pin_5,
    GPIO_Pin_6,
    GPIO_Pin_7,
    GPIO_Pin_8,
    GPIO_Pin_9,
    GPIO_Pin_10,
    GPIO_Pin_11,
    GPIO_Pin_12,
    GPIO_Pin_13,
    GPIO_Pin_14,
    GPIO_Pin_15,
};

const u8_t io_afs[] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
};

const GPIOSpeed_TypeDef io_speeds[] = {
    GPIO_Speed_2MHz,
    GPIO_Speed_25MHz,
    GPIO_Speed_50MHz,
    GPIO_Speed_100MHz
};

const GPIOMode_TypeDef io_modes[] = {
    GPIO_Mode_IN,
    GPIO_Mode_OUT,
    GPIO_Mode_AF,
    GPIO_Mode_AN,
};

const GPIOOType_TypeDef io_outtypes[] = {
    GPIO_OType_PP,
    GPIO_OType_OD,
};

const GPIOPuPd_TypeDef io_pulls[] = {
    GPIO_PuPd_NOPULL,
    GPIO_PuPd_UP,
    GPIO_PuPd_DOWN
};

static u32_t enabled_pins[_IO_PORTS];

static void io_enable_pin(gpio_port port, gpio_pin pin) {
  if (enabled_pins[port] == 0) {
    // first pin enabled on port, start port clock
    RCC_AHB1PeriphClockCmd(io_rcc[port], ENABLE);
  }
  enabled_pins[port] |= (1<<pin);
}

static void io_disable_pin(gpio_port port, gpio_pin pin) {
  enabled_pins[port] &= ~(1<<pin);
  if (enabled_pins[port] == 0) {
    // all pins disabled on port, stop port clock
    RCC_AHB1PeriphClockCmd(io_rcc[port], DISABLE);
  }
}

static void io_setup(gpio_port port, gpio_pin pin, io_speed speed, gpio_mode mode, gpio_af af, gpio_outtype outtype, gpio_pull pull) {
  GPIO_InitTypeDef hw;
  if (mode == AF) {
    GPIO_PinAFConfig((GPIO_TypeDef *)io_ports[port], io_pinsources[pin], io_afs[af]);
  }
  hw.GPIO_Pin = io_pins[pin];
  hw.GPIO_Mode = io_modes[mode];
  hw.GPIO_Speed = io_speeds[speed];
  hw.GPIO_OType = io_outtypes[outtype];
  hw.GPIO_PuPd  = io_pulls[pull];
  GPIO_Init((GPIO_TypeDef *)io_ports[port], &hw);
}

void gpio_config(gpio_port port, gpio_pin pin, io_speed speed, gpio_mode mode, gpio_af af, gpio_outtype outtype, gpio_pull pull) {
  io_enable_pin(port, pin);
  io_setup(port, pin, speed, mode, af, outtype, pull);
}
void gpio_config_out(gpio_port port, gpio_pin pin, io_speed speed, gpio_outtype outtype, gpio_pull pull) {
  io_enable_pin(port, pin);
  io_setup(port, pin, speed, OUT, AF0, outtype, pull);
}
void gpio_config_in(gpio_port port, gpio_pin pin, io_speed speed) {
  io_enable_pin(port, pin);
  io_setup(port, pin, speed, IN, AF0, PUSHPULL, NOPULL);
}
void gpio_config_analog(gpio_port port, gpio_pin pin) {
  io_enable_pin(port, pin);
  io_setup(port, pin, CLK_100MHZ, IN, AF0, PUSHPULL, NOPULL);
}
void gpio_release(gpio_port port, gpio_pin pin) {
  io_disable_pin(port, pin);
}
void gpio_init(void) {
  memset(enabled_pins, 0, sizeof(enabled_pins));
}


