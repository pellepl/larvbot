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

const static char * const ERR_HTTP_TIMEOUT = "Request timed out\n";
const static char * const ERR_HTTP_BAD_REQUEST = "Bad request\n";

typedef enum {
  HEADER_METHOD = 0,
  HEADER_FIELDS,
  CONTENT,
  MULTI_CONTENT_HEADER,
  MULTI_CONTENT_DATA,
} ws_state;

static struct {
  ringbuf tx_rb;
  u8_t tx_buf[SERVER_TX_MAX_LEN];

  ws_state state;

  ws_request_header req;
  ws_response res;

  char *multipart_boundary;
  u8_t multipart_boundary_ix;
  u8_t multipart_boundary_len;

  char req_buf[SERVER_REQ_BUF_MAX_LEN+1];
  volatile u16_t req_buf_len;
  u32_t received_content_len;
} server;

static char *space_strip(char *);
static char *minus_strip(char *s);

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
  memset(req, 0, sizeof(ws_request_header));
  server.state = HEADER_METHOD;
}

// request error response
static void server_error(u8_t io, ws_http_status status, const char *error_page) {
  ringbuf_put(&server.tx_rb, (u8_t*)error_page, strlen(error_page));
  ioprint(io,
  "HTTP/1.1 %i %s\n"
  "Server: "APP_NAME"\n"
  "Content-Type: text/html; charset=UTF-8\n"
  "Content-Length: %i\n"
  "Connection: close\n"
  "\n",
  WS_HTTP_STATUS_NUM[status], WS_HTTP_STATUS_STRING[status],
  ringbuf_available(&server.tx_rb));

  while (ringbuf_available(&server.tx_rb) > 0) {
    u8_t *data;
    u16_t len = ringbuf_available_linear(&server.tx_rb, &data);
    len = IO_put_buf(io, data, len);
    if (len < 0) break;
    //len = MIN(len, 64);
    ringbuf_get(&server.tx_rb, NULL, len);
  } // while tx

  server_clear_req(&server.req);
}


// serve a request and send answer
static void server_request(u8_t io, ws_request_header *req) {
  DBG(D_WEB, D_DEBUG, "WEB req method %s\n", WS_HTTP_REQ_METHODS[req->method]);
  DBG(D_WEB, D_DEBUG, "        res    %s\n", req->resource);
  DBG(D_WEB, D_DEBUG, "        host   %s\n", req->host);
  DBG(D_WEB, D_DEBUG, "        type   %s\n", req->content_type);
  DBG(D_WEB, D_DEBUG, "        length %i\n", req->content_length);
  DBG(D_WEB, D_DEBUG, "        conn   %s\n", req->connection);

  if (req->method == _BAD_REQ) {
    DBG(D_WEB, D_WARN, "WEB: BAD REQUEST\n");
    server_error(io, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
    return;
  }

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

static void server_find_boundary(char *b, u16_t len) {
  u16_t i;
  for (i = 0; i < len; i++) {
    if (b[i] == server.multipart_boundary[server.multipart_boundary_ix]) {
      server.multipart_boundary_ix++;
      if (server.multipart_boundary_ix == server.multipart_boundary_len) {
        DBG(D_WEB, D_DEBUG, "WEB: content boundary start @ %i\n", i);
        server.multipart_boundary_ix = 0;
      }
    } else {
      server.multipart_boundary_ix = 0;
    }
  }
}

// handle HTTP header line
static void server_http_header(u8_t io, char *s, u16_t len, ringbuf* rb) {
  if (len > 0) {
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
  else // if (len == 0)
  {
    // end of HTTP header

    // serve request
    server_request(io, &server.req);

    // expecting data?
    if (server.req.content_length > 0) {
      // get content
      server.received_content_len = 0;
      server.state = CONTENT;
      DBG(D_WEB, D_DEBUG, "WEB: getting content length %i, available now %i\n", server.req.content_length, ringbuf_available(rb));

      // multipart?
      if (strcmpbegin("multipart/form-data", server.req.content_type) == 0) {
        // get boundary string
        char *boundary_start = strstr(server.req.content_type, "boundary");
        if (boundary_start == 0) {
          DBG(D_WEB, D_WARN, "WEB: BAD MULTIPART REQUEST, boundary not found\n");
          server_error(io, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
          return;
        }
        boundary_start += 8; // "boundary"
        boundary_start = space_strip(boundary_start);
        if (*boundary_start != '=') {
          DBG(D_WEB, D_WARN, "WEB: BAD MULTIPART REQUEST, = not found\n");
          server_error(io, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
          return;
        }
        boundary_start++;
        boundary_start = space_strip(boundary_start);
        if (strlen(boundary_start) == 0) {
          DBG(D_WEB, D_WARN, "WEB: BAD MULTIPART REQUEST, boundary id not found\n");
          server_error(io, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
          return;
        }
        boundary_start = minus_strip(boundary_start);
        server.multipart_boundary = boundary_start;
        server.multipart_boundary_ix = 0;
        server.multipart_boundary_len = strlen(boundary_start);
        server.state = MULTI_CONTENT_HEADER;
        DBG(D_WEB, D_DEBUG, "WEB: boundary start: %s\n", boundary_start);
      }
    } else {
      // back to expecting a http header
      server_clear_req(&server.req);
    }

    return;
  }
}

// handle multipart content header line
static void server_multi_content_header(u8_t io, char *s, u16_t len, ringbuf* rb) {
  char *boundary_start;
  if (strcmpbegin("--", s) == 0 && (boundary_start = strstr(s, server.multipart_boundary))) {
    if (strstr(boundary_start, "--")) {
      // end of multipart message
      // back to expecting a http header
      DBG(D_WEB, D_DEBUG, "WEB: multipart finished\n");
      server_clear_req(&server.req);
    } else {
      // multipart part
      DBG(D_WEB, D_DEBUG, "WEB: multipart section\n");
    }
  } else if (len == 0) {
    // TODO server.state = MULTI_CONTENT_DATA;
  }
}

// HTTP data timeout
static void server_timeout(u8_t io) {
  if (server.state != HEADER_METHOD) {
    server_error(io, S408_REQUEST_TIMEOUT, ERR_HTTP_TIMEOUT);
  }
}

// parse HTTP data
static void server_parse(u8_t io, ringbuf* rb) {
  s32_t rx;
  while ((rx = ringbuf_available(rb)) > 0) {
    switch (server.state) {
    case MULTI_CONTENT_HEADER:
    case HEADER_METHOD:
    case HEADER_FIELDS: {
      u8_t c;
      s32_t res = ringbuf_getc(rb, &c);
      if (res == RB_ERR_EMPTY) {
        return;
      }

      if (c == '\r') continue;
      if (server.req_buf_len >= SERVER_REQ_BUF_MAX_LEN || c == '\n') {
        if (server.req_buf_len >= SERVER_REQ_BUF_MAX_LEN) {
          server.req_buf[SERVER_REQ_BUF_MAX_LEN] = 0;
        } else {
          server.req_buf[server.req_buf_len] = 0;
        }
        if (server.state != MULTI_CONTENT_HEADER) {
          DBG(D_WEB, D_DEBUG, "WEB HTTP-HDR: %s\n", server.req_buf);
          server_http_header(io, server.req_buf, server.req_buf_len, rb);
        } else {
          DBG(D_WEB, D_DEBUG, "WEB MULTI-HDR:%s\n", server.req_buf);
          server_multi_content_header(io, server.req_buf, server.req_buf_len, rb);
        }
        server.req_buf_len = 0;
      }

      if (c != '\n') {
        server.req_buf[server.req_buf_len++] = c;
      }
      break;
    }
    case CONTENT:
      // todo handle separately later
    case MULTI_CONTENT_DATA: {
      s32_t len = MIN(rx, SERVER_REQ_BUF_MAX_LEN);
      len = MIN(len, server.req.content_length - server.received_content_len);
      len = ringbuf_get(rb, (u8_t *)server.req_buf, len);
      if (len <= 0) return;
#if 1
// dbg only
      server.req_buf[len] = 0;
      //DBG(D_WEB, D_DEBUG, "WEB CONTENT:%s\n", server.req_buf);
      printbuf(IOSTD, server.req_buf, len);
#endif
      if (server.state == MULTI_CONTENT_DATA) {
        server_find_boundary(server.req_buf, len);
      }
      server.received_content_len += len;
      //DBG(D_WEB, D_DEBUG, "WEB: got %i bytes content of %i\n", server.content_len, server.req.content_length);
      if (server.received_content_len == server.req.content_length) {
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
  WIFI_IMPL_set_data_handler(server_parse);
  WIFI_IMPL_set_data_tmo_handler(server_timeout);
}

static char *space_strip(char *s) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  return s;
}
static char *minus_strip(char *s) {
  while (*s == '-') {
    s++;
  }
  return s;
}
