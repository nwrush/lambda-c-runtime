#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "tcp.h"

#define HANDLER_ENV_NAME "_HANDLER"
#define TASK_ROOT_ENV_NAME "LAMBDA_TASK_ROOT"
#define RUNTIME_API_ENV_NAME "AWS_LAMBDA_RUNTIME_API"
#define REQUEST_ID_HEADER "Lambda-Runtime-Aws-Request-Id"

#define INIT_ERROR_PATH "/2018-06-01/runtime/init/error"
#define NEXT_INVOKE_PATH "/2018-06-01/runtime/invocation/next"
#define SUCCESS_RESPONSE_PATH_FORMAT "/2018-06-01/runtime/invocation/%s/response"
#define SUCCESS_RESPONSE_PATH_SIZE 41
#define ERROR_RESPONSE_PATH_FORMAT "/2018-06-01/runtime/invocation/%s/error"
#define ERROR_RESPONSE_PATH_SIZE 38


void testFunction(char* buffer, size_t size) {
    puts("hello");
    puts(buffer);
}

int sendFunctionSuccess(int socket, const char* requestId, char* buffer, size_t bufferSize, HttpHeader hostHeader) {
    HttpRequest* request = create_request(POST);
    add_header(request, hostHeader);

    size_t respPathLength = SUCCESS_RESPONSE_PATH_SIZE + strlen(requestId);
    char respPath[respPathLength+1];
    snprintf(respPath, respPathLength+1, SUCCESS_RESPONSE_PATH_FORMAT, requestId);
    
    request->path = respPath;
    request->body = buffer;
    request->bodySize = bufferSize;

    HttpResponse* response = send_request(socket, request);

    free_request(request);
    // free(buffer);
    free_response(response);
}

int invokeFunction(int socket, HttpHeader hostHeader) {
    HttpRequest* request = create_request(GET);
    add_header(request, hostHeader);

    request->path = NEXT_INVOKE_PATH;
    
    HttpResponse* response = send_request(socket, request);
    
    free_request(request);
    if (response->responseCode != 200) {
    	puts("Received non 200 response from next");
    	printf("%d\n", response->responseCode);
    	return -1;
    }
    HttpHeader* requestIdHeader = http_find_header(response, REQUEST_ID_HEADER);
    if (requestIdHeader == NULL) {
	puts(REQUEST_ID_HEADER" header not present");
	abort();
    }
    
    testFunction(response->body, response->bodySize);
    sendFunctionSuccess(socket, requestIdHeader->value, "banana", 6, hostHeader);

    free_response(response);
    return 0;
}

int main(int argc, char** argv) {
    char* apiEndpoint = getenv(RUNTIME_API_ENV_NAME);

    size_t len = strlen(apiEndpoint);
    char endpointCopy[len+1];
    strncpy(endpointCopy, apiEndpoint, len);
    endpointCopy[len] = '\0';

    char* hostname = strtok(endpointCopy, ":");
    int port = atoi(strtok(NULL, ":"));

    int tcp_socket = connect_addr(hostname, port);

    if (tcp_socket == -1) {
	puts("Failed to create tcp connection. Exiting");
	printf("Error message: %d - %s\n", errno, strerror(errno));
	return 1;
    }

    HttpHeader hostHeader;
    hostHeader.key = "Host";
    hostHeader.value = hostname;

    invokeFunction(tcp_socket, hostHeader);
}
