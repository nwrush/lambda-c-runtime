#ifndef LAMBDA_RUNTIME_H
#define LAMBDA_RUNTIME_H

#include <stddef.h>

typedef void (*lambda_error_callback_f)(const char*, const char*, const char*);

typedef struct {
    char* body;
    size_t size;
    const char* awsRequestId;
    const char* runtimeDeadline;
    const char* functionArn;
    const char* runtimeTraceId;
    const char* runtimeClientContext;
    const char* runtimeCognitoIdentity;
    lambda_error_callback_f errorHandler;
} LambdaContext;

typedef struct {
    char* body;
    size_t size;
} LambdaHandlerResponse;

typedef LambdaHandlerResponse (*lambda_handler_f)(LambdaContext*);

#endif
