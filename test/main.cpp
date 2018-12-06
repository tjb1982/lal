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

std::string dbconfig("postgresql://lal:lal@laldb:5432/lal");

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


int
respond(int sock, struct lal_response *resp) {
	long long len;
	uint8_t *response = (uint8_t*)lal_serialize_response(resp, &len);
	send(sock, response, len, 0);
	free(response);
	lal_destroy_response(resp);
	return 0;
}


int
respond_with_posts(int sock, std::vector<Post> &posts) {
	int i = 0;
	auto ss = std::stringstream();
	char uuid_str[37];

	ss << "[";

	for (; i < posts.size(); i++) {
		Post p = posts[i];
		uuid_unparse(p.id, uuid_str);
		struct tm *tm = localtime(&p.created);
		char created[100];
		strftime(created, 100, "%Y-%m-%d %H:%M:%S", tm);
		ss	<< "{"
				<< "\"title\":\"" << p.title << "\","
				<< "\"body\":\"" << p.body << "\","
				<< "\"id\":\"" << uuid_str << "\","
				<< "\"created\":\"" << created << "\""
			<< "}" << (i+1!=posts.size() ? "," : "");
	}

	ss << "]";

	struct lal_response *resp = lal_create_response("200 OK");
	lal_append_to_entries(resp->headers,
		"Content-Type", "application/json");

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
post_middleware(int sock, struct lal_request *request, int *clptr) {

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
	log_trace("Content length is: %i", content_length);
	*clptr = content_length;

	if (content_length > (1 << 12)) {
		respond_500(sock, "Content-Length must be less than or equal to 4096 bytes.");
		return 1;
	}

	return 0;
}


int
insert_post(int sock, struct lal_request *request) {

	int content_length;
	if (post_middleware(sock, request, &content_length)) {
		return 1;
	}

	// recv content_length from sock into buffer
	char buffer[content_length + 1];
	recv(sock, buffer, content_length, 0);
	buffer[content_length] = '\0';
	// parse as json
	json j = json::parse(buffer);
	log_info("JSON received: %s", j.dump().c_str());

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
			insert into post (id, title, body) values ($1, $2, $3)
		));

		prepare::invocation i(t.prepared(id_str));
		i(id_str);
		i(j.at("body").get<std::string>());
		i(j.at("title").get<std::string>());
		auto r = i.exec();

		t.commit();
		c.disconnect();

		json j = {
			{"affected-rows": r.affected_rows()},
			{"instance": 
		};

		struct lal_response *resp = lal_create_response("201 Created");
		lal_append_to_entries(resp->headers,
			"Content-Type", "application/json");

		auto dump = j.dump();
		lal_nappend_to_body(resp->body,
			(const uint8_t *)dump().c_str(), dump.size());

		return respond(sock, resp);
	} catch (const std::exception &e) {
		log_error("Caught: %s", e.what());
		return 1;
	}
}


int
get_posts (int sock, struct lal_request *request) {
	auto post_vector = std::vector<Post>();
	result::size_type i = 0;

	try {
		connection c(dbconfig.c_str());
		if (!c.is_open()) {
			const char *msg = "Can't connect to database.";
			log_error(msg);
			respond_500(sock, msg);
			return 1;
		}

		work t(c);
		result results = t.exec("select * from post");

		t.commit();
		c.disconnect();
		
		while (i != results.size()) {
			struct std::tm tm;
			auto r = results[i++];
			std::istringstream ss(r["created"].c_str());
			ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S.%u");
			Post p(
				r["id"].c_str(),
				r["title"].c_str(),
				r["body"].c_str(),
				mktime(&tm)
			);
			post_vector.push_back(p);
		}

		return respond_with_posts(sock, post_vector);
	} catch (const std::exception &e) {
		std::stringstream errstr;
		errstr << "Caught: " << e.what() << endl;
		auto msg = errstr.str();
		log_fatal(msg.c_str());
		respond_500(sock, msg.c_str());
		return 1;
	}
}

int
ensure_schema() {
	try {
		connection c(dbconfig.c_str());
		if (!c.is_open()) {
			cout << "Can't open database" << endl;
			return 1;
		}

		const char *sql = "create table if not exists post ("
			"id uuid primary key,"
			"title text not null,"
			"body text not null,"
			"created timestamp default now()"
		")";

		work t(c);
		t.exec(sql);
		t.commit();
		c.disconnect();
		return 0;
	} catch (const std::exception &e) {
		std::stringstream errstr;
		errstr << "Caught: " << e.what() << endl;
		log_fatal(errstr.str().c_str());
		return 1;
	}
}

int
main(int argc, char **argv)
{
	struct lal_route *routes;

	if (argc > 3)
		dbconfig = dbconfig.replace(dbconfig.find("laldb"), 5, argv[3]);

	ensure_schema();

	routes = lal_init_routes();
	lal_register_route(routes, GET, "/posts/", get_posts);
	lal_register_route(routes, POST, "/posts/", insert_post);
	lal_serve_forever(
		(argc > 1 ? argv[1] : "localhost"),
		(argc > 2 ? argv[2] : "5000"),
		lal_route_request,
		routes
	);
	lal_destroy_routes(routes);

	return 0;
}
