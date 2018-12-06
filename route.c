#include "route.h"
#include "response.h"

int
resp_404(
	struct lal_request	*request,
	struct lal_job		*job,
	const char		*msg
) {
	char path[request->pathlen + 1];
	snprintf(path, request->pathlen + 1, request->path);
	//log_error("Responding with HTTP 404: lal_route_request failed: %s: %s", msg, path);

	struct lal_response *resp = lal_create_response("404 Not found");
	lal_append_to_entries(resp->headers, "Content-Type", "text/plain; charset=utf-8");
	lal_append_to_entries(resp->headers, "Server", "lal");
	lal_append_to_body(resp->body, msg);
	lal_append_to_body(resp->body, ": ");
	lal_append_to_body(resp->body, path);

	long long len;
	char *response = lal_serialize_response(resp, &len);
	send(job->socket, response, len, 0);

	free(response);
	lal_destroy_response(resp);
	if (~request->method)
		lal_destroy_request(request);
	return EXIT_FAILURE;
}

int
lal_route_request (void *arg)
{
	LAL_HEADER_ERROR header_error;
	struct lal_job *job = arg;
	char *header = NULL;
	struct lal_request *request;
	struct lal_route *route, *routes = (struct lal_route *)job->extra;

	if ((header_error = lal_parse_header(job->socket, &header))) {
		log_warn(
			"Couldn't parse header: (%li) %s.",
			job->hitcount,
			header_error == RECV_FAILED
				? strerror(errno)
				: lal_header_error_msg(header_error)
		);
		struct lal_request q = {
			.path = "(null)",
			.pathlen = 6,
			.method = -1
		};
		return resp_404(&q, job, lal_header_error_msg(header_error));
	}
	request = lal_create_request(header);

	if (request == NULL) {
		log_error(
			"`request` was NULL: socket: %i; header: \"%s\"",
			job->socket, header
		);
		return EXIT_FAILURE;
	}

	if (!~request->method) {
		log_error("route_request failed: method %s not implemented",
			lal_method_to_string(request->method));
		lal_destroy_request(request);
		return EXIT_FAILURE;
	}

	route = lal_get_route(routes, request);

	if (route) {
		request->extra = route->extra;
		(void) route->handler(job->socket, request);
	}
	else {
		return resp_404(request, job, "Thank you for my life");
	}

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
	if (!route->path) {
		log_error(
			"Route for method \"%s\" registered without a `path`",
			lal_method_to_string(route->method)
		);
		return;
	}
	char path[route->pathlen + 1];
	snprintf(path, route->pathlen + 1, route->path);
	log_info(
		"%s, %s, 0x%02x",
		path, lal_method_to_string(route->method), (void *)route->handler
	);
}

void
lal_register_route (
	struct lal_route *routes,
	const enum lal_http_method method,
	const char *path,
	int(*handler)(int, struct lal_request *),
	void *extra
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
		.next = NULL,
		.extra = extra
	};

	memcpy(route, &r, sizeof(r));

	log_info(
		//"Route registered: %s, %s, 0x%x",
		"Route registered: %s, %s, handler: %p, extra: %p",
		path, lal_method_to_string(route->method), (void *)route->handler, route->extra
	);
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
