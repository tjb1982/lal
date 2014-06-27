#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <syslog.h>
#include <errno.h>
#include <execinfo.h>

#define CR '\r'
#define LF '\n'
#define CRLF "\r\n"
#define MAXHEADERSIZE 8192

#ifndef ROUTE_H_
#define ROUTE_H_
enum lal_http_method {
    GET, POST, PUT, DELETE, HEAD, OPTIONS
};

struct lal_entry {
    char* key;
    char *val;
    struct lal_entry *next;
};

struct lal_request {
    enum lal_http_method method;
    char *path;
    struct lal_entry *entries;
    int nentries;
};

struct lal_route {
    char *path;
    enum lal_http_method method;
    int(*handler)(struct lal_request *, int sock);
    struct lal_route *next;
};
#endif // ROUTE_H_

enum lal_header
lal_header_from_string(char *name);

char *
lal_get_header_val(struct lal_request *request, const char *key);

void
lal_set_entries (struct lal_request *request, char *src);

void
lal_destroy_request (struct lal_request *request);

struct lal_request *
lal_create_request (char *src);

char *
lal_read_header (int sock);

void
lal_route_request (int sock, int hitcount);

void
lal_destroy_routes ();

void
lal_register_route(enum lal_http_method method, char *route,
                 int(*handler)(struct lal_request *, int));

struct lal_route *
lal_get_route(struct lal_request *request);
