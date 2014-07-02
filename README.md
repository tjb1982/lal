lal
===

Web framework in C

```bash
$ make
$ make install
```


```C
#include <lal/route.h>
#include <lal/network.h>
...
#define CRLF "\r\n"

int
say_something(struct lal_request *request, int sock)
{

    char *msg[21];
    sscanf(request->path, "/hello/%20[^/]", msg);

    struct lal_response *resp = lal_create_response("200 OK");

    char content_length[3];
    sprintf(content_length, "%i", strlen(msg));

    lal_append_to_entries(resp->entries, "Content-Length", content_length);
    lal_append_to_entries(resp->entries, "Content-Type", "text/plain; charset=utf-8");
    lal_append_to_body(resp->body, msg);

    char *response = lal_serialize_response(resp);
    send(sock, response, strlen(response), 0);

    free(response);
    lal_destroy_response(resp);

    return 0;
}

int
main(int argc, char **argv)
{
    lal_register_route(GET, "/hello/:message", say_something);

    lal_serve_forever(
        lal_route_request,
        argc > 1 ? argv[1] : "127.0.0.1",
        argc > 2 ? argv[2] : "8080",
        ( /// daemonize?
            argc < 4 ? 0
            : strcmp(argv[3], "true") ? 0
            : 1
        ),
        ( /// multithread?
            argc < 5 ? 0
            : strcmp(argv[4], "true") ? 0
            : 1
        )
    );
    lal_destroy_routes();

    return 0;
}
```

```bash
my_app [hostname] [port] [daemonize]
$ my_app localhost 9000 true
```
