/*
 * wifi_impl.h
 *
 *  Created on: Sep 23, 2013
 *      Author: petera
 */

#ifndef WIFI_IMPL_H_
#define WIFI_IMPL_H_

#include "usr_wifi232_driver.h"

void WIFI_IMPL_init(void);
void WIFI_IMPL_reset(void);
void WIFI_IMPL_state(void);
int WIFI_IMPL_scan(void);
int WIFI_IMPL_get_wan(void);
int WIFI_IMPL_get_ssid(void);


#endif /* WIFI_IMPL_H_ */
