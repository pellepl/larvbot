/*
 * userver.c
 *
 * Petite web server implementation
 * - supports multipart and chunked transfers
 *
 *  Created on: Oct 10, 2013
 *      Author: petera
 */

#include "system.h"
#include "userver.h"
#include "ringbuf.h"
#include "miniutils.h"

const static char * const ERR_HTTP_TIMEOUT = "Request timed out\n";
const static char * const ERR_HTTP_BAD_REQUEST = "Bad request\n";
const static char * const ERR_HTTP_NOT_IMPL = "Not implemented\n";

typedef enum {
  HEADER_METHOD = 0,
  HEADER_FIELDS,
  CONTENT,
  MULTI_CONTENT_HEADER,
  MULTI_CONTENT_DATA,
  CHUNK_DATA_HEADER,
  CHUNK_DATA,
  CHUNK_DATA_END,
  CHUNK_FOOTER,
} us_state;

static struct {
  userver_response_f server_resp_f;
  userver_data_f server_data_f;

  ringbuf tx_rb;
  u8_t tx_buf[USERVER_TX_MAX_LEN];

  us_state state;

  userver_request_header req;

  u16_t header_line;

  char *multipart_boundary;
  u8_t multipart_boundary_ix;
  u8_t multipart_boundary_len;
  u8_t multipart_delim;
  u32_t received_multipart_len;

  char req_buf[USERVER_REQ_BUF_MAX_LEN+1];
  volatile u16_t req_buf_len;

  u32_t chunk_ix;
  u32_t chunk_len;
  u32_t received_content_len;
} userv;

static char *space_strip(char *);

// clear incoming request and reset server states
static void server_clear_req(userver_request_header *req) {
  memset(req, 0, sizeof(userver_request_header));
  userv.state = HEADER_METHOD;
  userv.header_line = 0;
}

// send data to client
static void server_send_data(u8_t ioout) {
  while (ringbuf_available(&userv.tx_rb) > 0) {
    u8_t *data;
    u16_t len = ringbuf_available_linear(&userv.tx_rb, &data); // this is IOWEB
    len = IO_put_buf(ioout, data, len);
    if (len < 0) break;
    //len = MIN(len, 64);
    ringbuf_get(&userv.tx_rb, NULL, len);
  } // while tx
}

// request error response
static void server_error(u8_t ioout, userver_http_status http_status, const char *error_page) {
  ioprint(IOWEB, error_page);
  ioprint(ioout,
    "HTTP/1.1 %i %s\n"
    "Server: "APP_NAME"\n"
    "Content-Type: text/html; charset=UTF-8\n"
    "Content-Length: %i\n"
    "Connection: close\n"
    "\n",
    USERVER_HTTP_STATUS_NUM[http_status], USERVER_HTTP_STATUS_STRING[http_status],
    ringbuf_available(&userv.tx_rb));
  server_send_data(ioout);
  server_clear_req(&userv.req);
  userv.chunk_ix = 0;
  userv.chunk_len = 0;
}

// serve a request and send answer
static void server_request(u8_t ioout, userver_request_header *req) {
  DBG(D_WEB, D_DEBUG, "WEB req method %s\n", USERVER_HTTP_REQ_METHODS[req->method]);
  DBG(D_WEB, D_DEBUG, "        res    %s\n", req->resource);
  DBG(D_WEB, D_DEBUG, "        host   %s\n", req->host);
  DBG(D_WEB, D_DEBUG, "        type   %s\n", req->content_type);
  if (req->chunked) {
    DBG(D_WEB, D_DEBUG, "        chunked\n");
  } else {
    DBG(D_WEB, D_DEBUG, "        length %i\n", req->content_length);
  }
  DBG(D_WEB, D_DEBUG, "        conn   %s\n", req->connection);

  if (req->method == _BAD_REQ) {
    DBG(D_WEB, D_WARN, "WEB: BAD REQUEST\n");
    server_error(ioout, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
    return;
  }

  char content_type[USERV_MAX_CONTENT_TYPE_LEN];
  userver_http_status http_status = S200_OK;

  strncpy(content_type, "text/html", USERV_MAX_CONTENT_TYPE_LEN);

  userver_response res = USERVER_OK;
  if (userv.server_resp_f){
    res = userv.server_resp_f(req, IOWEB, &http_status, content_type);
  } else {
    server_error(ioout, S501_NOT_IMPLEMENTED, ERR_HTTP_NOT_IMPL);
    return;
  }

  if (res == USERVER_OK) {
    // plain response
    ioprint(ioout,
      "HTTP/1.1 %i %s\n"
      "Server: "APP_NAME"\n"
      "Content-Type: %s\n"
      "Content-Length: %i\n"
      "Connection: close\n"
      "\n",
      USERVER_HTTP_STATUS_NUM[http_status], USERVER_HTTP_STATUS_STRING[http_status],
      content_type,
      ringbuf_available(&userv.tx_rb));
    if (req->method != HEAD) {
      server_send_data(ioout);
    }
  } else if (res == USERVER_CHUNKED) {
    // chunked response
    ioprint(ioout,
      "HTTP/1.1 %i %s\n"
      "Server: "APP_NAME"\n"
      "Content-Type: %s\n"
      "Transfer-Encoding: chunked\n"
      "\n",
      USERVER_HTTP_STATUS_NUM[http_status], USERVER_HTTP_STATUS_STRING[http_status],
      content_type);
    if (req->method != HEAD) {
      u32_t chunk_len;
      while ((chunk_len = IO_rx_available(IOWEB)) > 0) {
        ioprint(ioout, "%x; chunk %i\r\n", chunk_len, req->chunk_nbr);
        server_send_data(ioout);
        ioprint(ioout, "\r\n");
        userv.req.chunk_nbr++;
        (void)userv.server_resp_f(req, IOWEB, &http_status, content_type); // from now on, we ignore response
      }
      ioprint(ioout, "0\r\n\r\n");
    }
  }
}

// handle HTTP header line
static void server_http_header_line(u8_t ioout, char *s, u16_t len, ringbuf* rb_in) {
  if (len > 0) {
    // http header element
    switch (userv.state) {
    case HEADER_METHOD: {
      u32_t i;
      for (i = 0; i < _REQ_METHOD_COUNT; i++) {
        if (strcmpbegin(USERVER_HTTP_REQ_METHODS[i], s) == 0) {
          userv.req.method = i;
          char *resource = space_strip(&s[strlen(USERVER_HTTP_REQ_METHODS[i])]);
          char *space = (char *)strchr(resource, ' ');
          if (space) {
            *space = 0;
          }
          strcpy(userv.req.resource, resource);
          break;
        }
      } // per method
      userv.state = HEADER_FIELDS;
      break;
    }

    case HEADER_FIELDS: {
      u32_t i;
      for (i = 0; i < _FIELD_COUNT; i++) {
        if (strcmpbegin(USERVER_HTTP_FIELDS[i], s) == 0) {
          switch (i) {
          case FCONNECTION: {
            char *value = space_strip(&s[strlen(USERVER_HTTP_FIELDS[i])]);
            strncpy(userv.req.connection, value, USERV_MAX_CONNECTION_LEN);
            break;
          }
          case FHOST: {
            char *value = space_strip(&s[strlen(USERVER_HTTP_FIELDS[i])]);
            strncpy(userv.req.host, value, USERV_MAX_HOST_LEN);
            break;
          }
          case FCONTENT_TYPE: {
            char *value = space_strip(&s[strlen(USERVER_HTTP_FIELDS[i])]);
            strncpy(userv.req.content_type, value, USERV_MAX_CONTENT_DISP_LEN);
            break;
          }
          case FCONTENT_LENGTH: {
            char *value = space_strip(&s[strlen(USERVER_HTTP_FIELDS[i])]);
            userv.req.content_length = atoi(value);
            break;
          }
          case FTRANSFER_ENCODING: {
            char *value = space_strip(&s[strlen(USERVER_HTTP_FIELDS[i])]);
            userv.req.chunked = strcmp("chunked", value) == 0;
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
    server_request(ioout, &userv.req);

    // expecting data?
    if (userv.req.chunked) {
      // --- chunked content
      if (userv.req.content_length > 0) {
        DBG(D_WEB, D_WARN, "WEB: BAD CHUNK REQUEST, Content-Length > 0\n");
        server_error(ioout, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
        return;
      }
      userv.state = CHUNK_DATA_HEADER;
      userv.chunk_ix = 0;
      userv.chunk_len = 0;
      userv.received_content_len = 0;
    } else  if (userv.req.content_length > 0) {
      // --- plain content
      userv.received_content_len = 0;
      userv.state = CONTENT;
      DBG(D_WEB, D_DEBUG, "WEB: getting content length %i, available now %i\n", userv.req.content_length, ringbuf_available(rb_in));

      // --- multipart content
      if (strcmpbegin("multipart/form-data", userv.req.content_type) == 0) {
        // get boundary string
        char *boundary_start = strstr(userv.req.content_type, "boundary");
        if (boundary_start == 0) {
          DBG(D_WEB, D_WARN, "WEB: BAD MULTIPART REQUEST, boundary not found\n");
          server_error(ioout, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
          return;
        }
        boundary_start += 8; // "boundary"
        boundary_start = space_strip(boundary_start);
        if (*boundary_start != '=') {
          DBG(D_WEB, D_WARN, "WEB: BAD MULTIPART REQUEST, = not found\n");
          server_error(ioout, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
          return;
        }
        boundary_start++;
        boundary_start = space_strip(boundary_start);
        if (strlen(boundary_start) == 0) {
          DBG(D_WEB, D_WARN, "WEB: BAD MULTIPART REQUEST, boundary id not found\n");
          server_error(ioout, S400_BAD_REQ, ERR_HTTP_BAD_REQUEST);
          return;
        }
        userv.multipart_boundary = boundary_start;
        userv.multipart_boundary_ix = 0;
        userv.multipart_delim = 0;
        userv.multipart_boundary_len = strlen(boundary_start);
        userv.req.cur_multipart.multipart_nbr = 0;
        userv.state = MULTI_CONTENT_HEADER;
        userv.header_line = 0;
        DBG(D_WEB, D_DEBUG, "WEB: boundary start: %s\n", boundary_start);
      }

    } else {
      // back to expecting a http header
      server_clear_req(&userv.req);
    }

    return;
  }
}

// handle multipart content header line
static void server_multi_content_header_line(u8_t ioout, char *s, u16_t len, ringbuf* rb_in) {
  char *boundary_start;
  if (strcmpbegin("--", s) == 0 && (boundary_start = strstr(s+2, userv.multipart_boundary))) {
    // boundary match
    if (strstr(boundary_start + userv.multipart_boundary_len, "--")) {
      // end of multipart message
      // back to expecting a http header
      DBG(D_WEB, D_DEBUG, "WEB: multipart finished\n");
      server_clear_req(&userv.req);
    } else {
      // multipart section
      DBG(D_WEB, D_DEBUG, "WEB: multipart section %i header\n", userv.req.cur_multipart.multipart_nbr);
    }
  } else if (len == 0) {
    // end of multipart header, start of multipart data
    DBG(D_WEB, D_DEBUG, "WEB: multipart data section %i [%s]\n", userv.req.cur_multipart.multipart_nbr,
        userv.req.cur_multipart.content_disp);
    userv.state = MULTI_CONTENT_DATA;
    userv.multipart_boundary_ix = 0;
    userv.multipart_delim = 0;
    userv.received_multipart_len = 0;
  } else {
    // multipart header, get fields
    u32_t i;
    for (i = 0; i < _FIELD_COUNT; i++) {
      if (strcmpbegin(USERVER_HTTP_FIELDS[i], s) == 0) {
        switch (i) {
        case FCONTENT_DISPOSITION: {
          char *value = space_strip(&s[strlen(USERVER_HTTP_FIELDS[i])]);
          strncpy(userv.req.cur_multipart.content_disp, value, USERV_MAX_CONTENT_DISP_LEN);
          break;
        }
        case FCONTENT_TYPE: {
          char *value = space_strip(&s[strlen(USERVER_HTTP_FIELDS[i])]);
          strncpy(userv.req.cur_multipart.content_type, value, USERV_MAX_CONTENT_TYPE_LEN);
          break;
        }
        } // switch field
        break;
      }
    } // per field
  }
}

// handle chunk header line
static void server_chunk_header_line(u8_t ioout, char *s, u16_t len, ringbuf* rb_in) {
  char *start = space_strip(s);
  char *end = (char *)strchr(start, ';');
  if (end) *end = 0;
  userv.chunk_len = atoin(start, 16, strlen(start));
  if (userv.chunk_len > 0) {
    DBG(D_WEB, D_DEBUG, "WEB: chunk %i, length %i\n", userv.chunk_ix, userv.chunk_len);
    userv.state = CHUNK_DATA;
  } else {
    DBG(D_WEB, D_DEBUG, "WEB: chunks finished, footer\n");
    userv.state = CHUNK_FOOTER;
    userv.header_line = 0;
  }
}

// handle chunk footer line
static void server_chunk_footer_line(u8_t ioout, char *s, u16_t len, ringbuf* rb_in) {
  if (len == 0) {
    server_clear_req(&userv.req);
  }
}

// http data timeout
void USERVER_timeout(u8_t ioout) {
  if (userv.state != HEADER_METHOD) {
    DBG(D_WEB, D_WARN, "WEB: request timeout\n");
    server_error(ioout, S408_REQUEST_TIMEOUT, ERR_HTTP_TIMEOUT);
  }
}

// parse http data characters
void USERVER_parse(u8_t ioout, ringbuf* rb_in) {
  s32_t rx;
  while ((rx = ringbuf_available(rb_in)) > 0) {
    switch (userv.state) {

    // --- HEADER PARSING

    case MULTI_CONTENT_HEADER:
    case CHUNK_DATA_HEADER:
    case CHUNK_DATA_END:
    case HEADER_METHOD:
    case CHUNK_FOOTER:
    case HEADER_FIELDS: {
      u8_t c;
      s32_t res = ringbuf_getc(rb_in, &c);
      if (res == RB_ERR_EMPTY) {
        return;
      }

      if (c == '\r') continue;
      if (userv.req_buf_len >= USERVER_REQ_BUF_MAX_LEN || c == '\n') {
        if (userv.req_buf_len >= USERVER_REQ_BUF_MAX_LEN) {
          userv.req_buf[USERVER_REQ_BUF_MAX_LEN] = 0;
        } else {
          userv.req_buf[userv.req_buf_len] = 0;
        }
        if (userv.state == CHUNK_DATA_HEADER) {
          DBG(D_WEB, D_DEBUG, "WEB CHUNK-HDR: %s\n", userv.req_buf);
          server_chunk_header_line(ioout, userv.req_buf, userv.req_buf_len, rb_in);
        } else if (userv.state == CHUNK_DATA_END) {
          // ignore
          DBG(D_WEB, D_DEBUG, "WEB CHUNK-DATA_END\n");
          userv.state = CHUNK_DATA_HEADER;
          userv.header_line = 0;
          userv.received_content_len = 0;
        } else if (userv.state == CHUNK_FOOTER) {
          DBG(D_WEB, D_DEBUG, "WEB CHUNK-FOOTER: %s\n", userv.req_buf);
          server_chunk_footer_line(ioout, userv.req_buf, userv.req_buf_len, rb_in);
        } else if (userv.state == MULTI_CONTENT_HEADER) {
          DBG(D_WEB, D_DEBUG, "WEB MULTI-HDR:%s\n", userv.req_buf);
          server_multi_content_header_line(ioout, userv.req_buf, userv.req_buf_len, rb_in);
        } else {
          DBG(D_WEB, D_DEBUG, "WEB HTTP-HDR: %s\n", userv.req_buf);
          server_http_header_line(ioout, userv.req_buf, userv.req_buf_len, rb_in);
        }
        userv.header_line++;
        userv.req_buf_len = 0;
      }

      if (c != '\n') {
        userv.req_buf[userv.req_buf_len++] = c;
      }
      break;
    }

    // --- DATA PARSING

    case CONTENT:
    case CHUNK_DATA: {
      // known content size
      s32_t len = MIN(rx, USERVER_REQ_BUF_MAX_LEN);
      if (userv.state == CONTENT) {
        len = MIN(len, userv.req.content_length - userv.received_content_len);
      } else if (userv.state == CHUNK_DATA) {
        len = MIN(len, userv.chunk_len - userv.received_content_len);
      }
      len = ringbuf_get(rb_in, (u8_t *)userv.req_buf, len);
      if (len <= 0) return;

      if (userv.server_data_f) {
        // report data
        userv.server_data_f(&userv.req, userv.state == CONTENT ? DATA_CONTENT : DATA_CHUNK,
            userv.received_content_len, (u8_t *)userv.req_buf, len);
      }

      userv.received_content_len += len;
      if (userv.req.chunked) {
        // chunking data
        if (userv.received_content_len == userv.chunk_len) {
          DBG(D_WEB, D_DEBUG, "WEB: chunk %i received\n", userv.chunk_ix);
          userv.chunk_ix++;
          userv.state = CHUNK_DATA_END;
          userv.header_line = 0;
        }
      } else {
        // content data
        if (userv.received_content_len == userv.req.content_length) {
          DBG(D_WEB, D_DEBUG, "WEB: all content received\n");
          server_clear_req(&userv.req);
        }
      }
      break;
    }
    case MULTI_CONTENT_DATA: {
      bool flush_buf = FALSE;
      u8_t c;
      s32_t res = ringbuf_getc(rb_in, &c);
      if (res == RB_ERR_EMPTY) {
        return;
      }

      userv.req_buf[userv.req_buf_len++] = c;

      if (c == '\n') {
        // report data at each newline for simplicity with boundary recognition
        if (userv.server_data_f) {
          userv.server_data_f(&userv.req, DATA_MULTIPART, userv.received_multipart_len,
              (u8_t*)userv.req_buf, userv.req_buf_len);
        }
        userv.received_multipart_len += userv.req_buf_len;
        userv.req_buf_len = 0;
      }

      // find boundary --<BOUNDARY>(--)
      if (c == '-' && userv.multipart_delim < 2) {
        userv.multipart_delim++;
      } else if (userv.multipart_delim == 2 &&
          c == userv.multipart_boundary[userv.multipart_boundary_ix]) {
        userv.multipart_boundary_ix++;
        if (userv.multipart_boundary_ix == userv.multipart_boundary_len) {
          userv.multipart_boundary_ix = 0;
          userv.multipart_delim = 0;
          userv.req.cur_multipart.multipart_nbr++;
          userv.state = MULTI_CONTENT_HEADER;
          server_multi_content_header_line(ioout,
              &userv.req_buf[userv.req_buf_len - userv.multipart_boundary_len - 2],
              userv.multipart_boundary_len + 2,
              rb_in);
          continue;
        }
      } else {
        // no boundary indication, pure data
        if (userv.multipart_delim > 0 || userv.multipart_boundary_ix > 0) {
          // report eaten data believed to be boundary
          flush_buf = TRUE;
        }
        userv.multipart_delim = 0;
        userv.multipart_boundary_ix = 0;
      }

      if (flush_buf || userv.req_buf_len >= USERVER_REQ_BUF_MAX_LEN) {
        // flush req or buffer overflow, report
        if (userv.server_data_f) {
          userv.server_data_f(&userv.req, DATA_MULTIPART, userv.received_multipart_len,
              (u8_t*)userv.req_buf, userv.req_buf_len);
        }
        userv.received_multipart_len += userv.req_buf_len;
        userv.req_buf_len = 0;
      }

      userv.received_content_len++;
      if (userv.received_content_len == userv.req.content_length) {
        if (userv.server_data_f) {
          // report last bytes if we have not left this state already
          userv.server_data_f(&userv.req, DATA_MULTIPART, userv.received_multipart_len,
              (u8_t*)userv.req_buf, userv.req_buf_len);
        }
        userv.received_multipart_len += userv.req_buf_len;
        DBG(D_WEB, D_DEBUG, "WEB: all multi content received %i\n", userv.req.content_length);
        server_clear_req(&userv.req);
      }

      break;
    }
    } // switch state
  } // while rx avail
}

void USERVER_init(userver_response_f server_resp_f, userver_data_f server_data_f) {
  memset(&userv, 0, sizeof(userv));
  userv.server_resp_f = server_resp_f;
  userv.server_data_f = server_data_f;
  ringbuf_init(&userv.tx_rb, userv.tx_buf, sizeof(userv.tx_buf));

  IO_define(IOWEB, io_ringbuffer, (u32_t)&userv.tx_rb);
}

static char *space_strip(char *s) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  return s;
}
