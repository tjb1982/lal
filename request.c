#include "request.h"

const char
*lal_method_to_string(enum lal_http_method m)
{
	return m == GET ? "GET"
	: m == POST ? "POST"
	: m == PUT ? "PUT"
	: m == PATCH ? "PATCH"
	: m == DELETE ? "DELETE"
	: m == HEAD ? "HEAD"
	: m == OPTIONS ? "OPTIONS"
	: "ANY";
}

struct lal_entry
*lal_get_header (struct lal_request *request, const char *key)
{
	struct lal_entry *entry = request->header;
	do {
		if (
			strlen(key) == entry->keylen &&
			!strncmp(key, entry->key, entry->keylen)
		) return entry;
		entry = entry->next;
	} while (entry);
	return NULL;
}

const char
*lal_parse_header (int sock)
{
	int nbytes, len = 0;
	char buf[MAXHEADERSIZE], *ptr = buf, *header;

	while (ptr - buf < MAXHEADERSIZE - 1 &&
	       (ptr - buf < 4 || strncmp(ptr - 4, "\r\n\r\n", 4) != 0) &&
	       (nbytes = recv(sock, ptr++, 1, 0)) != 0) {
		if (nbytes < 0)
			fprintf(stderr, "recv() failed: %s", strerror(errno));
		len += nbytes;
	}

	if (ptr - buf == MAXHEADERSIZE - 1 && strstr(buf, "\r\n\r\n") == NULL)
		fprintf(stderr, "lal_parse_header: "
			"header exceeded MAXHEADERSIZE of %i", MAXHEADERSIZE);

	*ptr = '\0';

	header = malloc((len + 1) * sizeof(char));
	memcpy(header, buf, len);
	header[len] = 0;

	return header;
}

void
lal_set_headers (struct lal_request *request, const char *src)
{
	struct lal_entry *entry = malloc(sizeof(struct lal_entry));
	request->header = entry;

	entry->key = entry->val = NULL;
	entry->next = NULL;

	while (*src != '\0') {
		entry->keylen = entry->vallen = 0;
		entry->key = src;
		while (*src != '\0' && *src++ != ':')
			entry->keylen++;
		if (*src == ' ')
			src++;
		entry->val = src;
		while (*src != '\0' && *src++ != '\n')
			entry->vallen++;

		if (*src == '\0')
			entry->next = NULL;
		else
			entry = entry->next = malloc(sizeof(struct lal_entry));
	}
}

enum lal_http_method
lal_method_from_string(const char *src)
{
	return strnstr(src, "GET", 3) == src ? GET
	: strnstr(src, "POST", 4)  == src ? POST
	: strnstr(src, "PUT", 3)  == src ? PUT
	: strnstr(src, "DELETE", 6)  == src ? DELETE
	: strnstr(src, "OPTIONS", 7)  == src ? OPTIONS
	: strnstr(src, "HEAD", 4)  == src ? HEAD
	: -1;
}

struct lal_request
*lal_create_request(const char *src)
{
	const char *header = src, *path;
	int pathlen = 0;
	enum lal_http_method method = lal_method_from_string(src);
	
	if (!~method)
		return NULL;
	
	/* skip over the method */
	while (*src++ != ' ')
	    ;
	
	path = src;
	while (*src++ != ' ')
		pathlen++;
	
	struct lal_request *request, r = {
		.method = method,
		._raw_header = header,
		.path = path,
		.pathlen = pathlen,
		.header = NULL,
		.content = NULL,
		.content_length = 0
	};
	
	request = malloc(sizeof (struct lal_request));
	memcpy(request, &r, sizeof(struct lal_request));
	
	/* fast forward to the next line */
	while (*src != '\0' && *src++ != '\n')
		;
	
	lal_set_headers(request, src);
	
	return request;
}

void
lal_set_content(int sock, struct lal_request *request)
{
	struct lal_entry *e = lal_get_header(request, "Content-Length");
	if (e) {
		char buf[e->vallen + 1];
		memcpy(buf, e->val, e->vallen); buf[e->vallen] = 0;
		int content_length = strtol(
			buf, NULL, 10
		);
		uint8_t *content = malloc(sizeof(uint8_t) * content_length);
		recv(sock, content, content_length, 0);
		request->content = content;
		request->content_length = content_length;
	}
}

void
lal_destroy_request (struct lal_request *request)
{
	struct lal_entry *prev, *entry = request->header;
	
	while (entry) {
		prev = entry;
		entry = entry->next;
		free(prev);
	}
	
	if (request->content != NULL)
		free((void *)request->content);
	
	free((void *)request->_raw_header);
	free(request);
}

