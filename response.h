#include "route.h"
#include "utils.h"
#include "request.h"

struct lal_response {
    char *status;
    struct lal_entry *headers;
    struct lal_body_part *body;
};

struct lal_response *
lal_create_response(const char *status);

char *
lal_serialize_response(struct lal_response *resp);

void
lal_destroy_response(struct lal_response *resp);
