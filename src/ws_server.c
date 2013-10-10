/*
 * ws_server.c
 *
 *  Created on: Oct 10, 2013
 *      Author: petera
 */

#include "system.h"
#include "ws_http.h"
#include "ringbuf.h"
#include "wifi_impl.h"
#include "miniutils.h"

static struct {
  ringbuf tx_rb;
  u8_t tx_buf[CONFIG_WIFI_TX_MAX_LEN];

  ws_request_header req;
  ws_response res;
} server;


static void res_put(char *str) {
  ringbuf_put(&server.tx_rb, (u8_t*)str, strlen(str));
}

static void server_get_content(char *req) {
  ringbuf_clear(&server.tx_rb);
  res_put("<html>"
      "<head>"
      "<title>An Example Page</title>"
      "</head>"
      "<body>"
      "Hello World, this is a very simple HTML document.<br/>");

  res_put("got request: ");
  res_put(req);

  res_put(
      "<form name=\"input\" action=\"posttest\" method=\"post\">"
      "Username: <input type=\"text\" name=\"user\">"
      "<input type=\"submit\" value=\"Submit\">"
      "<input type=\"reset\">"
      "</form>"
  );

  res_put(
  "<form enctype=\"multipart/form-data\" action=\"fileupload\" method=\"post\">"
  "<input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"1000000\" />"
  "Choose a file to upload: <input name=\"uploadedfile\" type=\"file\" /><br />"
  "<input type=\"submit\" value=\"Upload File\" />"
  "</form>"
  );

  res_put("</body>"
      "</html>");
}


static void server_handle(u8_t io, ringbuf* rb) {
  s32_t rx;
  while ((rx = ringbuf_available(rb)) > 0) {
    u8_t buf[256];
    s32_t got = ringbuf_get(rb, buf, MIN(sizeof(buf), rx));
    if (got <= 0) return;
    u32_t ix;
    for (ix = 0; ix < got; ix++) {
      if (buf[ix] == '\r') continue;
      if (buf[ix] == '\n') {
      }
    }

#if 0
    if (strncmp("GET ", (char*)buf, 4) == 0) {
      char *delim;
      if ((delim = (char *)strchr((char*)&buf[4], ' '))) {
        *delim = 0;
      }
      print("SERVER_RX: %s", buf);

      server_get_content((char *)&buf[4]);

      print("Serving %i data\n", ringbuf_available(&server.tx_rb));

      ioprint(io,
      "HTTP/1.1 200 OK\n"
      "Server: "APP_NAME"\n"
      "Content-Type: text/html; charset=UTF-8\n"
      "Content-Length: %i\n"
      "Connection: close\n"
      "\n", ringbuf_available(&server.tx_rb));

      while (ringbuf_available(&server.tx_rb) > 0) {
        u8_t *data;
        u16_t len = ringbuf_available_linear(&server.tx_rb, &data);
        len = IO_put_buf(io, data, len);
        if (len < 0) break;
        //len = MIN(len, 64);
        print("  >> sent %i bytes\n", len);
        ringbuf_get(&server.tx_rb, NULL, len);
      } // while tx
    } // if get
    else if (strncmp("POST ", (char*)buf, 5) == 0) {
      print("SERVER_RX: %s", buf);
      ioprint(io,
          "HTTP/1.1 404 Not Found\n"
          "Content-Length: 15\n"
          "Server: "APP_NAME"\n"
          "Connection: close\n"
          "\n"
          "Will not serve\n");
    } else {
      print("WIFI_RX: NOT PROCESSED %s", buf);
    }
#endif
  } // while rx avail
}

void SERVER_init() {
  memset(&server, 0, sizeof(server));
  ringbuf_init(&server.tx_rb, server.tx_buf, sizeof(server.tx_buf));
  WIFI_IMPL_set_delim(WIFI_IMPL_DELIM_CHAR, '\n', 0, 0);
  WIFI_IMPL_set_handler(server_handle);
}
