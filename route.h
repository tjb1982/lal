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

#ifdef __cplusplus
extern "C" {
#endif
//extern struct lal_route *routes;

struct lal_route {
    const char *path;
    const size_t pathlen;
    const enum lal_http_method method;
    int(*const handler)(int, struct lal_request *);
    struct lal_route *next;
};

int
lal_route_request (void *arg);

void
lal_destroy_routes (struct lal_route *routes);

struct lal_route *
lal_init_routes();

void
lal_register_route(struct lal_route *routes, enum lal_http_method method, const char *route,
                 int(*handler)(int, struct lal_request *));

struct lal_route *
lal_get_route(struct lal_route *routes, struct lal_request *request);

#ifdef __cplusplus
}
#endif
#endif // ROUTE_H_
