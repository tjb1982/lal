#include "response.h"

struct lal_response *
lal_create_response(const char *status)
{
	struct lal_response *resp = (struct lal_response *)malloc(
		sizeof(struct lal_response));
	int len = strlen(status);
	resp->status = (char *)calloc(sizeof(char), len + 1);
	memcpy(resp->status, status, len);
	resp->headers = lal_new_entry();
	resp->body = lal_new_body_part();

	return resp;
}

char *
lal_serialize_response(struct lal_response *resp, long long *len)
{
	char *r, *body_str, *header_str, content_length_str[100];
	struct lal_entry *entry = resp->headers;
	struct lal_body_part *header = lal_create_body_part("HTTP/1.1 ");
	lal_append_to_body(header, resp->status);
	lal_append_to_body(header, "\r\n");

	body_str = lal_join_body(resp->body, NULL);
	*len = strlen(body_str);
	sprintf(content_length_str, "%lli", *len);

	lal_append_to_entries(resp->headers, "Content-Length", content_length_str);

	while (entry) {
		lal_append_to_body(header, entry->key);
		lal_append_to_body(header, ": ");
		lal_append_to_body(header, entry->val);
		lal_append_to_body(header, "\r\n");
		entry = entry->next;
	}

	lal_append_to_body(header, "\r\n");

	header_str = lal_join_body(header, NULL);
	lal_destroy_body(header);

	*len = strlen(header_str) + *len;

	r = (char *)calloc(*len + 1, sizeof(char));
	strcpy(r, header_str);
	strcat(r, body_str);

	free(header_str);
	free(body_str);

	return r;
}

void
lal_destroy_response(struct lal_response *resp)
{
	struct lal_entry *prev, *entry = resp->headers;

	while (entry) {
		free((void *)entry->val);
		free((void *)entry->key);
		prev = entry;
		entry = entry->next;
		free(prev);
	}

	lal_destroy_body(resp->body);

	free(resp->status);
	free(resp);

}
