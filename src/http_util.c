#include <string.h>

#include "http.h"

HttpHeader* find_header(HttpResponse* response, const char* key) {
    for (size_t i=0; i<response->numHeaders; ++i) {
	if (!strcmp(key, response->headers[i].key)) {
	    return response->headers + i;
	}
    }
    return NULL;
}
