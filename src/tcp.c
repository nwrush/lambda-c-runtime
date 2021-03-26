#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

#include "tcp.h"

tcp_conn* connect_addr(const char* hostname, int port) {
  
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
	return NULL;
    }

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (1 != inet_pton(AF_INET, hostname, &(addr.sin_addr))) {
	return NULL;
    }

    if (-1 == connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
	return NULL;
    }

    tcp_conn* connection = (tcp_conn*)malloc(sizeof(tcp_conn));
    connection->socket = fd;
    connection->port = addr.sin_port;
    connection->hostname = malloc(strlen(hostname)+1);
    strncpy(connection->hostname, hostname, strlen(hostname)+1);

    return connection;
}

tcp_conn* reconnect(tcp_conn* connection) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = connection->port;
    
    if (1 != inet_pton(AF_INET, connection->hostname, &(addr.sin_addr))) {
	puts("Error converting hostname");
	return NULL;
    }

    if (-1 == connect(connection->socket, (struct sockaddr *)&addr, sizeof(addr))) {
	return NULL;
    }
    return connection;
} 

ssize_t send_msg(tcp_conn* connection, const void* buf, size_t len) {
    ssize_t sentCount = send(connection->socket, buf, len, 0);
    if (sentCount == -1 && errno == ECONNRESET) {
	connection = reconnect(connection);
	if (connection == NULL) {
	    puts("Failed to re-establish connection");
	    return -1;
	}
	return send(connection->socket, buf, len, 0);
    }
    return sentCount;
}

ssize_t recv_msg(tcp_conn* connection, void* buffer, ssize_t size) {
    ssize_t recvCount = recv(connection->socket, buffer, size, 0);
    if (recvCount == 0) {
	connection = reconnect(connection);
	if (connection == NULL) {
	    puts("Failed to re-establish connection");
	    return -1;
	}
	return recv(connection->socket, buffer, size, 0);
    }
    return recvCount;
}

void tcp_free_conn(tcp_conn* connection) {
    close(connection->socket);
    free(connection->hostname);
    connection->hostname = NULL;
    free(connection);
}
