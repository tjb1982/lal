#include "route.h"
#include "utils.h"
#include "request.h"

#ifndef RESPONSE_H_
#define RESPONSE_H_
struct lal_response {
	char *status;
	struct lal_entry *headers;
	struct lal_body_part *body;
};

struct lal_response *
lal_create_response(const char *status);

char *
lal_serialize_response(struct lal_response *resp, long long *len);

void
lal_destroy_response(struct lal_response *resp);
#endif // RESPONSE_H_
