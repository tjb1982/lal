#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include "utils.h"

#define CR '\r'
#define LF '\n'
#define CRLF "\r\n"
#define MAXHEADERSIZE 8192

#ifndef REQUEST_H_
#define REQUEST_H_
enum lal_http_method {
    GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS, ANY
};

struct lal_request {
    enum lal_http_method method;
    char *path;
    struct lal_entry *entries;
    int nentries;
};

const char *
lal_method_to_string(enum lal_http_method m);

char *
lal_get_header_val (struct lal_request *request, const char *key);

void
lal_set_entries (struct lal_request *request, char *src);

char *
lal_read_header (int sock);

void
lal_destroy_request (struct lal_request *request);

struct lal_request *
lal_create_request (char *src);
#endif // REQUEST_H_
