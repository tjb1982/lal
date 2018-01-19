#include "network.h"

int volatile listening_socket = -1;
pthread_cond_t cond;
pthread_mutex_t mutex;

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
		freeaddrinfo(host);
		exit(EXIT_FAILURE);
	}

	return sock;

}

void
lal_bind_and_listen_or_die (int sock, struct addrinfo *host)
{

	int status = 0, opt = 1;

	status = setsockopt(
		sock,
		SOL_SOCKET,
		SO_REUSEADDR,
		&opt,
		sizeof(opt)
	);

	if (status < 0) {
		fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	status = bind(
		sock,
		host->ai_addr,
		host->ai_addrlen
	);

	if (status < 0) {
		fprintf(stderr, "Error binding socket to port: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	status = listen(sock, SOMAXCONN);

	if (status < 0) {
		fprintf(stderr, "Error listening to socket: %s\n", strerror(errno));
		freeaddrinfo(host);
		exit(EXIT_FAILURE);
	}

}

struct addrinfo *
lal_get_host_addrinfo_or_die (const char *hostname, const char *port)
{

	int status;
	static struct /* static--i.e., initialized to 0 */
			addrinfo hints,
			*host; 

	hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* TCP */

	status = getaddrinfo(hostname, port, &hints, &host);

	if (status != 0) {
		fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	return host;

}

void
handleINT(int sig)
{
	if (~listening_socket) {
		signal(sig, SIG_IGN);
		fprintf(stderr, "\n%s\n", "Shutting down");
		shutdown(listening_socket, SHUT_RDWR);
		close(listening_socket);
		listening_socket = -1;
	}
}

int normalize_index(int index) {
	return index == JOBNUM
		? index - JOBNUM
		: index; 
}

int
get_open_socket(int *queue) {
	static int index = 0;
	int socket;
	index = normalize_index(index);
	for (; index < JOBNUM; index++) {
		if (~(socket = queue[index])) {
			queue[index] = -1;
			return socket;
		}
		index = normalize_index(index);
	}
	return 0;
}

//int
//index_of_open(int *queue) {
//	int index = 0;
//	for (; index < JOBNUM; index++) {
//		if (~queue[index]) {
//			return index;
//		}
//	}
//	return -1;
//}

void *
hunt_heads(void *arg)
{
	int socket;
	struct lal_thread *data = arg;
	struct lal_headhunter *h = data->headhunter;

	printf("hunt_heads started on thread #%li.\n", data->id);

	while (~listening_socket) {

		/* find an open job opportunity */
		pthread_mutex_lock(&mutex);
		while (!(socket = get_open_socket(h->queue))) {
			pthread_cond_wait(&cond, &mutex);
		}
		pthread_mutex_unlock(&mutex);

		struct lal_job job = {
			.socket = socket,
			.hitcount = h->hitcount,
			.extra = h->extra,
			.job_started = 0,
			.canceled = 0
		};

		time(&job.job_started);

		h->job(&job);
		shutdown(job.socket, SHUT_RDWR);
		close(job.socket);
	}
	pthread_exit(NULL);
	return NULL;
}

void
lal_serve_forever(
	const char *host,
	const char *port,
	int (*job)(void *arg),
	void *extra
) {

	int request_socket = 0, queue_index = 0, rc, i;
	int queue[JOBNUM]; memset(queue, -1, sizeof(queue));
	struct addrinfo request_addrinfo,
		*host_addrinfo = lal_get_host_addrinfo_or_die(
			host ? host : "localhost",
			port ? port : "80"
		);
	socklen_t request_socklen = sizeof(request_addrinfo);
	struct lal_headhunter headhunter = {
			.hitcount = 0,
			.queue = queue,
			.job = job,
			.extra = extra
		};
	pthread_attr_t pthread_attr;
	struct lal_thread threads[THREADNUM];

	listening_socket = lal_get_socket_or_die(host_addrinfo);

	lal_bind_and_listen_or_die(listening_socket, host_addrinfo);

	freeaddrinfo(host_addrinfo);

	signal(SIGINT, handleINT);

	/*
	 * Thread loop that matches jobs to worker jobs.
	 * */
	pthread_attr_init(&pthread_attr);
	pthread_attr_setstacksize(&pthread_attr, 32768);
	for (i = 0; i < THREADNUM; i++) {
		struct lal_thread *thread = &threads[i];
		thread->headhunter = &headhunter;
		if ((rc = pthread_create(
			&thread->id, &pthread_attr, hunt_heads, (void *)thread
		))) {
			fprintf(
				stderr,
				"pthread_create failed: cancelling thread %li. "
					"rc: %d.\n",
				thread->id, rc
			);
			// TODO: is pthread_cancel the right move here?
			pthread_cancel(thread->id);
			goto exit; 
		}
	}

	printf("Lal serving at %s:%s\n", host, port);

	/*
	 * Accepted `request_socket`s are added to the `headhunter`'s job queue in round
	 * robin fashion. If the `listening_socket` is closed (i.e., its value set
	 * to `-1`), then this function returns control to the client application.
	 * */
	pthread_cond_init(&cond, NULL);
	pthread_mutex_init(&mutex, NULL);

	while (~listening_socket) {

		headhunter.hitcount++;

		request_socket = accept(
			listening_socket,
			(struct sockaddr *) &request_addrinfo,
			&request_socklen
		);

		/**
		 * Loop through the job queue until an open slot (`-1`) is found
		 * */
		while (~headhunter.queue[queue_index]) {
			queue_index = ++queue_index == JOBNUM
				? queue_index - JOBNUM
				: queue_index;
		}

		pthread_mutex_lock(&mutex);

		headhunter.queue[queue_index] = request_socket;

		pthread_mutex_unlock(&mutex);
		pthread_cond_signal(&cond);
	}

exit:
	return;

}
