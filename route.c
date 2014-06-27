#include "route.h"

static struct lal_route *routes;

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
           strstr(buf, "\r\n\r\n") == NULL &&
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
    int nentries = 0;

    entry = malloc(sizeof(struct lal_entry));
    request->entries = entry;

    line = strtok(src, "\r\n");
    for (;;) {
        if (strstr(line, ":")) {
            nentries++;

            ptr = line;
            while (*ptr != ':' && *ptr != '\0') {
                ptr++;
            }
            *ptr++ = '\0';

            entry->key = malloc((strlen(line) + 1) * sizeof(char));
            strcpy(entry->key, line);

            while (*ptr == ' ')
                ptr++; /* skip the intervening spaces */
            entry->val = malloc((strlen(ptr) + 1) * sizeof(char));
            strcpy(entry->val, ptr);

            if ((line = strtok(NULL, "\r\n")))
                entry = entry->next = malloc(sizeof(struct lal_entry));
            else {
                entry->next = NULL;
                break;
            }
        }
    };
    request->nentries = nentries;
}

struct lal_request *
lal_create_request(char *src)
{
    char *ptr;
    struct lal_request *request = malloc(sizeof(struct lal_request));

    request->method = strstr(src, "GET") == src ? GET
    : strstr(src, "POST") == src ? POST
    : -1;

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

void
lal_route_request (int sock, int hitcount)
{
    char *header;
    struct lal_request *request;
    struct lal_route *route;

    syslog(LOG_INFO, "%i request%s routed\n",
           hitcount, hitcount < 2 ? "" : "s");

    header = lal_read_header(sock);
    request = lal_create_request(header);

    if (!~request->method)
        syslog(LOG_ERR, "route_request failed: method %s not implemented\n",
               strtok(header, " "));

    free(header);

    route = lal_get_route(request);

    if (route)
        route->handler(request, sock);

    lal_destroy_request(request);
}

void
lal_register_route (enum lal_http_method method, char *path,
               int(*handler)(struct lal_request *, int))
{
    struct lal_route *route;

    if (routes == NULL)
        routes = route = malloc(sizeof(struct lal_route));
    else {
        route = routes;
        while (route->next)
            route = route->next;

        route->next = malloc(sizeof(struct lal_route));
        route = route->next;
    }

    route->path = malloc((strlen(path) + 1) * sizeof(char));
    strcpy(route->path, path);

    route->method = method;
    route->handler = handler;
    route->next = NULL;

    syslog(LOG_INFO, "route created: %s, %i, %p\n",
           route->path, route->method, (void *)route->handler);

}

struct lal_route *
lal_get_route (struct lal_request *request)
{
    struct lal_route *route = routes;
    char *route_pos, *request_pos;

    while (route) {
        route_pos = route->path;
        request_pos = request->path;
compare:
        while (*route_pos == *request_pos) {
            if (*request_pos == '\0') {
                if (route->method == request->method)
                    return route;
                else
                    break;
            }
            else {
                route_pos++;
                request_pos++;
            }
        }

        if (*request_pos == '?')
            return route;

        if (*route_pos == ':') {
            int contains_param = 0;
            while (*route_pos != '/' && *route_pos != '\0')
                route_pos++;
            while (*request_pos != '/' && *request_pos != '\0') {
                contains_param = 1;
                request_pos++;
            }
            if (contains_param && *route_pos == *request_pos)
                goto compare;
        }
        route = route->next;
    }
    return NULL;
}

void
lal_destroy_routes ()
{
    struct lal_route *prev;
    int i = 0;
    while (routes) {
        free(routes->path);
        prev = routes;
        routes = routes->next;
        free(prev);
        i++;
    }
    syslog(LOG_INFO, "destroyed %i routes\n", i);
}
