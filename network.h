#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include "route.h"

#define BOCKLOG 1000
#define THREADNUM 10
#define THREAD_TIMEOUT 5

int
lal_get_socket_or_die (struct addrinfo *host);

int
lal_bind_and_listen_or_die (int sock, struct addrinfo *host);

struct addrinfo *
lal_get_host_addrinfo_or_die (const char *hostname, const char *port);

void
lal_serve_forever(void *(*socket_handler)(void *arg),
                  const char *host,
                  const char *port,
                  int daemonize,
                  int threaded);
