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

    status = listen(sock, BOCKLOG);

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

    hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP */

    status = getaddrinfo(
        hostname, port, &hints, &host
    );

    if (status != 0)
        syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(status));

    return host;

}

void
lal_serve_forever(void *(*socket_handler)(void *arg),
                  const char *host,
                  const char *port,
                  int daemonize,
                  int threaded)
{
    int /*pid, */listening_socket, request_socket, hitcount = 0;

    struct addrinfo request_addrinfo,
                    *host_addrinfo = lal_get_host_addrinfo_or_die(
                        host ? host : "localhost",
                        port ? port : "8080"
                    );

    socklen_t request_addrinfo_socklen = sizeof request_addrinfo;


    listening_socket = lal_get_socket_or_die(host_addrinfo);
    (void) lal_bind_and_listen_or_die(listening_socket, host_addrinfo);

    freeaddrinfo(host_addrinfo);

    if (daemonize)
        if (fork() != 0) exit(0); /* daemonize */
    (void) signal(SIGCLD, SIG_IGN); /* ignore child death */
    (void) signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
    (void) setpgid(0,0); /* break away from process group */

    struct handler_args *args = calloc(
        THREADNUM,
        sizeof(struct handler_args)
    );

    while (++hitcount) {

        request_socket = accept(
            listening_socket,
            (struct sockaddr *) &request_addrinfo,
            &request_addrinfo_socklen
        );

        int id = 0;

        while (args[id].thread && !args[id].ready) {
            time_t now; time(&now);
            if (now - args[id].created > THREAD_TIMEOUT) {
                pthread_cancel(args[id].thread);
                break;
            }
            id = ++id == THREADNUM ? id - THREADNUM : id;
        }

        /*if (args[id].thread) {
            int perr = pthread_join(args[id].thread, NULL);
            if (perr) {
                syslog(LOG_ERR, "pthread_join: %s", strerror(perr));
                pthread_cancel(args[id].thread);
            }
        }*/

        args[id].socket = request_socket;
        args[id].hitcount = hitcount;
        args[id].ready = 0;
        args[id].thread = 0;
        time(&args[id].created);

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 32768);
//        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        pthread_create(&args[id].thread, &attr, socket_handler, &args[id]);

        pthread_attr_destroy(&attr);

        printf("thread created %i\n", id);
        /*else {

            pid = fork();

            if (pid < 0) {
                syslog(LOG_ERR, "fork failed: %s", strerror(errno));
                exit(1);
            }

            if (pid == 0) {
                // child process
                (void) close(listening_socket);
                (void) socket_handler (&request_socket);
                (void) close(request_socket);
                exit(1);
            }
            else
                // parent process
                (void) close(request_socket);

        }*/

    }
}
