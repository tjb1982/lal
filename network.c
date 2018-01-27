#include "network.h"

int volatile listening_socket = -1;
struct lal_thread threads[THREADNUM];
pthread_cond_t cond;
pthread_mutex_t mutex;
pthread_mutex_t log_mutex;
int volatile failed_requests = 0;


void
lock_fn(void *_, int lock) {
	if (lock)
		pthread_mutex_lock(&log_mutex);
	else
		pthread_mutex_unlock(&log_mutex);
}

int
lal_get_socket_or_die (struct addrinfo *host)
{
	int sock = socket(
		host->ai_family,
		host->ai_socktype,
		host->ai_protocol
	);

	if (sock < 0) {
		log_fatal("Unable to connect to socket: %s", strerror(errno));
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
		log_fatal("setsockopt failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	status = bind(
		sock,
		host->ai_addr,
		host->ai_addrlen
	);

	if (status < 0) {
		log_fatal("Error binding socket to port: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	status = listen(sock, SOMAXCONN);

	if (status < 0) {
		log_fatal("Error listening to socket: %s", strerror(errno));
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
		log_fatal("getaddrinfo failed: %s", gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	return host;

}

void
handleINT(int sig)
{
	if (~listening_socket) {
		signal(sig, SIG_IGN);
		log_info("%s", "Shutting down");
		shutdown(listening_socket, SHUT_RDWR);
		close(listening_socket);
		listening_socket = -1;
	}
}

//int normalize_index(int index) {
//	return index == JOBNUM ? index - JOBNUM : index; 
//}
#define normalize_index(index) ((index) == JOBNUM ? (index) - JOBNUM : (index))

int
get_open_job(struct lal_job *queue) {
	static int index = 0;
	index = normalize_index(index);
	for (; index != JOBNUM; index++) {
		if (~queue[index].socket) {
			return index;
		}
		index = normalize_index(index);
	}
	return -1;
}


void *
hunt_heads(void *arg)
{
	int bad_rc = 0;
	int job_index;
	struct lal_thread *thread = arg;
	struct lal_headhunter *h = thread->headhunter;

	log_trace("hunt_heads started on thread #%li.", thread->id);

	while (~listening_socket) {

		/* find an open job opportunity */
		pthread_mutex_lock(&mutex);
		while (!~(job_index = get_open_job(h->queue))) {
			pthread_cond_wait(&cond, &mutex);
			if (!~listening_socket)
				goto exit;
		}
		struct lal_job job = h->queue[job_index];
		h->queue[job_index].socket = -1;
		pthread_mutex_unlock(&mutex);

		time(&(job.job_started));

		int rc = h->job(&job);
		//shutdown(job.socket, SHUT_RDWR);
		close(job.socket);
		if (rc) {
			bad_rc++;
			pthread_mutex_lock(&log_mutex);
			failed_requests++;
			pthread_mutex_unlock(&log_mutex);
		}
	}
exit:
	log_info("#%li: Failed requests: %i", thread->id, bad_rc);
	if (pthread_mutex_trylock(&mutex))
		pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	return NULL;
}

void
spawn_workers(void) {
	int num_workers = WORKERNUM;
	int pid;

	while (--num_workers) {

		pid = fork();

		if (pid < 0) {
			/* something went wrong */
			break;
		} else if (!pid) {
			/* prevent child from spawning its own children */
			break;
		}
	}
}


/*
 * Thread loop that matches jobs to worker jobs.
 * */
void
spawn_threads(struct lal_headhunter *headhunter) {

	int i, rc;
	pthread_attr_t pthread_attr;
	pthread_cond_init(&cond, NULL);
	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_init(&log_mutex, NULL);

	pthread_attr_init(&pthread_attr);
	pthread_attr_setstacksize(&pthread_attr, 32768);
	for (i = 0; i < THREADNUM; i++) {
		struct lal_thread *thread = &threads[i];
		thread->headhunter = headhunter;
		if ((rc = pthread_create(
			&thread->id, &pthread_attr, hunt_heads, (void *)thread
		))) {
			log_fatal(
				"pthread_create failed: cancelling thread %li. "
					"rc: %d.\n",
				thread->id, rc
			);
			// TODO: is pthread_cancel the right move here?
			pthread_cancel(thread->id);
			break;
		}
	}
	pthread_attr_destroy(&pthread_attr);
}

void
lal_serve_forever(
	const char	*host,
	const char	*port,
	int		(*fn)(void *arg),
	void		*extra
) {

	int request_socket = 0, queue_index = 0, i;
	struct lal_job *queue, *job;
	struct addrinfo request_addrinfo,
		*host_addrinfo = lal_get_host_addrinfo_or_die(
			host ? host : "localhost",
			port ? port : "80"
		);
	socklen_t request_socklen = sizeof(request_addrinfo);

	log_set_lock(lock_fn);

	queue = calloc(JOBNUM, sizeof(struct lal_job));
	for (i = 0; i < JOBNUM; i++)
		queue[i].socket = -1;

	struct lal_headhunter headhunter = {
		.hitcount = 0,
		.queue = queue,
		.job = fn,
		.extra = extra
	};

	listening_socket = lal_get_socket_or_die(host_addrinfo);

	lal_bind_and_listen_or_die(listening_socket, host_addrinfo);
	log_info("Lal serving at %s:%s", host, port);

	freeaddrinfo(host_addrinfo);

	signal(SIGINT, handleINT);

	spawn_workers();

	spawn_threads(&headhunter);

	/*
	 * Accepted `request_socket`s are added to the `headhunter`'s job queue in round
	 * robin fashion. If the `listening_socket` is closed (i.e., its value set
	 * to `-1`), then this function returns control to the client application.
	 * */
	while (~listening_socket) {

		request_socket = accept(
			listening_socket,
			(struct sockaddr *) &request_addrinfo,
			&request_socklen
		);

		/**
		 * Loop through the job queue until an open slot (`-1`) is found
		 * */
		while (~headhunter.queue[queue_index].socket) {
			queue_index = ++queue_index == JOBNUM
				? queue_index - JOBNUM
				: queue_index;
		}

		//pthread_mutex_lock(&mutex);
			job = &headhunter.queue[queue_index];
			job->socket = request_socket;
			job->hitcount = ++headhunter.hitcount;
			job->extra = headhunter.extra;
			//log_debug("hit %li", headhunter.hitcount);
		pthread_cond_signal(&cond);
		//pthread_mutex_unlock(&mutex);
	}

	pthread_cond_broadcast(&cond);
	for (i = 0; i < THREADNUM; i++)
		pthread_join(threads[i].id, NULL);
	log_info("%i failed requests", failed_requests);
	free(queue);
	return;
}
