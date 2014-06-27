lal
===

Web framework in C



```C
int
authenticated(struct lal_request *request, int sock)
{
    char *response;
    char *token = lal_get_header_val(request, "Authorization");
    
    if (strstr(token, "Token abc123") == token)
        response = "HTTP/1.1 200 OK";
    else
        response = "HTTP/1.1 403 Forbidden";
    
    send(sock, response, (strlen(response) + 1), 0);
    
    return 0;
}


int
main(int argc, char **argv)
{
    db_setup();

    lal_register_route(GET, "/authenticated", authenticated);
    lal_register_route(GET, "/token", get_token);
    lal_register_route(PUT, "/user", create_user);
    lal_register_route(DELETE, "/user", delete_user);

    lal_serve_forever(
        argc > 1 ? argv[1] : "localhost",
        argc > 2 ? argv[2] : "8080",
        (
            argc < 4 ? 0
            : strcmp(argv[3], "true") ? 0
            : 1
        )
    );

    return 0;
}
```
