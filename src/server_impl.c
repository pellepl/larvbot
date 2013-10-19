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

userver_response server_response(userver_request_header *req, u8_t iobuf, userver_http_status *http_status,
    char *content_type) {
  ioprint(iobuf, "<html>"
      "<head>"
      "<title>An Example Page</title>"
      "</head>"
      "<body>"
      "Hello World, this is a very simple HTML document.<br/>");

  ioprint(iobuf, "got request: ");
  ioprint(iobuf, req->resource);

  ioprint(iobuf,
      "<form name=\"input\" action=\"posttest\" method=\"post\">"
      "Username: <input type=\"text\" name=\"user\">"
      "<input type=\"submit\" value=\"Submit\">"
      "<input type=\"reset\">"
      "</form>"
  );

  ioprint(iobuf,
    "<form enctype=\"multipart/form-data\" action=\"fileupload\" method=\"post\">"
    /*"<input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"1000000\" />"*/
    "Choose a file to upload: <input name=\"uploadedfile\" type=\"file\" /><br />"
    "<input type=\"submit\" value=\"Upload File\" />"
    "</form>"
  );

  ioprint(iobuf, "</body>"
      "</html>");

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
