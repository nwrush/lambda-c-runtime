#include <stdio.h>

#include "cJSON.h"
#include "http.h"
#include "tcp.h"

int main(int argc, char** argv) {
    int tcp_socket = connect_addr("127.0.0.1");

    HttpHeader hostHeader;
    hostHeader.key = "Host";
    hostHeader.value = "127.0.0.1";

    HttpRequest* request = create_request(GET);
    
    request->path = "/runtime/invocation/next";
    add_header(request, hostHeader);

    HttpResponse* response = send_request(tcp_socket, request);

    printf("%d\n", response->responseCode);
    printf("Found %d http headers\n", response->numHeaders);
    for (size_t i = 0; i < response->numHeaders; ++i) {
	printf("%s: %s\n", response->headers[i].key, response->headers[i].value);
    }

    printf("Data: %s\n", response->body);

    free_request(request);
    free_response(response);
}
