/* cs194-24 Lab 1 */

#ifndef HTTP_H
#define HTTP_H

#include "palloc.h"

/* Linked list of headers */
struct http_header
{
    const char* header;
    struct http_header* next;
};

/* Allows HTTP sessions to be transported over the HTTP protocol */
struct http_session
{
    const char * (*gets)(struct http_session *);

    ssize_t (*puts)(struct http_session *, const char *);

    ssize_t (*write)(struct http_session *, const char *, size_t);

    /* Stores a resizeable, circular buffer */
    char *buf;
    size_t buf_size, buf_used;

    int fd; //network fd
    int disk_fd; //disk fd
    int done_processing; //0 is haven't read, 1 if we did
    int done_reading;
    struct http_header* headers;

    char* response;
};

/* A server that listens for HTTP connections on a given port. */
struct http_server
{
    struct http_session * (*wait_for_client)(struct http_server *);

    int fd;
};

/* Creates a new HTTP server listening on the given port. */
struct http_server *http_server_new(palloc_env env, short port);
int listen_on_port(short port);

#endif
