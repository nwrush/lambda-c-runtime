#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lambda_runtime.h"
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


static tcp_conn* connection;
static HttpHeader hostHeader;


LambdaHandlerResponse testFunction(LambdaContext* context) {
    puts(context->body);

    LambdaHandlerResponse response;
    response.body = "apple";
    response.size = 5;
    return response;
}

int sendFunctionError(const char* message, const char* type) {
    HttpRequest* request = create_request(POST);
    return 0;
}

HttpResponse* sendFunctionSuccess(const char* requestId, LambdaHandlerResponse resp) {
    HttpRequest* request = create_request(POST);
    add_header(request, hostHeader);

    size_t respPathLength = SUCCESS_RESPONSE_PATH_SIZE + strlen(requestId);
    char respPath[respPathLength+1];
    snprintf(respPath, respPathLength+1, SUCCESS_RESPONSE_PATH_FORMAT, requestId);
    
    request->path = respPath;
    request->body = resp.body;
    request->bodySize = resp.size;

    HttpResponse* response = send_request(connection, request);

    free_request(request);
    return response;
}

HttpResponse* getNextInvocation() {
    HttpRequest* request = create_request(GET);
    add_header(request, hostHeader);

    request->path = NEXT_INVOKE_PATH;
    
    HttpResponse* response = send_request(connection, request);
    
    free_request(request);
    request = NULL;

    if (response->responseCode != 200) {
    	puts("Received non 200 response from next");
    	printf("%d\n", response->responseCode);
    	return NULL;
    }
    HttpHeader* requestIdHeader = http_find_header(response, REQUEST_ID_HEADER);
    if (requestIdHeader == NULL) {
	puts(REQUEST_ID_HEADER" header not present");
	abort();
    }

    return response;
}

LambdaContext createContextFromResponse(HttpResponse* response) {
    LambdaContext context;
    context.body = response->body;
    context.size = response->bodySize;

    return context;
}

void handleRequests(lambda_handler_f handler) {
    int shouldLoop = 1;
    while (shouldLoop) {
	HttpResponse* nextResponse = getNextInvocation();
	HttpHeader* requestIdHeader = http_find_header(nextResponse, REQUEST_ID_HEADER);

	LambdaContext context = createContextFromResponse(nextResponse);

	LambdaHandlerResponse handlerResp = handler(&context);
	
	HttpResponse* response = sendFunctionSuccess(requestIdHeader->value, handlerResp);

	if (response->responseCode == 500) {
	    puts("Error posting invocation response");
	    shouldLoop = 0;
	}
	
	free_response(nextResponse);
	free_response(response);
	// free(handlerResp.body);
	handlerResp.body = NULL;
    }
} 

int main(int argc, char** argv) {
    char* apiEndpoint = getenv(RUNTIME_API_ENV_NAME);

    size_t len = strlen(apiEndpoint);
    char endpointCopy[len+1];
    strncpy(endpointCopy, apiEndpoint, len);
    endpointCopy[len] = '\0';

    char* hostname = strtok(endpointCopy, ":");
    int port = atoi(strtok(NULL, ":"));

    connection = connect_addr(hostname, port);

    if (connection == NULL) {
	puts("Failed to create tcp connection. Exiting");
	printf("Error message: %d - %s\n", errno, strerror(errno));
	return 1;
    }

    hostHeader.key = "Host";
    hostHeader.value = hostname;

    handleRequests(&testFunction);

    tcp_free_conn(connection);
    connection = NULL;
}
