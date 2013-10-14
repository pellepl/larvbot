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

typedef enum {
  HEADER_METHOD = 0,
  HEADER_FIELDS,
  CONTENT,
  MULTI_CONTENT
} ws_state;

static struct {
  ringbuf tx_rb;
  u8_t tx_buf[SERVER_TX_MAX_LEN];

  ws_state state;

  ws_request_header req;
  ws_response res;

  char req_buf[SERVER_REQ_BUF_MAX_LEN];
  u16_t req_buf_len;
  u32_t content_len;
} server;

static char *space_strip(char *);

static void res_put(char *str) {
  ringbuf_put(&server.tx_rb, (u8_t*)str, strlen(str));
}

static ws_http_status server_get_content(ws_request_header *req) {
  res_put("<html>"
      "<head>"
      "<title>An Example Page</title>"
      "</head>"
      "<body>"
      "Hello World, this is a very simple HTML document.<br/>");

  res_put("got request: ");
  res_put(req->resource);

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

  return S200_OK;
}

static void server_clear_req(ws_request_header *req) {
  WIFI_IMPL_set_delim(WIFI_IMPL_DELIM_CHAR, '\n', 0, 0);
  memset(req, 0, sizeof(ws_request_header));
  server.state = HEADER_METHOD;
}

static void server_request(u8_t io, ws_request_header *req) {
  DBG(D_WEB, D_DEBUG, "WEB req method %s\n", WS_HTTP_REQ_METHODS[req->method]);
  DBG(D_WEB, D_DEBUG, "        res    %s\n", req->resource);
  DBG(D_WEB, D_DEBUG, "        host   %s\n", req->host);
  DBG(D_WEB, D_DEBUG, "        type   %s\n", req->content_type);
  DBG(D_WEB, D_DEBUG, "        length %i\n", req->content_length);
  DBG(D_WEB, D_DEBUG, "        conn   %s\n", req->connection);

  int http_res = server_get_content(req);

  ioprint(io,
  "HTTP/1.1 %i %s\n"
  "Server: "APP_NAME"\n"
  "Content-Type: text/html; charset=UTF-8\n"
  "Content-Length: %i\n"
  "Connection: close\n"
  "\n",
  WS_HTTP_STATUS_NUM[http_res], WS_HTTP_STATUS_STRING[http_res],
  ringbuf_available(&server.tx_rb));

  while (ringbuf_available(&server.tx_rb) > 0) {
    u8_t *data;
    u16_t len = ringbuf_available_linear(&server.tx_rb, &data);
    len = IO_put_buf(io, data, len);
    if (len < 0) break;
    //len = MIN(len, 64);
    ringbuf_get(&server.tx_rb, NULL, len);
  } // while tx

}

// handle HTTP header line
static void server_http_header(u8_t io, char *s, u16_t len, ringbuf* rb) {
  if (len == 0) {
    // end of HTTP header
    server_request(io, &server.req);
    if (server.req.content_length > 0) {
      // get content
      server.content_len = 0;
      server.state = CONTENT;
      WIFI_IMPL_set_delim(WIFI_IMPL_DELIM_LENGTH, 0, server.req.content_length, 0);
      DBG(D_WEB, D_DEBUG, "WEB: getting content length %i, available now %i\n", server.req.content_length, ringbuf_available(rb));
    } else {
      // back to expecting a http header
      server_clear_req(&server.req);
    }

    return;
  }

  // http header element
  switch (server.state) {
  case HEADER_METHOD: {
    u32_t i;
    for (i = 0; i < _REQ_METHOD_COUNT; i++) {
      if (strcmpbegin(WS_HTTP_REQ_METHODS[i], s) == 0) {
        server.req.method = i;
        char *resource = space_strip(&s[strlen(WS_HTTP_REQ_METHODS[i])]);
        char *space = (char *)strchr(resource, ' ');
        if (space) {
          *space = 0;
        }
        strcpy(server.req.resource, resource);
        break;
      }
    } // per method
    server.state = HEADER_FIELDS;
    break;
  }

  case HEADER_FIELDS: {
    u32_t i;
    for (i = 0; i < _FIELD_COUNT; i++) {
      if (strcmpbegin(WS_HTTP_FIELDS[i], s) == 0) {
        switch (i) {
        case FCONNECTION: {
          char *value = space_strip(&s[strlen(WS_HTTP_FIELDS[i])]);
          strcpy(server.req.connection, value);
          break;
        }
        case FHOST: {
          char *value = space_strip(&s[strlen(WS_HTTP_FIELDS[i])]);
          strcpy(server.req.host, value);
          break;
        }
        case FCONTENT_TYPE: {
          //Content-Type: multipart/form-data; boundary=----WebKitFormBoundarylA80LJLgp4DMpLj5
          char *value = space_strip(&s[strlen(WS_HTTP_FIELDS[i])]);
          strcpy(server.req.content_type, value);
          break;
        }
        case FCONTENT_LENGTH: {
          char *value = space_strip(&s[strlen(WS_HTTP_FIELDS[i])]);
          server.req.content_length = atoi(value);
          break;
        }
        } // switch field
        break;
      }
    } // per field

    break;
  }
  default:
    ASSERT(FALSE);
    break;
  }
}

// parse HTTP data
static void server_parse(u8_t io, ringbuf* rb) {
  s32_t rx;
  while ((rx = ringbuf_available(rb)) > 0) {
    switch (server.state) {
    case HEADER_METHOD:
    case HEADER_FIELDS: {
      u8_t c;
      s32_t res = ringbuf_getc(rb, &c);
      if (res == RB_ERR_EMPTY) return;
      if (c == '\r') continue;
      if (server.req_buf_len >= SERVER_REQ_BUF_MAX_LEN || c == '\n') {
        server.req_buf[server.req_buf_len] = 0;
        DBG(D_WEB, D_DEBUG, "WEB HTTP-HDR:%s\n", server.req_buf);
        server_http_header(io, server.req_buf, server.req_buf_len, rb);
        server.req_buf_len = 0;
      } else {
        server.req_buf[server.req_buf_len++] = c;
      }
      break;
    }
    case MULTI_CONTENT:
      // todo handle separately later
    case CONTENT: {
      s32_t avail = ringbuf_available(rb);
      s32_t len = MIN(avail, SERVER_REQ_BUF_MAX_LEN);
      len = MIN(len, server.req.content_length - server.content_len);
      len = ringbuf_get(rb, (u8_t *)server.req_buf, len);
      if (len <= 0) return;
#if 1
// dbg only
      server.req_buf[len] = 0;
      DBG(D_WEB, D_DEBUG, "WEB CONTENT:%s\n", server.req_buf);
#endif
      server.content_len += len;
      //DBG(D_WEB, D_DEBUG, "WEB: got %i bytes content of %i\n", server.content_len, server.req.content_length);
      if (server.content_len == server.req.content_length) {
        DBG(D_WEB, D_DEBUG, "WEB: all content received\n");
        server_clear_req(&server.req);
      }
      break;
    }
    } // switch state
  } // while rx avail
}

void SERVER_init() {
  memset(&server, 0, sizeof(server));
  ringbuf_init(&server.tx_rb, server.tx_buf, sizeof(server.tx_buf));
  WIFI_IMPL_set_delim(WIFI_IMPL_DELIM_CHAR, '\n', 0, 0);
  WIFI_IMPL_set_handler(server_parse);
}

static char *space_strip(char *s) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  return s;
}
