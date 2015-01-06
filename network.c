#include "network.h"

int
lal_get_socket_or_die (struct addrinfo *host)
{
	int sock = socket(
		host->ai_family,
		host->ai_socktype,
		host->ai_protocol
	);

	if (sock < 0) {
		fprintf(stderr, "Unable to connect to socket: %s\n", strerror(errno));
		exit(1);
	}

	return sock;

}

int
lal_bind_and_listen_or_die (int sock, struct addrinfo *host)
{
	int status = bind(
		sock,
		host->ai_addr,
		host->ai_addrlen
	);

	if (status < 0) {
		fprintf(stderr, "Error binding socket to port: %s\n", strerror(errno));
		exit(1);
	}
	
	status = listen(sock, BACKLOG);
	
	if (status < 0) {
		fprintf(stderr, "Error listening to socket: %s\n", strerror(errno));
		exit(1);
	}

	int opt = 1;
	if (!~setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))) {
		fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
	}

	return status;

}

struct addrinfo *
lal_get_host_addrinfo_or_die (const char *hostname, const char *port)
{

    int status;
    static struct addrinfo hints, *host; /* static; i.e., initialized to 0 */

    hints.ai_family = AF_INET; //AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP */

    status = getaddrinfo(
        hostname, port, &hints, &host
    );

    if (status != 0)
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(status));

    return host;

}

int volatile listening_socket = 0;
int volatile accept_socket = 0;

void
handleINT(int sig)
{
	if (listening_socket) {
		char buf[20];
		signal(sig, SIG_IGN);
		snprintf(buf, 20, "\nreceived signal %i\n", sig);
		write(2, buf, 20);
		close(listening_socket);
	}
	exit(0);
}

void *
loop(void *args) {
	Thread *thread = args;
	while (!thread->ready) {
		thread->job(args);
		shutdown(thread->socket, SHUT_RDWR);
		close(thread->socket);
		thread->ready = true;
		while (thread->ready)
			;
	}
	return NULL;
}

void
lal_serve_forever(
	const char *host,
	const char *port,
	int (*job)(void *arg),
	void *extra
) {
	int id = 0, /*listening_socket, */request_socket, hitcount = 0, sockopt = 1;
	pthread_attr_t pthread_attr;

	struct addrinfo
		request_addrinfo,
		*host_addrinfo = lal_get_host_addrinfo_or_die(
			host ? host : "localhost",
			port ? port : "8080"
		);

	socklen_t request_addrinfo_socklen = sizeof request_addrinfo;


	listening_socket = lal_get_socket_or_die(host_addrinfo);
	(void) lal_bind_and_listen_or_die(listening_socket, host_addrinfo);

	freeaddrinfo(host_addrinfo);

	signal(SIGINT, handleINT);

	Thread *threads = calloc(
		THREADNUM,
		sizeof(Thread)
	);

	pthread_attr_init(&pthread_attr);
	pthread_attr_setstacksize(&pthread_attr, 32768);

	printf("Lal serving on port %s\n", port);

	while (++hitcount) {

		request_socket = accept(
			listening_socket,
			(struct sockaddr *) &request_addrinfo,
			&request_addrinfo_socklen
		);

		if (!~setsockopt(request_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&sockopt, sizeof(sockopt))) {
			fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
		}

		id = 0;
		while (threads[id].id && !threads[id].ready) {
			time_t now; time(&now);
			if (now - threads[id].job_started >= THREAD_TIMEOUT) {
				fprintf(stderr, "thread %li cancelled: Timed out\n", (long)threads[id].id);
				pthread_cancel(threads[id].id);
				close(threads[id].socket);
				threads[id].ready = true;
				threads[id].id = 0;
				break;
			}
			id = ++id == THREADNUM ? id - THREADNUM : id;
		}

		threads[id].socket = request_socket;
		threads[id].hitcount = hitcount;
		threads[id].job_started = time(&threads[id].job_started);
		threads[id].ready = false;

		if (!threads[id].id) {
			int attempts = 0;
try_again:
			fprintf(stderr, "creating new thread....");
			threads[id].job = job;
			threads[id].extra = extra;

			if (pthread_create(
				&threads[id].id, &pthread_attr, loop, &threads[id]
			)) {
				fprintf(
					stderr,
					"pthread_create failed: cancelling thread %li. Ready?: %i\n",
					threads[id].id,
					threads[id].ready
				);
				pthread_cancel(threads[id].id);
				if (++attempts <= 10) {
					sleep(1);
					goto try_again;
				}
				else {
					fprintf(
						stderr,
						"%i attempts to pthread_create failed. Shutting down.\n",
						attempts
					);
					handleINT(SIGINT);
				}
			}
			else {
				fprintf(stderr, "%li\n", threads[id].id);
				attempts = 0;
			}
		}
	}
	pthread_attr_destroy(&pthread_attr);
}
