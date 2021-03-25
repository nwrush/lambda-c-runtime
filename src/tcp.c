#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>

#include "tcp.h"


int connect_addr(const char* hostname, int port) {
  
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
	return -1;
    }

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (1 != inet_pton(AF_INET, hostname, &(addr.sin_addr))) {
	return -1;
    }

    if (-1 == connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
	return -1;
    }

    return fd;
}

ssize_t send_msg(int socket, const void* buf, size_t len) {
    return send(socket, buf, len, 0);
}

ssize_t recv_msg(int socket, void* buffer, ssize_t size) {
    ssize_t recvCount = recv(socket, buffer, size, 0);
    if (recvCount == 0) {
	puts("Connection closed by remote");
	abort();
    }
}
