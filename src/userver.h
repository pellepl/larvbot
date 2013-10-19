/*
 * userver.h
 *
 *  Created on: Oct 10, 2013
 *      Author: petera
 */

#ifndef USERVER_H_
#define USERVER_H_

#include "userver_http.h"
#include "ringbuf.h"

#ifndef USERVER_TX_MAX_LEN
#define USERVER_TX_MAX_LEN              2048
#endif
#ifndef USERVER_TX_MAX_LEN
#define USERVER_TX_MAX_LEN              512
#endif

#ifndef USERV_MAX_RESOURCE_LEN
#define USERV_MAX_RESOURCE_LEN          256
#endif
#ifndef USERV_MAX_HOST_LEN
#define USERV_MAX_HOST_LEN              64
#endif
#ifndef USERV_MAX_CONTENT_TYPE_LEN
#define USERV_MAX_CONTENT_TYPE_LEN      128
#endif
#ifndef USERV_MAX_CONNECTION_LEN
#define USERV_MAX_CONNECTION_LEN        64
#endif
#ifndef USERV_MAX_CONTENT_DISP_LEN
#define USERV_MAX_CONTENT_DISP_LEN      256
#endif

// Request return codes
typedef enum {
  USERVER_OK = 0,
  USERVER_CHUNKED
} userver_response;

// Multipart content metadata
typedef struct {
  u32_t multipart_nbr;
  char content_type[USERV_MAX_CONTENT_TYPE_LEN];
  char content_disp[USERV_MAX_CONTENT_DISP_LEN];
} userver_request_multipart;

// Request metadata
typedef struct {
  userver_http_req_method method;
  char resource[USERV_MAX_RESOURCE_LEN];
  char host[USERV_MAX_HOST_LEN];
  u32_t content_length;
  char content_type[USERV_MAX_CONTENT_TYPE_LEN];
  char connection[USERV_MAX_CONNECTION_LEN];
  bool chunked;
  u32_t chunk_nbr;
  userver_request_multipart cur_multipart;
} userver_request_header;

// Data types
typedef enum {
  DATA_CONTENT = 0,
  DATA_CHUNK,
  DATA_MULTIPART
} userver_data_type;

/**
 * Serve a client request.
 * Can respond with a full data, or with chunked transfer.
 *
 * @param req - contains the client request data
 * @param iobuf - iostream where to put data to be sent to client
 * @param http_status - defaults to S200_OK, but can be altered if necessary
 * @param content_type - defaults to text/plain, but can be altered if necessary
 * @return SERVER_OK if all data to send to client is filled in iobuf;
 *         SERVER_CHUNK if server wants to send partial data to client via iobuf.
 *         If so, this function will be called repeatedly until user sends zero data.
 */
typedef userver_response (*userver_response_f)(
    userver_request_header *req,
    u8_t iobuf,
    userver_http_status *http_status,
    char *content_type);

typedef void (*userver_data_f)(
    userver_request_header *req,
    userver_data_type type,
    u32_t offset,
    u8_t *data,
    u32_t length);

/* Initiates userver with given response and data functions */
void USERVER_init(userver_response_f server_resp_f, userver_data_f server_data_f);
/*  Call this when client has sent no data in a while */
void USERVER_timeout(u8_t ioout);
/* Call this when there is client request data in ringbuffer rb_in.
 * Response will be sent to ioout */
void USERVER_parse(u8_t ioout, ringbuf* rb_in);

/* Returns a url-decoded version of str */
char *urlndecode(char *dst, char *str, int num);
/* Returns a url-encoded version of str */
char *urlnencode(char *dst, char *str, int num);



#endif /* USERVER_H_ */
