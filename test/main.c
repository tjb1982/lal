#include <stdio.h>
#include <lal/route.h>
#include <lal/request.h>
#include <lal/response.h>
#include <lal/network.h>

struct post {
	char id[36]; // uuid
	struct tm created;
	char *post;
};

int
say_something(int sock, struct lal_request *request)
{

	char msg[21];
	sscanf(request->path, "/hello/%20[^/]", msg);

	struct lal_response *resp = lal_create_response("200 OK");

	lal_append_to_entries(resp->headers, "Content-Type", "text/plain; charset=utf-8");
	lal_append_to_entries(resp->headers, "Server", "lal");
	lal_append_to_body(resp->body, msg);

	long long len;
	char *response = lal_serialize_response(resp, &len);
	send(sock, response, len, 0);

	free(response);
	lal_destroy_response(resp);

	return 0;
}

int
main(int argc, char **argv)
{
	struct lal_route *routes;
	routes = lal_init_routes();
	lal_register_route(routes, GET, "/hello/:message/", say_something);
	lal_register_route(routes, GET, "/hello/:message/foo", say_something);
	lal_serve_forever(
		(argc > 1 ? argv[1] : "localhost"),
		(argc > 2 ? argv[2] : "5000"),
		lal_route_request,
		routes
	);
	lal_destroy_routes(routes);

	return 0;
}
