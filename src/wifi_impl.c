/*
 * wifi_impl.c
 *
 *  Created on: Sep 23, 2013
 *      Author: petera
 */

#include "wifi_impl.h"
#include "miniutils.h"

static union {
  wifi_ap ap;
  wifi_wan_setting wan;
  char ssid[64];
} wifi_arg;


static void wifi_impl_cb(wifi_cfg_cmd cmd, int res, u32_t arg, void *data) {
  if (res < WIFI_OK) {
    print("wifi err:%i\n", res);
    return;
  }

  switch (cmd) {
  case WIFI_SCAN: {
    if (res == WIFI_END_OF_SCAN) {
      print("  end of scan\n");
    } else {
      wifi_ap *ap = (wifi_ap *)data;
      print(
          "  ch%i\t%s\t[%s]\t%i%%\t%s\n",
          ap->channel,
          ap->ssid,
          ap->mac,
          ap->signal,
          ap->encryption);
    }
    break;
  }
  case WIFI_GET_WAN: {
    wifi_wan_setting *wan = (wifi_wan_setting *)data;
    print(
        "  %s  ip:%i.%i.%i.%i  netmask %i.%i.%i.%i  gateway %i.%i.%i.%i\n",
        wan->method == WIFI_WAN_STATIC ? "STATIC":"DHCP",
        wan->ip[0], wan->ip[1], wan->ip[2], wan->ip[3],
        wan->netmask[0], wan->netmask[1], wan->netmask[2], wan->netmask[3],
        wan->gateway[0], wan->ip[1], wan->gateway[2], wan->gateway[3]);
    break;
  }
  case WIFI_GET_SSID: {
    char *ssid= (char *)data;
    print("  %s\n", ssid);
    break;
  }
  default:
    break;
  } // switch
}

void WIFI_IMPL_init(void) {
  WIFI_init(wifi_impl_cb);
  print("UART wifi cb: %08x\n", _UART(WIFI_UART)->rx_f);
}

void WIFI_IMPL_state() {
  WIFI_state();
}

void WIFI_IMPL_reset(void) {
  WIFI_reset();
}

int WIFI_IMPL_scan() {
  int res = WIFI_scan(&wifi_arg.ap);
  return res;
}

int WIFI_IMPL_get_wan() {
  int res = WIFI_get_wan(&wifi_arg.wan);
  return res;
}

int WIFI_IMPL_get_ssid() {
  int res = WIFI_get_ssid(&wifi_arg.ssid[0]);
  return res;
}

