#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "http.h"
#include "string_utils.h"
#include "tcp.h"

#define HTTP_VERSION "HTTP/1.1"
#define INITIAL_HEADER_SIZE 10
#define HTTP_CONTENT_LENGTH "Content-Length"

#define BUFFER_RESIZE_FACTOR 2
#define NEWLINE '\n'

// The max size of a lambda payload is 6 MB, so we'll receive 6MB + 1KB for the entire message
#define INIT_HTTP_RECV_SIZE 6 * 1000 * 1000 + 1000

void add_header_resp(HttpResponse*, HttpHeader);
HttpResponse* create_http_response();

const char* convert_method_to_string(HttpMethod header) {
    switch (header) {
    case GET:
	return "GET";
    case POST:
	return "POST";
    default:
	abort();
    }
}

size_t write_line_to_request(char** buffer, size_t* buffer_size, size_t offset, char* str) {
    size_t length = strlen(str);
    if (offset + length + 1>= *buffer_size) {
	*buffer = (char*)realloc((void*)*buffer, *buffer_size * BUFFER_RESIZE_FACTOR);
	*buffer_size = *buffer_size * BUFFER_RESIZE_FACTOR;
	if (*buffer == NULL) {
	    abort();
	}
    }

    strncat(*buffer, str, length);
    strncpy(*buffer + offset, str, length);

    return offset + length;
}

size_t write_header_to_request(char** buffer, size_t* buffer_size, size_t offset, HttpHeader header) {
    size_t keySize = strlen(header.key);
    size_t valueSize = strlen(header.value);

    char* line = malloc(keySize + valueSize + 5);
    snprintf(line, keySize + valueSize + 5, "%s: %s\r\n", header.key, header.value);

    size_t newOffset = write_line_to_request(buffer, buffer_size, offset, line);
    free(line);

    return newOffset;
}

void parse_http_response_first_line(HttpResponse* response, const char* buffer, size_t lineLen) {
    char* lineBuffer = (char*)malloc(lineLen+1);
    strncpy(lineBuffer, buffer, lineLen);
    lineBuffer[lineLen+1] = '\0';

    char* token = strtok(lineBuffer, " ");
    if (token == NULL) {
	abort();
    }

    if (strncmp(HTTP_VERSION, token, strlen(HTTP_VERSION)) != 0) {
	abort();
    }

    token = strtok(NULL, " ");
    response->responseCode = atoi(token);

    free(lineBuffer);
}

size_t parse_http_response_headers(HttpResponse* response, const char* buffer, size_t bufferLen) {
    size_t pos = 0;

    size_t tokenLen = 0;
    bool emptyLine = true;
    bool hasSplit = false;
    char* marker = buffer;
    char* key = '\0';

    while (pos < bufferLen && buffer[pos] != '\0') {
	if (buffer[pos] == '\r') {
	    if (buffer[pos+1] == '\n') {
		if (emptyLine) {
		    return pos+4;
		}
		char* value = malloc(tokenLen + 1);
		strncpy(value, marker, tokenLen);
		value[tokenLen] = '\0';

		HttpHeader header;
		header.key = trim(key);
		header.value = trim(value);
		add_header_resp(response, header);

		tokenLen = 0;
		emptyLine = true;
		hasSplit = false;
		marker = buffer + pos + 2;
		pos += 2;
	    } else {
		pos++;
	    }
	    continue;
	}
	if (buffer[pos] == ':' && !hasSplit) {
	    emptyLine = false;
	    hasSplit = true;
	    
	    key = malloc(tokenLen + 1);
	    strncpy(key, marker, tokenLen);
	    key[tokenLen] = '\0';
	    marker = buffer+pos+1;
	    tokenLen = 0;
	    pos++;
	    continue;	    
	}
	tokenLen++;
	if (emptyLine && !isspace(buffer[pos])) {
	    emptyLine = false;
	}
	pos++;
    }
    return pos;
}

HttpResponse* read_http_response(tcp_conn* connection) {
    HttpResponse* response = create_http_response();

    char* buffer = (char*)calloc(INIT_HTTP_RECV_SIZE+1, sizeof(char));
    ssize_t recvSize = recv_msg(connection, buffer, INIT_HTTP_RECV_SIZE);

    printf("Received message of size %zd\n", recvSize);
    printf("Response content: %s\n", buffer);

    char* lineEnding = strstr(buffer, "\r\n");
    if (lineEnding == NULL) {
	abort();
    }

    size_t firstLineLen = lineEnding - buffer;
    parse_http_response_first_line(response, buffer, lineEnding - buffer);
    size_t bytesRead = parse_http_response_headers(response, lineEnding + 2, strlen(lineEnding));

    HttpHeader* header = http_find_header(response, "Content-Length");
    if (header == NULL) {
	// Assume the content length is 0
	return response;
    }

    int contentLength = atoi(header->value);
    if (contentLength == 0) {
	response->body = '\0';
	response->bodySize = 0;
    } else if (recvSize - (bytesRead + firstLineLen) >= contentLength) {
	response->body = malloc(contentLength+1);
	memcpy(response->body, buffer+bytesRead + firstLineLen, contentLength);
	response->body[contentLength] = '\0';
	response->bodySize = contentLength;
    } else {
	puts("Buffer missing enough data to fit content length");
	printf("RecvSize: %zd\nBytesRead: %zd\nFirstLineLen: %zd\nContentLength: %d\n", recvSize, bytesRead, firstLineLen, contentLength);
	if (recvSize + contentLength > INIT_HTTP_RECV_SIZE) {
	    puts("Request body too large to fit in data buffer");
	    abort();
	}

	recv_msg(connection, buffer+recvSize, contentLength);
	response->body = malloc(contentLength+1);
	memcpy(response->body, buffer+bytesRead + firstLineLen, contentLength);
	response->body[contentLength] = '\0';
	response->bodySize = contentLength;
    }

    free(buffer);
    
    return response;
}

HttpResponse* send_request(tcp_conn* connection, HttpRequest* request) {
    char* payload = (char*)calloc(1024, 1);

    size_t buffer_size = 1024;
    size_t buffer_pos = 0;
    int written = snprintf(payload, 1024, "%s %s "HTTP_VERSION"\r\n", convert_method_to_string(request->method), request->path);

    if (written >= buffer_size) {
	// Output was truncated
	// TODO: This should retry the write?
	abort();
    }

    buffer_pos = written;

    if (request->method == POST) {
	HttpHeader contentLengthHeader;
	contentLengthHeader.key = HTTP_CONTENT_LENGTH;

	int numDigits = (int) ceil(log10((double)request->bodySize));
	char* bodySizeStr = (char*)calloc(numDigits+1, sizeof(char));
	snprintf(bodySizeStr, numDigits+1, "%zu", request->bodySize);
	bodySizeStr[numDigits] = '\0';
	contentLengthHeader.value = bodySizeStr;
    
	add_header(request, contentLengthHeader);

	HttpHeader contentTypeHeader;
	contentTypeHeader.key = "Content-Type";
	contentTypeHeader.value = "application/json";
	add_header(request, contentTypeHeader);
    }

    for (unsigned int i = 0; i < request->numHeaders; ++i) {
	buffer_pos = write_header_to_request(&payload, &buffer_size, buffer_pos, request->headers[i]);
    }
    payload[buffer_pos] = '\r';
    payload[buffer_pos+1] = '\n';
    buffer_pos += 2;

    if (request->method == POST) {
	memcpy(payload+buffer_pos, request->body, request->bodySize);
	buffer_pos += request->bodySize;
    }
	    
    puts(payload);
    send_msg(connection, payload, buffer_pos);

    free(payload);

    return read_http_response(connection);
}

HttpRequest* create_request(HttpMethod method) {
    HttpRequest* request = (HttpRequest*)calloc(1, sizeof(HttpRequest));
    request->method = method;
    request->headers = (HttpHeader*)calloc(INITIAL_HEADER_SIZE, sizeof(HttpHeader));
    request->headerSpace = INITIAL_HEADER_SIZE;
    return request;
}

HttpResponse* create_http_response() {
    HttpResponse* response = (HttpResponse*)calloc(1, sizeof(HttpResponse));
    response->headers = (HttpHeader*)calloc(INITIAL_HEADER_SIZE, sizeof(HttpHeader));
    response->headerSpace = INITIAL_HEADER_SIZE;
    return response;
}

void add_header(HttpRequest* request, HttpHeader header) {
    if (request->numHeaders == request->headerSpace) {
	unsigned int newHeaderCount = request->headerSpace * 2;
	size_t newSize = sizeof(HttpHeader) * newHeaderCount;

	request->headers = realloc(request->headers, newSize);
	request->headerSpace = newHeaderCount;
    }

    request->headers[request->numHeaders++] = header;
}

void add_header_resp(HttpResponse* response, HttpHeader header) {
    if (response->numHeaders == response->headerSpace) {
	unsigned int newHeaderCount = response->headerSpace * 2;
	size_t newSize = sizeof(HttpHeader) * newHeaderCount;

	response->headers = realloc(response->headers, newSize);
	response->headerSpace = newHeaderCount;
    }

    response->headers[response->numHeaders++] = header;
}

void free_request(HttpRequest* request) {
    free(request->headers);
    request->headers = NULL;
    free(request);
}

void free_response(HttpResponse* response) {
    for (size_t i=0; i<response->numHeaders; ++i) {
	HttpHeader header = response->headers[i];
	free((char*)header.key);
	free((char*)header.value);
    }
    free(response->headers);
    free(response->body);
    response->headers = NULL;
    response->body = NULL;
    free(response);
}
