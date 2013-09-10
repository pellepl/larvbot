/*
 * gpio.h
 *
 *  Created on: Sep 9, 2013
 *      Author: petera
 */

#ifndef GPIO_H_
#define GPIO_H_

#include "system.h"

typedef enum {
  PORTA = 0,
  PORTB,
  PORTC,
  PORTD,
  PORTE,
  PORTF,
  PORTG,
  PORTH,
  PORTI,
  _IO_PORTS
} io_port;

typedef enum {
  PIN0 = 0,
  PIN1,
  PIN2,
  PIN3,
  PIN4,
  PIN5,
  PIN6,
  PIN7,
  PIN8,
  PIN9,
  PIN10,
  PIN11,
  PIN12,
  PIN13,
  PIN14,
  PIN15,
  PIN16,
  _IO_PINS
} io_pin;


typedef enum {
  AF0 = 0,
  AF1,
  AF2,
  AF3,
  AF4,
  AF5,
  AF6,
  AF7,
  AF8,
  AF9,
  AF10,
  AF11,
  AF12,
  AF13,
  AF14,
  AF15,
  AF16,
  _AFS
} io_af;


typedef enum {
  IO_2MHZ = 0,
  IO_25MHZ,
  IO_50MHZ,
  IO_100MHZ,
  _IO_SPEEDS
} io_speed;


typedef enum {
  IN = 0,
  OUT,
  AF,
  ANALOG,
  _IO_MODES
} io_mode;


typedef enum {
  PUSHPULL = 0,
  OPENDRAIN,
  _IO_OUTTYPES
} io_outtype;

typedef enum {
  NOPULL= 0,
  PULLUP,
  PULLDOWN,
  _IO_PULLS
} io_pull;

void io_init(void);
void io_config(io_port port, io_pin pin, io_speed speed, io_mode mode, io_af af, io_outtype outtype, io_pull pull);
void io_config_out(io_port port, io_pin pin, io_speed speed, io_outtype outtype, io_pull pull);
void io_config_in(io_port port, io_pin pin, io_speed speed);
void io_config_analog(io_port port, io_pin pin);
void io_release(io_port port, io_pin pin);

#endif /* GPIO_H_ */
