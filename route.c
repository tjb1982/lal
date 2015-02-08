#include "route.h"

int
lal_route_request (void *arg)
{
	struct lal_thread *thread = arg;
	struct lal_request *request = lal_create_request(
		lal_read_header(thread->socket)
	);
	struct lal_route *route;

	if (request == NULL)
		return EXIT_FAILURE;

	if (!~request->method) {
		fprintf(stderr, "route_request failed: method %s not implemented\n",
			lal_method_to_string(request->method));
		lal_destroy_request(request);
		return EXIT_FAILURE;
	}

	route = lal_get_route((struct lal_route *)thread->extra, request);
//	write(1, route->path, route->pathlen);
//	printf(", %i, %p\n", route->method, route->handler);

	if (route)
		(void) route->handler(thread->socket, request);
	else
		fprintf(stderr, "lal_route_request failed: route not found");

	lal_destroy_request(request);

	return EXIT_SUCCESS;
}

struct lal_route *
lal_init_routes()
{
	struct lal_route r = {
		.path = NULL,
		.pathlen = 0,
		.method = 0,
		.handler = NULL,
		.next = NULL
	}, *routes = (struct lal_route *) malloc(
		sizeof(r)
	);
	memcpy(routes, &r, sizeof(r));
	return routes;
}

void
print_route(const struct lal_route *route)
{
	if (route->path)
		write(1, route->path, route->pathlen);
	else write(1, "(nil)", 5);
	fprintf(stderr, ", %i, %p\n", route->method, (void *)route->handler);
}

void
lal_register_route (
	struct lal_route *routes,
	const enum lal_http_method method,
	const char *path,
	int(*handler)(int, struct lal_request *)
) {
	struct lal_route *route = routes;
	size_t pathlen = 0;
	char *path_copy;

	if (!handler) {
		fprintf(stderr,
			"lal_register_route failed: No handler specified\n");
	}

	while (route->next)
		route = route->next;

	if (route->handler) {
		route->next = (struct lal_route *)malloc(sizeof(struct lal_route));
		route = route->next;
	}

	if (path) {
		pathlen = strlen(path);
		path_copy = (char *) malloc(
			pathlen * sizeof(char)
		);
		memcpy((void *)path_copy, path, pathlen);
	}
	else {
		path_copy = NULL;
		pathlen = 0;
	}

	struct lal_route r = {
		.method = method,
		.handler = handler,
		.pathlen = pathlen,
		.path = path_copy,
		.next = NULL
	};

	memcpy(route, &r, sizeof(r));

	fprintf(stderr, "route registered: ");
	print_route(route);
}

bool
methods_match(
	const struct lal_request *request,
	const struct lal_route *route
) {
	return route->method == ANY || request->method == route->method;
}

bool path_exhausted(const char *pos, const char *path, size_t len)
{ return pos - path == len; }

struct lal_route *
lal_get_route (struct lal_route *routes, struct lal_request *request)
{
	struct lal_route *route = routes;

	while (route) {

		if (!methods_match(request, route))
			goto next_route;

		if (route->path == NULL)
			return route;

		const char *rpos = route->path, *qpos = request->path;
		bool route_exhausted = false;
		bool request_exhausted = false;

compare:
		while (*rpos++ == *qpos++) {
			route_exhausted = path_exhausted(rpos, route->path, route->pathlen);
			request_exhausted = path_exhausted(qpos, request->path, request->pathlen);

			if (route_exhausted && request_exhausted)
				return route;
			else if (route_exhausted || request_exhausted) {
				if (route_exhausted && *qpos == '?')
					return route;
				else goto next_route;
			}
		}

		/*
		 * It's at this point where the two paths diverge. They both have length
		 * remaining, but they no longer match. The goal now is to find out whether
		 * we've encountered a route argument (`:`) or not; if so, fast-forward to the
		 * next `'/'` in both `rpos` and `qpos`. If either one is exhausted before reaching
		 * a `'/'`, `goto next_route`.
		 * */

		rpos--; qpos--;

		if (*qpos == '?') goto next_route;

		if (*rpos == ':') {

			while (!path_exhausted(rpos, route->path, route->pathlen) && *rpos != '/')
				rpos++;

			while (!path_exhausted(qpos, request->path, request->pathlen) && *qpos != '/')
				qpos++;

			route_exhausted = path_exhausted(rpos, route->path, route->pathlen);
			request_exhausted = path_exhausted(qpos, request->path, request->pathlen);

			if (route_exhausted && request_exhausted)
				return route;
			else if (*rpos == '/' && *qpos == '/')
				goto compare;
			else if (*rpos == '/' || *qpos == '/') {
				if (request->pathlen - (qpos - request->path) > 1)
					goto next_route;
				else return route;
			}
			else goto next_route;

		}

next_route:
		route = route->next;
	}
	return NULL;
}

//struct lal_route *
//lal_get_route (struct lal_route *routes, struct lal_request *request)
//{
//    struct lal_route *route = routes;
//    const char *route_pos, *request_pos;
//
//    while (route) {
//        if (!route->path) {
//            if (route->method == request->method ||
//                route->method == ANY)
//                return route;
//            else return NULL;
//	}
//        route_pos = route->path;
//        request_pos = request->path;
//compare:
//        while (*route_pos++ == *request_pos++) {
//            int traversed = request_pos - request->path;
//            if (traversed == request->pathlen &&
//                route_pos - route->path == route->pathlen) {
//                if (route->method == request->method ||
//                    route->method == ANY) {
//                    return route;
//                }
//                else break;
//            }
//        }
//
//        if (*request_pos == '?' &&
//            (route->method == request->method ||
//	    route->method == ANY)) {
//            return route;
//        }
//
//printf("*route_pos: %c\n", *route_pos);
//
//        if (*route_pos == ':') {
//write(1, request->path, request->pathlen);
//puts("");
//write(1, route->path, route->pathlen);
//puts("");
//            int contains_param = 0;
//            while (*route_pos != '/' && *route_pos != '\0')
//                route_pos++;
//            while (*request_pos != '/' &&
//                   request_pos - request->path < request->pathlen) {
//                contains_param = 1;
//                request_pos++;
//            }
//            if (contains_param && *route_pos == *request_pos)
//                goto compare;
//            else return route;
//        }
//        route = route->next;
//    }
//    return NULL;
//}

void
lal_destroy_routes (struct lal_route *routes)
{
	struct lal_route *prev;
	int i = 0;
	while (routes) {
		free((void *)routes->path);
		prev = routes;
		routes = routes->next;
		free(prev);
		i++;
	}
	fprintf(stderr, "destroyed %i routes\n", i);
}
