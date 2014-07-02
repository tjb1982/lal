#include "response.h"

struct lal_response *
lal_create_response(const char *status)
{
    struct lal_response *resp = malloc(sizeof(struct lal_response));
    resp->status = malloc(sizeof(char) * (strlen(status) + 1));
    strcpy(resp->status, (status ? status : "200 OK"));
    resp->entries = lal_create_entry();
    resp->body = lal_create_body_part();

    return resp;
}

char *
lal_serialize_response(struct lal_response *resp)
{
    char *r;
    struct lal_body_part *header = lal_create_body_part();

    lal_append_to_body(header, "HTTP/1.1 ");
    lal_append_to_body(header, resp->status);
    lal_append_to_body(header, "\r\n");

    struct lal_entry *entry = resp->entries;
    while (entry) {
        lal_append_to_body(header, entry->key);
        lal_append_to_body(header, ": ");
        lal_append_to_body(header, entry->val);
        lal_append_to_body(header, "\r\n");
        entry = entry->next;
    }

    lal_append_to_body(header, "\r\n");

    char *header_str = lal_join_body(header, NULL);
    lal_destroy_body(header);
    char *body_str = lal_join_body(resp->body, NULL);

    r = malloc((strlen(header_str) + strlen(body_str) + 1) * sizeof(char));
    strcpy(r, header_str);
    strcat(r, body_str);

    free(header_str);
    free(body_str);

    return r;
}

void
lal_destroy_response(struct lal_response *resp)
{
    struct lal_entry *prev, *entry = resp->entries;

    while (entry) {
        free(entry->val);
        free(entry->key);
        prev = entry;
        entry = entry->next;
        free(prev);
    }

    lal_destroy_body(resp->body);

    free(resp->status);
    free(resp);

}
