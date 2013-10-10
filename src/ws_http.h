/*
 * ws_http.h
 *
 *  Created on: Oct 10, 2013
 *      Author: petera
 */

#ifndef WS_HTTP_H_
#define WS_HTTP_H_

#include "system.h"

#define WS_MAX_CONTENT_TYPE_LEN     64
#define WS_MAX_CONNECTION_LEN       64

typedef enum {
  // Requests a representation of the specified resource. Requests using GET should only retrieve data and should have no other effect. (This is also true of some other HTTP methods.)[1] The W3C has published guidance principles on this distinction, saying, "Web application design should be informed by the above principles, but also by the relevant limitations."
  GET = 0,
  // Asks for the response identical to the one that would correspond to a GET request, but without the response body. This is useful for retrieving meta-information written in response headers, without having to transport the entire content.
  HEAD,
  // Requests that the server accept the entity enclosed in the request as a new subordinate of the web resource identified by the URI. The data POSTed might be, as examples, an annotation for existing resources; a message for a bulletin board, newsgroup, mailing list, or comment thread; a block of data that is the result of submitting a web form to a data-handling process; or an item to add to a database.
  POST,
  // Requests that the enclosed entity be stored under the supplied URI. If the URI refers to an already existing resource, it is modified; if the URI does not point to an existing resource, then the server can create the resource with that URI
  PUT,
  // Deletes the specified resource.
  DELETE,
  // Echoes back the received request so that a client can see what (if any) changes or additions have been made by intermediate servers.
  TRACE,
  // Returns the HTTP methods that the server supports for the specified URL. This can be used to check the functionality of a web server by requesting '*' instead of a specific resource
  OPTIONS,
  // Converts the request connection to a transparent TCP/IP tunnel, usually to facilitate SSL-encrypted communication (HTTPS) through an unencrypted HTTP proxy
  CONNECT,
  // Is used to apply partial modifications to a resource
  PATCH,
  _REQ_METHOD_COUNT
} ws_req_method;

static const char* const SERVER_REQ_METHODS[] = {
  "GET",
  "HEAD",
  "POST",
  "PUT",
  "DELETE",
  "TRACE",
  "OPTIONS",
  "CONNECT",
  "PATCH",
};

typedef enum {
  FCONNECTION = 0,
  FHOST,
  FCONTENT_LENGTH,
  FCONTENT_TYPE,
  _FIELD_COUNT
} ws_http_fields;

static const char* const SERVER_FIELDS[] = {
  "Connection",
  "Host",
  "Content-Length",
  "Content-Type",
};


typedef enum {
  S100_CONTINUE = 100,
  S101_SWITCHING_PROTOCOLS = 101,
  S200_OK = 200,
  S201_CREATED = 201,
  S202_ACCEPTED = 202,
  S203_NON_AUTH_INFO = 203,
  S204_NO_CONTENT = 204,
  S205_RESET_CONTENT = 205,
  S206_PARTIAL_CONTENT = 206,
  S300_MULT_CHOICES = 300,
  S301_MOVED_PERMANENTLY = 301,
  S302_FOUND = 302,
  S303_SEE_OTHER = 303,
  S304_NOT_MODIFIED = 304,
  S305_USE_PROXY = 305,
  S400_BAD_REQ = 400,
  S401_UNAUTH = 401,
  S402_PAYMENT_REQUIRED = 402,
  S403_FORBIDDEN = 403,
  S404_NOT_FOUND = 404,
  S405_METHOD_NOT_ALLOWED = 405,
  S406_NOT_ACCEPTABLE = 406,
  S407_PROXY_AUTH_REQ = 407,
  S408_REQUEST_TIMEOUT = 408,
  S409_CONFLICT = 409,
  S410_GONE = 410,
  S411_LENGTH_REQ = 411,
  S412_PRECONDITION_FAILED = 412,
  S413_REQ_ENTITY_TOO_LARGE = 413,
  S414_REQ_URI_TOO_LONG = 414,
  S415_UNSUPPORTED_MEDIA_TYPE = 415,
  S416_REQ_RANGE_NOT_SATISFIABLE = 416,
  S417_EXPECTATION_FAILED = 417,
  S500_INTERNAL_SERVER_ERROR = 500,
  S501_NOT_IMPLEMENTED = 501,
  S502_BAD_GATEWAY = 502,
  S503_SERVICE_UNAVAILABLE = 503,
  S504_GATEWAY_TIMEOUT = 504,
  S505_HTTP_VERSION_NOT_SUPPORTED = 505,
} ws_status;

typedef struct {
  ws_req_method method;
  u32_t content_length;
  char content_type[WS_MAX_CONTENT_TYPE_LEN];
  char connection[WS_MAX_CONNECTION_LEN];
} ws_request_header;

typedef struct {
  ws_status status;
  u32_t content_length;
  char content_type[WS_MAX_CONTENT_TYPE_LEN];
  char connection[WS_MAX_CONNECTION_LEN];
} ws_response;


#endif /* WS_HTTP_H_ */
