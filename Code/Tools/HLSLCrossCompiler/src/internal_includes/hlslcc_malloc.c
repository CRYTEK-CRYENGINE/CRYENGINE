#include <stdlib.h>

#ifdef __APPLE_CC__
	#include <malloc/malloc.h>
#else
	#include <malloc.h>
#endif

void* (*hlslcc_malloc)(size_t size) = malloc;
void* (*hlslcc_calloc)(size_t num,size_t size) = calloc;
void  (*hlslcc_free)(void *p) = free;
void* (*hlslcc_realloc)(void *p,size_t size) = realloc;