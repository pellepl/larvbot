/*
 * server_impl.c
 *
 *  Created on: Oct 18, 2013
 *      Author: petera
 */

#ifndef SERVER_IMPL_C_
#define SERVER_IMPL_C_

#include "server_impl.h"
#include "io.h"
#include "miniutils.h"
#include "wifi_impl.h"

static void _pre_html(u8_t iobuf) {
  ioprint(iobuf, "<html>"
      "<head>"
      "<title>"APP_NAME" Server</title>"
      "</head>"
      "<body>"
      );
}

static void _post_html(u8_t iobuf) {
  ioprint(iobuf, "</body>"
      "</html>");
}

extern u8_t adc_buf[256];

userver_response server_response(userver_request_header *req, u8_t iobuf, userver_http_status *http_status,
    char *content_type) {
  if (strcmp(req->resource, "/adc") == 0) {
    if (req->chunk_nbr == 0) {
      _pre_html(iobuf);
      ioprint(iobuf, "ADC measures<br/>");
      ioprint(iobuf, "<canvas id=\"adccanvas\" width=\"512\" height=\"256\" style=\"border:1px solid #000000;\"></canvas>");
      ioprint(iobuf, "<script>\n"
                      "var c=document.getElementById(\"adccanvas\");\n"
                      "var ctx=c.getContext(\"2d\");\n"
                      "ctx.fillStyle=\"#000000\";\n"
                      "ctx.fillRect(0,0,512,256);\n"
                      "ctx.strokeStyle=\"#00ff00\";\n"
                      "ctx.beginPath();\n"
                      "ctx.moveTo(0,%i);\n", adc_buf[0]);
      return USERVER_CHUNKED;
    } else if ((req->chunk_nbr-1) < 256/32) {
      int sample = (req->chunk_nbr-1) * 16;
      int i;
      for (i = sample; i < sample + 16; i ++) {
        //ioprint(iobuf, "%i\t%02x\t%02x<br/>\n", i, adc_buf[i*2], adc_buf[i*2+1]);
        ioprint(iobuf, "ctx.lineTo(%i,%i);\n",
                       i*4, adc_buf[i*2]);
      }
      return USERVER_CHUNKED;
    } if ((req->chunk_nbr-1) == 256/32) {
      ioprint(iobuf, "ctx.stroke();\n"
                     "</script>\n");
      _post_html(iobuf);
      return USERVER_CHUNKED;
    } else {
      return USERVER_CHUNKED;
    }
  } else {
    _pre_html(iobuf);
    ioprint(iobuf, "Unknown request: %s<br/>", req->resource);
    _post_html(iobuf);
  }
  return USERVER_OK;
}

void server_data(userver_request_header *req, userver_data_type type, u32_t offset, u8_t *data, u32_t length) {
  switch(type) {
  case DATA_CONTENT:
    print("SERVER_DATA CONTENT offs:%i len:%i\n", offset, length);
    break;
  case DATA_CHUNK:
    print("SERVER_DATA CHUNK #%i offs:%i len:%i\n", req->chunk_nbr, offset, length);
    break;
  case DATA_MULTIPART:
    print("SERVER_DATA MULTI #%i offs:%i len:%i [%s]\n", req->cur_multipart.multipart_nbr, offset, length,
        req->cur_multipart.content_disp);
    break;
  }
  printbuf(IOSTD, data, length);
}


void SERVER_LARV_init() {
  USERVER_init(server_response, server_data);
  WIFI_IMPL_set_data_handler(USERVER_parse);
  WIFI_IMPL_set_data_tmo_handler(USERVER_timeout);
  WIFI_set_data_delimiter(WIFI_DELIM_CHAR | WIFI_DELIM_LENGTH | WIFI_DELIM_TIME,
      '\n', USERVER_REQ_BUF_MAX_LEN/2, 200);
  WIFI_set_data_silence_timeout(2000);

}

#endif /* SERVER_IMPL_C_ */
