#ifndef MALLOC_WRAPPERS_H
#define MALLOC_WRAPPERS_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void* malloc_with_check(size_t size);
void free_with_check(void *mem);
void* realloc_with_check(void *ptr, size_t size);
size_t get_alloc_count();
size_t get_allocation_size(const void *mem);
size_t get_alloc_bytes();
void set_max_alloc_bytes(size_t max_bytes);
size_t get_max_alloc_bytes();

#define pb_realloc(ptr,size) realloc_with_check(ptr,size)
#define pb_free(ptr) free_with_check(ptr)

struct pb_allocator_s;
extern struct pb_allocator_s* malloc_wrappers_allocator;

#ifdef __cplusplus
}
#endif

#endif
