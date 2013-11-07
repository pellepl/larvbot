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
#include "os.h"
#include "adc_driver.h"

static void _pre_html(u8_t iobuf) {
  ioprint(iobuf,
      //"<!DOCTYPE HTML>" // PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n"
      "<html>"
      "<head>"
      "<title>"APP_NAME" server</title>"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"></link>"
      "</head>"
      "<body>"
      );
}

static void _post_html(u8_t iobuf) {
  ioprint(iobuf, "</body>"
      "</html>");
}

extern u8_t adc_buf[256*2];
extern volatile u16_t adc_ix;
static u16 _adc_ix;
#define SAMPLE_CHUNK_BUF    64

userver_response server_response(userver_request_header *req, u8_t iobuf, userver_http_status *http_status,
    char *content_type) {
  if (strcmp(req->resource, "/") == 0 || strcmp(req->resource, "/index.html") == 0) {
    _pre_html(iobuf);
    ioprint(iobuf, "<h1>"APP_NAME" server</h1>");
    ioprint(iobuf, "ticks:&nbsp;%08x<br/>", SYS_get_tick());
    ioprint(iobuf, "time:&nbsp;&nbsp;%i ms<br/>", SYS_get_time_ms());
    ioprint(iobuf, "<br/>");
    ioprint(iobuf, "<a href=\"/adc\">ADC</a><br/>");
    ioprint(iobuf, "<a href=\"/dump\">System dump</a><br/>");
    ioprint(iobuf, "<a href=\"/reset\" "
        "onClick=\"t=setTimeout(function(){ window.location='/';}, 3000); return true;\">Reset</a><br/>");
    _post_html(iobuf);
  }
  else if (strcmp(req->resource, "/style.css") == 0) {
    ioprint(iobuf,
        "a:link {color:#00ffff;}\n"
        "a:visited {color:#00aaaa;}\n"
        "a:hover {color:#80ffff;}\n"
        "a:active {color:#ffff0ff;}\n"
        "body {"
        "font-family: Courier, sans-serif;"
        "color: #80ff80;"
        "background-color: #000000;"
        "}\n"
        "button {"
        "font-family: Courier, sans-serif;"
        "font-weight: bold;"
        "color: #80ff80;"
        "background-color: #404040;"
        "border: 1px solid #808080;"
        "}\n"
        "input[type=\"range\"] {"
        "-webkit-appearance: none !important;"
        "color: #80ff80;"
        "background-color: #404040;"
        "border: 1px solid #808080;"
        "left: 10px;"
        "right: 10px;"
        "top: 10px;"
        "}\n"
        "h1 {font-family: Courier, sans-serif;}\n");
  }
  else if (strcmpbegin("/adc", req->resource) == 0) {
    char *sfreq;
    if ((sfreq = strstr(req->resource, "freq="))) {
      sfreq += 5;
      u32_t freq = atoi(sfreq);
      ADC_sample_continuous_stop();
      if (freq > 0) {
        ADC_sample_stereo_continuous(1, 2, freq);
      }
    }
    int channel_chunks = sizeof(adc_buf)/2/SAMPLE_CHUNK_BUF;
    if (req->chunk_nbr == 0) {
      _adc_ix = adc_ix;
      _pre_html(iobuf);
      ioprint(iobuf, "<h1>ADC</h1><br/>");
      ioprint(iobuf, "<canvas id=\"adccanvas\" width=\"512\" height=\"512\" style=\"border:1px solid #808080;\"></canvas>");
      ioprint(iobuf, "<script>\n"
                      "var c=document.getElementById(\"adccanvas\");\n"
                      "var ctx=c.getContext(\"2d\");\n"
                      "ctx.fillStyle=\"#000000\";\n"
                      "ctx.fillRect(0,0,512,512);\n"
                      "ctx.strokeStyle=\"rgba(255,255,255,0.5)\";\n"
                      "for (var i = 0; i < 512; i+=64) {\n"
                      "  ctx.moveTo(0,i);\n"
                      "  ctx.lineTo(512,i);\n"
                      "}\n"
                      "ctx.stroke();\n"
                      "var data1=[");
      return USERVER_CHUNKED;
    } else if ((req->chunk_nbr-1) < channel_chunks) {
      int sample_ix = (req->chunk_nbr-1) * SAMPLE_CHUNK_BUF;
      int i;
      for (i = sample_ix; i < sample_ix + SAMPLE_CHUNK_BUF; i ++) {
        if (i*2 + _adc_ix >= sizeof(adc_buf)) {
          ioprint(iobuf, "%i,", adc_buf[i*2 + _adc_ix - sizeof(adc_buf)]);
        } else {
          ioprint(iobuf, "%i,", adc_buf[i*2 + _adc_ix]);
        }
      }
      return USERVER_CHUNKED;
    } else if ((req->chunk_nbr-1) < 2*channel_chunks) {
      if ((req->chunk_nbr-1) == channel_chunks) {
        ioprint(iobuf, "];\n"
                       "var data2=[");
      }
      int sample_ix = (req->chunk_nbr-1-channel_chunks) * SAMPLE_CHUNK_BUF;
      int i;
      for (i = sample_ix; i < sample_ix + SAMPLE_CHUNK_BUF; i ++) {
        if (i*2 + _adc_ix + 1 >= sizeof(adc_buf)) {
          ioprint(iobuf, "%i,", adc_buf[i*2 + _adc_ix - sizeof(adc_buf) + 1]);
        } else {
          ioprint(iobuf, "%i,", adc_buf[i*2 + _adc_ix + 1]);
        }
      }
      return USERVER_CHUNKED;
    } else if ((req->chunk_nbr-1) == 2*sizeof(adc_buf)/2/SAMPLE_CHUNK_BUF) {
      ioprint(iobuf, "];\n"
          "ctx.strokeStyle=\"rgba(127,255,255,0.75)\";\n"
          "ctx.beginPath();\n"
          "ctx.moveTo(0,data1[0]);\n"
          "for (var i = 0; i < data1.length; i++) {\n"
          "  ctx.lineTo(i*2,data1[i]*2);\n"
          "}\n"
          "ctx.stroke();\n"

          "ctx.strokeStyle=\"rgba(255,255,127,0.75)\";\n"
          "ctx.beginPath();\n"
          "ctx.moveTo(0,data2[0]);\n"
          "for (var i = 0; i < data2.length; i++) {\n"
          "  ctx.lineTo(i*2,data2[i]*2);\n"
          "}\n"
          "ctx.stroke();\n"
          "</script>\n");
      ioprint(iobuf, "<br/>current frequency: %i Hz<br/>", ADC_get_freq()/1000);
      ioprint(iobuf,
          "<form name=\"adc\" method=\"GET\">"
          "<table border=0><tr>"
          "<td><button type=button onClick=\"window.location.reload()\">reload</button></td>"
          "<td><input type=\"range\" name=\"freq\" min=0 max=30000000></td>"
          "<td><button type=submit>sample</button></td>"
          "</tr></table></form>"
          );
      _post_html(iobuf);
      return USERVER_CHUNKED;
    } else {
      return USERVER_CHUNKED;
    }
  }
  else if (strcmp(req->resource, "/dump") == 0) {
    if (req->chunk_nbr == 0) {
      _pre_html(iobuf);
      ioprint(iobuf, "<h1>task dump</h1><pre>");
    } else if (req->chunk_nbr == 1) {
      TASK_dump(iobuf);
    } else if (req->chunk_nbr == 2) {
      ioprint(iobuf, "</pre><h1>thread dump</h1><pre>");
    } else if (req->chunk_nbr == 3) {
      OS_DBG_dump(iobuf);
    } else if (req->chunk_nbr == 4) {
      ioprint(iobuf, "</pre>");
      _post_html(iobuf);
    }
    return USERVER_CHUNKED;
  }
  else if (strcmp(req->resource, "/reset") == 0) {
    SYS_reboot(REBOOT_USER);
  }
  else {
    // unkown
    _pre_html(iobuf);
    ioprint(iobuf, "<h1>Error</h1>unknown request: %s<br/>", req->resource);
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
