#ifndef LAMBDA_RUNTIME_H
#define LAMBDA_RUNTIME_H

#include <stddef.h>

typedef struct {
    char* body;
    size_t size;
} LambdaContext;

typedef struct {
    char* body;
    size_t size;
} LambdaHandlerResponse;

typedef void (*lambda_error_callback_f)(char*, char*);
typedef LambdaHandlerResponse (*lambda_handler_f)(LambdaContext*);

#endif
