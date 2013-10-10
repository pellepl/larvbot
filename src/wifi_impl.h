/*
 * wifi_impl.h
 *
 *  Created on: Sep 23, 2013
 *      Author: petera
 */

#ifndef WIFI_IMPL_H_
#define WIFI_IMPL_H_

#include "usr_wifi232_driver.h"
#include "ringbuf.h"

// handles data when specified character is found
#define WIFI_IMPL_DELIM_CHAR        (1<<0)
// handles data when specified length is received
#define WIFI_IMPL_DELIM_LENGTH      (1<<1)
// handles data when timeout occured
#define WIFI_IMPL_DELIM_TIME        (1<<2)

void WIFI_IMPL_init(void);
void WIFI_IMPL_set_handler(void (*data_handler_f)(u8_t io, ringbuf *rx_data_rb));
void WIFI_IMPL_set_delim(u8_t delim_mask,
    u8_t delim_char, u32_t delim_len, u32_t delim_ms);
void WIFI_IMPL_reset(bool hw);
void WIFI_IMPL_state(void);
int WIFI_IMPL_scan(void);
int WIFI_IMPL_get_wan(void);
int WIFI_IMPL_get_ssid(void);
int WIFI_IMPL_set_config(
    wifi_wan_method wan_method, u8_t wan_ip[4], u8_t wan_netmask[4], u8_t wan_gateway[4],
    u8_t gateway_ip[4],
    char *wifi_ssid, char *wifi_encryption, char *wifi_password,
    wifi_type ctrl_type, wifi_comm_protocol ctrl_protocol, u16_t ctrl_port, u8_t ctrl_server_ip[4]
    );

#endif /* WIFI_IMPL_H_ */
