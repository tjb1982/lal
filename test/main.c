#include <stdio.h>
#include <lal/route.h>
#include <lal/request.h>
#include <lal/response.h>
#include <lal/network.h>

//struct post {
//	char id[36]; // uuid
//	struct tm created;
//	char *post;
//};

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

int respond(int sock, struct lal_response *resp) {

	long long len;
	char *response = lal_serialize_response(resp, &len);
	send(sock, response, len, 0);
	free(response);
	lal_destroy_response(resp);

	return 0;
}

#define QUOTE(...) #__VA_ARGS__

struct post {
	char id[37]; // i.e., uuid + \0
	char *title;
	char *body;
	time_t created;
};

int say_test(int sock, struct lal_request *request) {
	const char *format = QUOTE({
		"title":"%s",
		"body":"%s",
		"id":"%s",
		"created":"%li"
	}%s);
	struct lal_response *resp = lal_create_response("200 OK");
	lal_append_to_entries(resp->headers,
		"Content-Type", "application/json; charset=utf-8");

	lal_nappend_to_body(resp->body, "[", 1);

	int x = 10;
	while(x--) {
		struct post p = {
			.id = "88ffab8b-8f75-48a8-8680-159fe121621e",
			.title = "this is a test",
			.body = "a test in working with C to create an API",
			.created = time(NULL)
		};
		int max = 255;
		char buffer[max];
		snprintf(buffer, max - 1, format,
			p.title, p.body, p.id, p.created, x ? "," : ""
		);
		if (buffer[max - 1] == '\0') {
			lal_append_to_body(resp->body, buffer);
		} else {
			lal_destroy_response(resp);
			resp = lal_create_response("500 Internal Server Error");
			lal_append_to_body(resp->body, QUOTE({
				"error":"Internal Server Error",
				"message":"resource too large",
				"http_status":"500"
			}));
			return respond(sock, resp);
		}
	}

	lal_nappend_to_body(resp->body, "]", 1);

	return respond(sock, resp);
}

int
main(int argc, char **argv)
{
	struct lal_route *routes;
	routes = lal_init_routes();
	lal_register_route(routes, GET, "/", say_test);
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
