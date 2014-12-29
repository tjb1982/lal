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
#include "request.h"

#ifndef ROUTE_H_
#define ROUTE_H_
extern struct lal_route *routes;

struct thread_state {
    pthread_t thread;
    int socket;
    size_t hitcount;
    int ready;
    time_t job_started;
};

struct lal_route {
    char *path;
    enum lal_http_method method;
    int(*handler)(struct lal_request *, int sock);
    struct lal_route *next;
};
#endif // ROUTE_H_

void *
lal_route_request (void *arg);

void
lal_destroy_routes ();

void
lal_register_route(enum lal_http_method method, const char *route,
                 int(*handler)(struct lal_request *, int));

struct lal_route *
lal_get_route(struct lal_request *request);
