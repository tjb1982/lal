#include <stdio.h>
#include <uuid/uuid.h>
#include <lal/route.h>
#include <lal/request.h>
#include <lal/response.h>
#include <lal/network.h>
#include <lal/log.h>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <pqxx/pqxx>
#include "json.hpp"

#define QUOTE(...) #__VA_ARGS__

using namespace pqxx;
using namespace std;
using json = nlohmann::json;

std::string dbconfig = "dbname = lal user = lal password = lal "
	"host = laldb port = 5432";

int respond(int sock, struct lal_response *resp) {

	long long len;
	char *response = lal_serialize_response(resp, &len);
	send(sock, response, len, 0);
	free(response);
	lal_destroy_response(resp);

	return 0;
}

struct Post {
public:
	uuid_t id;
	std::string title;
	std::string body;
	time_t created;

	Post(
		std::string id_str,
		std::string title,
		std::string body,
		time_t created
	) : title(title), body(body), created(created) {
		uuid_parse(id_str.c_str(), id);
	}
};

int say_test(int sock, struct lal_request *request) {
	int x = 10;
	auto ss = std::stringstream();
	char uuid_str[37];

	ss << "[";

	while(x--) {
		Post p(
			"88ffab8b-8f75-48a8-8680-159fe121621e",
			"this is a test",
			"a test in working with C++ to create an API",
			time(NULL)
		);
		uuid_unparse(p.id, uuid_str);
		ss	<< "{"
				<< "\"title\":\"" << p.title << "\","
				<< "\"body\":\"" << p.body << "\","
				<< "\"id\":\"" << uuid_str << "\","
				<< "\"created\":\"" << p.created << "\""
			<< "}" << (x ? "," : "");
	}

	ss << "]";

	struct lal_response *resp = lal_create_response("200 OK");
	lal_append_to_entries(resp->headers,
		"Content-Type", "application/json; charset=utf-8");

	lal_nappend_to_body(resp->body,
		(const uint8_t *)ss.str().c_str(), ss.str().size());

	return respond(sock, resp);
}

int
respond_500 (int sock, const char *msg) {
	struct lal_response *resp = lal_create_response("500 Bad");
	json j = {
		{"message", msg}
	};
	auto dump = j.dump();
	lal_nappend_to_body(resp->body,
		(const uint8_t *)dump.c_str(), dump.size());
	respond(sock, resp);
	return 1;
}

int
middleware(int sock, struct lal_request *request, int *clptr) {

	struct lal_entry *header;

	// Get Content-Type header, value == application/json
	if (!(header = lal_get_header(request, "Content-Type"))) {
		return respond_500(sock, "Content-Type header was empty");
	}
	char content_type_str[header->vallen + 1];
	strncpy(content_type_str, header->val, header->vallen);
	if (strstr(content_type_str, "application/json") != content_type_str) {
		return respond_500(sock, "Content-Type must be application/json");
	}

	// Get Content-Length header, value
	if (!(header = lal_get_header(request, "Content-Length"))) {
		return respond_500(sock, "Content-Length header was empty");
	}
	char content_header_str[header->vallen + 1];
	strncpy(content_header_str, header->val, header->vallen);
	// Convert content_length to int
	int content_length = std::stoi(content_header_str);
	log_fatal("content length is: %i", content_length);
	*clptr = content_length;
	return 0;
}

int
insert_post(int sock, struct lal_request *request) {

	int content_length;
	if (middleware(sock, request, &content_length)) {
		return 1;
	}

	log_trace("%i", content_length);
	// recv content_length from sock into buffer
	char buffer[content_length + 1];
	recv(sock, buffer, content_length, 0);
	buffer[content_length] = '\0';
	// parse as json
	json j = json::parse(buffer);
	log_info("json string: %s", j.dump().c_str());

	// use it in pqxx try block
	auto ss = stringstream();
	try {
		connection c(dbconfig.c_str());
		if (!c.is_open()) {
			log_error("Can't open database");
			return 1;
		}

		uuid_t id;
		uuid_generate_time_safe(id);
		char id_str[37];
		uuid_unparse_lower(id, id_str);

		work t(c);
		c.prepare(id_str, QUOTE(
			insert into post (id, body) values ($1, $2)
		));

		prepare::invocation i(t.prepared(id_str));
		i(id_str);
		i(j.at("body").get<std::string>());
		auto r = i.exec();

		t.commit();
		c.disconnect();

		ss << "Affected rows: " << r.affected_rows();

		struct lal_response *resp = lal_create_response("200 OK");
		lal_append_to_entries(resp->headers,
			"Content-Type", "application/json; charset=utf-8");

		lal_nappend_to_body(resp->body,
			(const uint8_t *)ss.str().c_str(), ss.str().size());

		return respond(sock, resp);
	} catch (const std::exception &e) {
		log_error("Caught: %s", e.what());
		return 1;
	}
}

int
create_schema(std::string &dbconfig) {
	try {
		connection C(dbconfig.c_str());
		if (!C.is_open()) {
			cout << "Can't open database" << endl;
			return 1;
		}

		const char *sql = "create table if not exists post ("
			"id uuid primary key,"
			"body text not null,"
			"created timestamp default now()"
		")";

		work W(C);
		W.exec(sql);
		W.commit();
		C.disconnect();
		return 0;
	} catch (const std::exception &e) {
		cerr << "Caught: " << e.what() << endl;
		return 1;
	}
}

int
main(int argc, char **argv)
{
	struct lal_route *routes;

	create_schema(dbconfig);

	routes = lal_init_routes();
	lal_register_route(routes, GET, "/", say_test);
	lal_register_route(routes, POST, "/post", insert_post);
	lal_serve_forever(
		(argc > 1 ? argv[1] : "localhost"),
		(argc > 2 ? argv[2] : "5000"),
		lal_route_request,
		routes
	);
	lal_destroy_routes(routes);

	return 0;
}
