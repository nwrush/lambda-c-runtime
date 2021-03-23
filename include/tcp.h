#ifndef TCP_H
#define TCP_H

#include <sys/types.h>

int connect_addr(const char*);

ssize_t send_msg(int, const void*, size_t);

ssize_t recv_msg(int, void*, ssize_t);
ssize_t recv_all(int, void*, ssize_t*);

#endif
