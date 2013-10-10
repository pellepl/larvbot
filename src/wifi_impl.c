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
  ringbuf rx_rb;
  u8_t rx_buf[CONFIG_WIFI_RX_MAX_LEN];

  volatile bool handling_input;
  void (*data_handler_f)(u8_t io, ringbuf *rx_data_rb);

  u8_t delim_mask;
  u16_t delim_char;
  u32_t delim_length;
  u32_t rec;
} _wi;


static void wifi_impl_task_f(u32_t io, void* vrb) {
  _wi.handling_input = FALSE;
  if (_wi.data_handler_f) _wi.data_handler_f(io, (ringbuf *)vrb);
}

static void wifi_impl_data_cb(u8_t io, u16_t len) {
  int i;
  for (i = 0; i < len; i++) {
    s32_t c = IO_get_char(io);
    if (c == -1) return;
    _wi.rec++;
    s32_t res = ringbuf_putc(&_wi.rx_rb, c);
    if (res == RB_OK || res == RB_ERR_FULL) {
      bool handle = !_wi.handling_input && (
          res == RB_ERR_FULL ||
          (c == _wi.delim_char) ||
          ((_wi.delim_mask & WIFI_IMPL_DELIM_LENGTH) && _wi.rec >= _wi.delim_length));
      if (handle) {
        _wi.handling_input = TRUE;
        task *t = TASK_create(wifi_impl_task_f, 0);
        ASSERT(t);
        TASK_run(t, IOWIFI, &_wi.rx_rb);
      }
    }
  }
}

static void wifi_impl_cb(wifi_cfg_cmd cmd, int res, u32_t arg, void *data) {
  // TODO: copy and report data struct back via task message
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
  memset(&_wi, 0, sizeof(_wi));
  ringbuf_init(&_wi.rx_rb, _wi.rx_buf, CONFIG_WIFI_RX_MAX_LEN);
  _wi.delim_mask = WIFI_IMPL_DELIM_LENGTH;
  _wi.delim_length = 1;
  WIFI_init(wifi_impl_cb, wifi_impl_data_cb);
}

void WIFI_IMPL_set_delim(u8_t delim_mask,
    u8_t delim_char, u32_t delim_len, u32_t delim_ms) {
  _wi.delim_mask = delim_mask;

  if (delim_mask && WIFI_IMPL_DELIM_CHAR) {
    _wi.delim_char = delim_char;
  } else {
    _wi.delim_char = 0xff00;
  }
  if (delim_mask && WIFI_IMPL_DELIM_LENGTH) {
    _wi.delim_length = delim_len;
    _wi.rec = ringbuf_available(&_wi.rx_rb);

    // got enough data for callback already?
    if (_wi.rec >= delim_len) {
      if (!_wi.handling_input) {
        _wi.handling_input = TRUE;
        task *t = TASK_create(wifi_impl_task_f, 0);
        ASSERT(t);
        TASK_run(t, IOWIFI, &_wi.rx_rb);
      }
    }
  }
  // TODO millisecond delimiter
}

void WIFI_IMPL_set_handler(void (*data_handler_f)(u8_t io, ringbuf *rx_data_rb)) {
  _wi.data_handler_f = data_handler_f;
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
