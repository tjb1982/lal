#include "request.h"

const char *
lal_method_to_string(enum lal_http_method m)
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

const char *
lal_get_header_val(struct lal_request *request, const char *key)
{
    struct lal_entry *entry = request->header;
    do {
        if (strstr(key, entry->key) != NULL)
            return entry->val;
        entry = entry->next;
    } while (entry);
    return NULL;
}

char *
lal_read_header (int sock)
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
		fprintf(stderr, "read_header failed: \
			header exceeded MAXHEADERSIZE of %i", MAXHEADERSIZE);

	*ptr = '\0';

	header = (char *)malloc((len + 1) * sizeof(char));
	strcpy(header, buf);

	return header;
}

int
lal_set_entries (struct lal_request *request, char *src)
{
    struct lal_entry *entry;
    char *ptr, *line;
    char **save_ptr = &src;
    int nentries = 0;

		/* **TODO**: cleanup and refactor this so that we're not using `strtok` and since we're
		 * using `keylen` and `vallen`, there's not need to make these `'\0'`-terminated.
		 * */
    entry = malloc(sizeof(struct lal_entry));
    request->header = entry;

    line = strtok_r(src, "\r\n", save_ptr);
    for (;;) {
        if (strchr(line, ':')) {
            nentries++;

            ptr = line;
            while (*ptr != ':' && *ptr != '\0') {
                ptr++;
            }
            *ptr++ = '\0';

            entry->keylen = strlen(line);
            entry->key = malloc((entry->keylen + 1) * sizeof(char));
            strcpy(entry->key, line);

            while (*ptr == ' ')
                ptr++; /* skip the intervening spaces */
            entry->vallen = strlen(ptr);
            entry->val = malloc((entry->vallen + 1) * sizeof(char));
            strcpy(entry->val, ptr);

            if ((line = strtok_r(NULL, "\r\n", save_ptr)))
                entry = entry->next = malloc(sizeof(struct lal_entry));
            else {
                entry->next = NULL;
                break;
            }
        }
    };
    return nentries;
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

struct lal_request *
lal_create_request(char *src)
{
    char *header = src, *path;
    int pathlen = 0;
    enum lal_http_method method = lal_method_from_string(src);

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
        .header = NULL
    };

    request = (struct lal_request *)malloc(sizeof (struct lal_request));
    memcpy(request, &r, sizeof(struct lal_request));

    /* fast forward to the next line */
    while (*src != '\0' && *src++ != '\n')
        ;
    lal_set_entries(request, src);

    return request;
}

void
lal_destroy_request (struct lal_request *request)
{
    struct lal_entry *prev, *entry = request->header;

    while (entry) {
        free(entry->val);
        free(entry->key);
        prev = entry;
        entry = entry->next;
        free(prev);
    }

    free((void *)request->_raw_header);
    free(request);
}

