#ifndef TCP_H
#define TCP_H

#include <inttypes.h>
#include <sys/types.h>

typedef struct {
    int socket;
    char* hostname;
    uint16_t port;
} tcp_conn;

tcp_conn* connect_addr(const char*, int);

ssize_t send_msg(tcp_conn*, const void*, size_t);

ssize_t recv_msg(tcp_conn*, void*, ssize_t);

void tcp_free_conn(tcp_conn*);

#endif
