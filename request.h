#ifndef REQUEST_H_
#define REQUEST_H_

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <bsd/string.h>
#include "utils.h"

#define MAXHEADERSIZE 8192

enum lal_http_method {
    GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS, ANY
};

struct lal_request {
	const char *path;
  int pathlen;
	enum lal_http_method method;
	const char *_raw_header;
	struct lal_entry *header;
	const uint8_t *content;
	int content_length;
};

const char
*lal_method_to_string (enum lal_http_method m);

struct lal_entry
*lal_get_header (struct lal_request *request, const char *key);

void
lal_set_headers (struct lal_request *request, const char *src);

const char
*lal_parse_header (int sock);

void
lal_set_content(int sock, struct lal_request *request);

void
lal_destroy_request (struct lal_request *request);

struct lal_request
*lal_create_request (const char *src);

#endif // REQUEST_H_
