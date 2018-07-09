// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef CRY_DLMALLOC_H
#define CRY_DLMALLOC_H
#pragma once

typedef void* dlmspace;

#ifdef __cplusplus
extern "C"
{
#endif

typedef void* (* dlmmap_handler)(void* user, size_t sz);
typedef int (*   dlmunmap_handler)(void* user, void* mem, size_t sz);
static void* const dlmmap_error = (void*)(INT_PTR)-1;

int      dlmspace_create_overhead(void);
dlmspace dlcreate_mspace(size_t capacity, int locked, void* user = NULL, dlmmap_handler mmap = NULL, dlmunmap_handler munmap = NULL);
size_t   dldestroy_mspace(dlmspace msp);
dlmspace dlcreate_mspace_with_base(void* base, size_t capacity, int locked);
int      dlmspace_track_large_chunks(dlmspace msp, int enable);
void*    dlmspace_malloc(dlmspace msp, size_t bytes);
void     dlmspace_free(dlmspace msp, void* mem);
void*    dlmspace_realloc(dlmspace msp, void* mem, size_t newsize);
void*    dlmspace_calloc(dlmspace msp, size_t n_elements, size_t elem_size);
void*    dlmspace_memalign(dlmspace msp, size_t alignment, size_t bytes);
void**   dlmspace_independent_calloc(dlmspace msp, size_t n_elements, size_t elem_size, void* chunks[]);
void**   dlmspace_independent_comalloc(dlmspace msp, size_t n_elements, size_t sizes[], void* chunks[]);
size_t   dlmspace_footprint(dlmspace msp);
size_t   dlmspace_max_footprint(dlmspace msp);
size_t   dlmspace_usable_size(void* mem);
void     dlmspace_malloc_stats(dlmspace msp);
size_t   dlmspace_get_used_space(dlmspace msp);
int      dlmspace_trim(dlmspace msp, size_t pad);
int      dlmspace_mallopt(int, int);

#ifdef __cplusplus
}
#endif

#endif
