/*
 * wifi_impl.c
 *
 *  Created on: Sep 23, 2013
 *      Author: petera
 */

#include "wifi_impl.h"
#include "miniutils.h"
#include "ringbuf.h"
#include "taskq.h"

static union {
  wifi_ap ap;
  wifi_wan_setting wan;
  char ssid[64];
  wifi_config config;
} wifi_arg;

static struct {
  void (*data_handler_f)(u8_t io, ringbuf *rx_data_rb);
  void (*data_tmo_handler_f)(u8_t io);

  bool idle;
} wi;


static void wifi_impl_cfg_cb(wifi_cfg_cmd cmd, int res, u32_t arg, void *argp) {
  // TODO: copy and report data struct back via task message
  if (wi.idle) return;
  if (res < WIFI_OK) {
    print("wifi err:%i\n", res);
    return;
  }

  switch (cmd) {
  case WIFI_SCAN: {
    if (res == WIFI_END_OF_SCAN) {
      print("  end of scan\n");
    } else {
      wifi_ap *ap = (wifi_ap *)argp;
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
    wifi_wan_setting *wan = (wifi_wan_setting *)argp;
    print(
        "  %s  ip:%i.%i.%i.%i  netmask %i.%i.%i.%i  gateway %i.%i.%i.%i\n",
        wan->method == WIFI_WAN_STATIC ? "STATIC":"DHCP",
        wan->ip[0], wan->ip[1], wan->ip[2], wan->ip[3],
        wan->netmask[0], wan->netmask[1], wan->netmask[2], wan->netmask[3],
        wan->gateway[0], wan->ip[1], wan->gateway[2], wan->gateway[3]);
    break;
  }
  case WIFI_GET_SSID: {
    char *ssid= (char *)argp;
    print("  %s\n", ssid);
    break;
  }
  default:
    break;
  } // switch
}

static void wifi_impl_data_cb(u8_t io_out, ringbuf *rb_in) {
  if (wi.data_handler_f) wi.data_handler_f(io_out, rb_in);
}

static void wifi_impl_data_silence_cb(u8_t io_out) {
  if (wi.data_tmo_handler_f) wi.data_tmo_handler_f(io_out);
}

void WIFI_IMPL_init(void) {
  memset(&wi, 0, sizeof(wi));
  WIFI_init(wifi_impl_cfg_cb, wifi_impl_data_cb, wifi_impl_data_silence_cb);
  WIFI_set_data_delimiter(WIFI_DELIM_CHAR | WIFI_DELIM_LENGTH | WIFI_DELIM_TIME,
      '\n', SERVER_REQ_BUF_MAX_LEN/2, 200);
  WIFI_set_data_silence_timeout(2000);
}

void WIFI_IMPL_set_data_handler(void (*data_handler_f)(u8_t io, ringbuf *rx_data_rb)) {
  wi.data_handler_f = data_handler_f;
}

void WIFI_IMPL_set_data_tmo_handler(void (*data_tmo_handler_f)(u8_t io)) {
  wi.data_tmo_handler_f = data_tmo_handler_f;
}

void WIFI_IMPL_set_idle(bool idle) {
  wi.idle = idle;
}

void WIFI_IMPL_state() {
  WIFI_state();
}

void WIFI_IMPL_reset(bool hw) {
  WIFI_reset(hw);
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

int WIFI_IMPL_set_config(
    wifi_wan_method wan_method, u8_t wan_ip[4], u8_t wan_netmask[4], u8_t wan_gateway[4],
    u8_t gateway_ip[4],
    char *wifi_ssid, char *wifi_encryption, char *wifi_password,
    wifi_type ctrl_type, wifi_comm_protocol ctrl_protocol, u16_t ctrl_port, u8_t ctrl_server_ip[4]
    ) {
  wifi_arg.wan.method = wan_method;
  memcpy(wifi_arg.config.wan.ip, wan_ip, 4);
  memcpy(wifi_arg.config.wan.netmask, wan_netmask, 4);
  memcpy(wifi_arg.config.wan.gateway, wan_gateway, 4);

  memcpy(wifi_arg.config.gateway, gateway_ip, 4);

  strcpy(wifi_arg.config.ssid, wifi_ssid);
  strcpy(wifi_arg.config.encryption, wifi_encryption);
  strcpy(wifi_arg.config.password, wifi_password);

  wifi_arg.config.type = ctrl_type;
  wifi_arg.config.protocol = ctrl_protocol;
  wifi_arg.config.port = ctrl_port;
  memcpy(wifi_arg.config.server, ctrl_server_ip, 4);

  int res = WIFI_set_config(&wifi_arg.config);
  return res;
}
