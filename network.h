#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdbool.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include "route.h"

#define BACKLOG 200
#define THREADNUM 4
#define THREAD_TIMEOUT 5

struct lal_thread {
	pthread_t id;
	int socket;
	size_t hitcount;
	int volatile ready;
	int (*job)(void *args);
	void *extra;
	time_t job_started;
};

int
lal_get_socket_or_die (struct addrinfo *host);

int
lal_bind_and_listen_or_die (int sock, struct addrinfo *host);

struct addrinfo *
lal_get_host_addrinfo_or_die (const char *hostname, const char *port);

void
lal_serve_forever (
	const char *host,
	const char *port,
	int (*socket_handler)(void *arg),
	void *extra
);


#endif // _NETWORK_H_
