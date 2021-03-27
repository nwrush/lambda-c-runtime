#define _POSIX_C_SOURCE 200112L

#include <dlfcn.h>
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
#define ERROR_MSG_FMT "{\"errorMessage\":\"%s\",\"errorType\":\"%s\"}"


static tcp_conn* connection;
static HttpHeader hostHeader;


char* formatErrorMessage(const char* message, const char* type) {
    size_t length = strlen(message) + strlen(type) + strlen(ERROR_MSG_FMT)+1;
    char* formattedMessage = (char*)malloc(length);
    snprintf(formattedMessage, length, ERROR_MSG_FMT, message, type);
    return formattedMessage;
}

void sendFunctionInitError(const char* message) {
    char* errorMsg = formatErrorMessage(message, "RuntimeInitError");
    
    HttpRequest* request = create_request(POST);
    add_header(request, hostHeader);
    
    request->path = INIT_ERROR_PATH;
    request->body = errorMsg;
    request->bodySize = strlen(errorMsg);
    
    HttpResponse* response = send_request(connection, request);

    free_response(response);

    free_request(request);
    free(errorMsg);
}

int sendFunctionError(const char* message, const char* type, const char* requestId) {
    HttpRequest* request = create_request(POST);
    add_header(request, hostHeader);

    size_t respPathLength = SUCCESS_RESPONSE_PATH_SIZE + strlen(requestId);
    char respPath[respPathLength+1];
    snprintf(respPath, respPathLength+1, SUCCESS_RESPONSE_PATH_FORMAT, requestId);

    char* errorMsg = formatErrorMessage(message, type);
    
    request->path = respPath;
    request->body = errorMsg;
    request->bodySize = strlen(errorMsg);

    HttpResponse* response = send_request(connection, request);
    
    free_request(request);
    free(errorMsg);

    if (response->responseCode == 500) {
	return 1;
    }

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

    HttpHeader* requestIdHeader = http_find_header(response, "Lambda-Runtime-Aws-Request-Id");
    context.awsRequestId = requestIdHeader->value;

    HttpHeader* deadlineHeader = http_find_header(response, "Lambda-Runtime-Deadline-Ms");
    context.runtimeDeadline = deadlineHeader->value;
    
    HttpHeader* functionArnHeader = http_find_header(response, "Lambda-Runtime-Invoked-Function-Arn");
    context.functionArn = functionArnHeader->value;

    HttpHeader* traceIdHeader = http_find_header(response, "Lambda-Runtime-Trace-Id");
    context.runtimeTraceId = traceIdHeader->value;

    setenv("_X_AMZN_TRACE_ID", traceIdHeader->value, 1);

    HttpHeader* clientContextHeader = http_find_header(response, "Lambda-Runtime-Client-Context");
    if (clientContextHeader != NULL) {
	context.runtimeClientContext = clientContextHeader->value;
    } else {
	context.runtimeClientContext = NULL;
    }

    HttpHeader* cognitoIdentityHeader = http_find_header(response, "Lambda-Runtime-Cognito-Context");
    if (cognitoIdentityHeader != NULL) {
	context.runtimeCognitoIdentity = cognitoIdentityHeader->value;
    } else {
	context.runtimeCognitoIdentity = NULL;
    }

    context.errorHandler = (lambda_error_callback_f)sendFunctionError;
    
    return context;
}

void handleRequests(lambda_handler_f handler) {
    int shouldLoop = 1;
    while (shouldLoop) {
	HttpResponse* nextResponse = getNextInvocation();
	HttpHeader* requestIdHeader = http_find_header(nextResponse, REQUEST_ID_HEADER);

	LambdaContext context = createContextFromResponse(nextResponse);

	LambdaHandlerResponse handlerResp = handler(&context);

	unsetenv("_X_AMZN_TRACE_ID");
	
	HttpResponse* response = sendFunctionSuccess(requestIdHeader->value, handlerResp);

	if (response->responseCode == 500) {
	    puts("Error posting invocation response");
	    shouldLoop = 0;
	}
	
	free_response(nextResponse);
	free_response(response);
	handlerResp.body = NULL;
    }
}

lambda_handler_f loadHandler(const char* handlerName) {
    size_t len = strlen(handlerName);
    char handlerNameCopy[len+1];
    strncpy(handlerNameCopy, handlerName, len);
    handlerNameCopy[len] = '\0';
    
    char* libName = strtok(handlerNameCopy, ":");
    char* handlerSymbol = strtok(NULL, ":");

    void* handle = dlopen(libName, RTLD_LAZY);
    if (handle == NULL) {
	printf("Could not load provided library: %s\n", libName);
	
	return NULL;
    }

    dlerror();
    lambda_handler_f handler = dlsym(handle, handlerSymbol);

    if (handler == NULL) {
	printf("Could not load provided handler: %s\n", handlerSymbol);
	return NULL;
    }

    char* error = dlerror();
    if (error != NULL) {
	sendFunctionInitError(error);
	abort();	
    }
    
    return handler;
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

    char* taskRoot = getenv("LAMBDA_TASK_ROOT");

    char* handlerName = getenv(HANDLER_ENV_NAME);

    if (handlerName == NULL) {
	sendFunctionInitError("_HANDLER env-var was not set");
	return 2;
    }
    
    lambda_handler_f handler = loadHandler(handlerName);
    if (handler == NULL) {
	char* errorMsg = "Could not load handler: ";
	
	char error[strlen(handlerName) + strlen(errorMsg) + 1];
	strncpy(error, errorMsg, strlen(errorMsg));
	strncat(error, handlerName, strlen(errorMsg)+1);
	
	sendFunctionInitError(error);
	puts(error);
	return 3;
    }
    

    handleRequests(handler);

    tcp_free_conn(connection);
    connection = NULL;
}
