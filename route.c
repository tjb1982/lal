#include "route.h"

static struct lal_route *routes;

void *
lal_route_request (void *arg)
{
    struct handler_args *args = arg;
    char *header;
    struct lal_request *request;
    struct lal_route *route;

    header = lal_read_header(args->socket);
    request = lal_create_request(header);

    if (!~request->method) {
        syslog(LOG_ERR, "route_request failed: method %s not implemented\n",
               strtok(header, " "));
    }

    free(header);

    route = lal_get_route(request);

    if (route)
        (void) route->handler(request, args->socket);
    else
        syslog(LOG_ERR, "lal_route_request failed: route not found");

    lal_destroy_request(request);
    close(args->socket);

    args->ready = 1;
    pthread_exit(NULL);
    return NULL;
}

void
lal_register_route (enum lal_http_method method, char *path,
               int(*handler)(struct lal_request *, int))
{
    struct lal_route *route;

    if (routes == NULL)
        routes = route = malloc(sizeof(struct lal_route));
    else {
        route = routes;
        while (route->next)
            route = route->next;

        route->next = malloc(sizeof(struct lal_route));
        route = route->next;
    }

    if (path) {
        route->path = malloc((strlen(path) + 1) * sizeof(char));
        strcpy(route->path, path);
    }
    else
        route->path = NULL;

    route->method = method;
    route->handler = handler;
    route->next = NULL;

    syslog(LOG_INFO, "route created: %s, %i, %p\n",
           route->path, route->method, (void *)route->handler);
}

struct lal_route *
lal_get_route (struct lal_request *request)
{
    struct lal_route *route = routes;
    char *route_pos, *request_pos;

    while (route) {
        if (!route->path)
            return route;
        route_pos = route->path;
        request_pos = request->path;
compare:
        while (*route_pos == *request_pos) {
            if (*request_pos == '\0') {
                if (route->method == request->method ||
                    route->method == ANY) {
                    return route;
                }
                else
                    break;
            }
            else {
                route_pos++;
                request_pos++;
            }
        }

        if (*request_pos == '?' &&
            route->method == request->method) {
            return route;
        }

        if (*route_pos == ':') {
            int contains_param = 0;
            while (*route_pos != '/' && *route_pos != '\0')
                route_pos++;
            while (*request_pos != '/' && *request_pos != '\0') {
                contains_param = 1;
                request_pos++;
            }
            if (contains_param && *route_pos == *request_pos)
                goto compare;
        }
        route = route->next;
    }
    return NULL;
}

void
lal_destroy_routes ()
{
    struct lal_route *prev;
    int i = 0;
    while (routes) {
        free(routes->path);
        prev = routes;
        routes = routes->next;
        free(prev);
        i++;
    }
    syslog(LOG_INFO, "destroyed %i routes\n", i);
}
