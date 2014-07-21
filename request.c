#include "request.h"

char *
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

char *
lal_get_header_val(struct lal_request *request, const char *key)
{
    struct lal_entry *entry = request->entries;
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
    int nbytes;
    char buf[MAXHEADERSIZE] = "", *ptr = buf, *header;

    while (ptr - buf < MAXHEADERSIZE - 1 &&
           strcmp(ptr - 4, "\r\n\r\n") != 0 &&
           (nbytes = recv(sock, ptr++, 1, 0)) != 0)
        if (nbytes < 0)
            syslog(LOG_ERR, "recv() failed: %s", strerror(errno));

    if (ptr - buf == MAXHEADERSIZE - 1 && strstr(buf, "\r\n\r\n") == NULL)
        syslog(LOG_ERR, "read_header failed: \
               header exceeded MAXHEADERSIZE of %i", MAXHEADERSIZE);

    *ptr = '\0';

    if ((header = malloc((strlen(buf) + 1) * sizeof(char))) == NULL)
        syslog(LOG_ERR, "malloc failed: %s", strerror(errno));

    strcpy(header, buf);

    return header;
}

void
lal_set_entries (struct lal_request *request, char *src)
{
    struct lal_entry *entry;
    char *ptr, *line;
    char **save_ptr = &src;
    int nentries = 0;

    entry = malloc(sizeof(struct lal_entry));
    request->entries = entry;

    line = strtok_r(src, "\r\n", save_ptr);
    for (;;) {
        if (strstr(line, ":")) {
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
    request->nentries = nentries;
}

enum lal_http_method
lal_method_from_string(const char *src)
{
    return strstr(src, "GET") == src ? GET
    : strstr(src, "POST")  == src ? POST
    : strstr(src, "PUT")  == src ? PUT
    : strstr(src, "DELETE")  == src ? DELETE
    : strstr(src, "OPTIONS")  == src ? OPTIONS
    : strstr(src, "HEAD")  == src ? HEAD
    : -1;
}

struct lal_request *
lal_create_request(char *src)
{
    char *ptr;
    struct lal_request *request = malloc(sizeof(struct lal_request));

    request->method = lal_method_from_string(src);

    /* skip over the method */
    while (*src++ != ' ')
        ;

    /* delineate the path */
    ptr = src;
    while (*ptr != ' ')
        ptr++;
    *ptr++ = '\0';

    request->path = malloc((strlen(src) + 1) * sizeof(char));
    strcpy(request->path, src);

    src = ptr;

    /* fast forward to the next line */
    while (*src++ != '\n')
        ;

    lal_set_entries(request, src);

    return request;
}

void
lal_destroy_request (struct lal_request *request)
{
    struct lal_entry *prev, *entry = request->entries;

    while (entry) {
        free(entry->val);
        free(entry->key);
        prev = entry;
        entry = entry->next;
        free(prev);
    }

    free(request->path);
    free(request);
}
