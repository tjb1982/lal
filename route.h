#ifndef ROUTE_H_
#define ROUTE_H_

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <syslog.h>
#include <errno.h>
#include <execinfo.h>
#include <pthread.h>
#include <signal.h>
#include "network.h"
#include "request.h"

extern struct lal_route *routes;

struct lal_route {
    char *path;
    enum lal_http_method method;
    int(*handler)(struct lal_request *, int sock);
    struct lal_route *next;
};

void *
lal_route_request (void *arg);

void
lal_destroy_routes ();

void
lal_register_route(enum lal_http_method method, const char *route,
                 int(*handler)(struct lal_request *, int));

struct lal_route *
lal_get_route(struct lal_request *request);

#endif // ROUTE_H_
