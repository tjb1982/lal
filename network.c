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
        syslog(LOG_ERR, "Unable to connect to socket: %s", strerror(errno));
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
        syslog(LOG_ERR, "Error binding socket to port: %s", strerror(errno));
        exit(1);
    }

    status = listen(sock, BACKLOG);

    if (status < 0) {
        syslog(LOG_ERR, "Error listening to socket: %s", strerror(errno));
        exit(1);
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
        syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(status));

    return host;

}

void *
loop(void *args) {
	Thread *thread = args;
	while (thread->id) {
		thread->current_job->job(args);
		thread->ready = true;
	
		while (!thread->ready)
			;
	}
	return NULL;
}

void
lal_serve_forever(
	void *(*job)(void *arg),
	const char *host,
	const char *port
) {
	int id = 0, job_id = 0, listening_socket, request_socket, hitcount = 0;
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

//	if (daemonize)
//		if (fork() != 0) exit(0); /* daemonize */
//	(void) signal(SIGCLD, SIG_IGN); /* ignore child death */
//	(void) signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
//	(void) setpgid(0,0); /* break away from process group */

	Thread *threads = calloc(
		THREADNUM,
		sizeof(Thread)
	);

	Job *backlog = calloc(
		BACKLOG,
		sizeof(Job)
	);

	pthread_attr_init(&pthread_attr);
	pthread_attr_setstacksize(&pthread_attr, 32768);
//	pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);

	printf("Lal serving on port %s\n", port);

	while (++hitcount) {
	
		request_socket = accept(
			listening_socket,
			(struct sockaddr *) &request_addrinfo,
			&request_addrinfo_socklen
		);

		id = 0;
		while (threads[id].id && !threads[id].ready) {
			time_t now; time(&now);
			if (now - threads[id].job_started == THREAD_TIMEOUT) {
				pthread_cancel(threads[id].id);
				threads[id].ready = true;
				threads[id].id = 0;
				break;
			}
			id = ++id == THREADNUM ? id - THREADNUM : id;
		}

		threads[id].socket = request_socket;
		threads[id].hitcount = hitcount;
//		job_id = 0; 
printf("job_id %i\n", threads[id].current_job);
//		while (threads[id].current_job && threads[id].current_job->running)
			job_id = (++job_id == BACKLOG) ? (job_id - BACKLOG) : job_id;
		Job j = backlog[job_id];
		j.running = false;
		j.job = job;
		threads[id].current_job = &j;
		threads[id].job_started = time(&threads[id].job_started);
		threads[id].ready = false;

		if (!threads[id].id) {
try_again:
			if (pthread_create(
				&threads[id].id, &pthread_attr, loop, &threads[id]
			)) {
printf("trying again....\n");
				pthread_cancel(threads[id].id);
				goto try_again;
			}
printf("pthread_create called: %li ready?: %i\n", threads[id].id, threads[id].ready);
		}
	}
	pthread_attr_destroy(&pthread_attr);
}
