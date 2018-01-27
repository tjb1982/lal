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
#include "log.h"

#define THREADNUM 4
#define THREAD_TIMEOUT 5
#define JOBNUM 10000
#define WORKERNUM 1

struct lal_job {
	int	socket;
	size_t	hitcount;
	void	*extra;
	/* e.g., `struct lal_route *`*/
	time_t	job_started;
	int	canceled;
};

struct lal_headhunter {
	size_t		hitcount;
	//int		*queue;
	struct lal_job	*queue;
	int		(*job)(void *args);
	/* e.g., `struct lal_route *`*/
	void		*extra;
};

struct lal_thread {
	pthread_t		id;
	struct lal_headhunter 	*headhunter;
};


/**
 * Returns a socket file descriptor (int) or frees addrinfo and dies
 * */
int
lal_get_socket_or_die (struct addrinfo *host);

void
lal_bind_and_listen_or_die (
	int	sock,
	struct	addrinfo *host
);

/**
 * Hostname can be either IPv4/6 or domain name string
 * */
struct addrinfo *
lal_get_host_addrinfo_or_die (
	const char *hostname,
	const char *port
);

void
lal_serve_forever (
	const char	*host,
	const char	*port,
	int		(*socket_handler)(void *arg),
	void		*extra
);


#endif // _NETWORK_H_
