/* Example of a custom allocator implementation for nanopb */

#ifndef ARENA_ALLOCATOR_H_INCLUDED
#define ARENA_ALLOCATOR_H_INCLUDED

#include <pb.h>

typedef struct {
    pb_allocator_t allocator;

    // Arena from which memory will be allocated
    void *arena;
    size_t arena_size;

    // Total number of bytes allocated so far
    size_t total_alloc_size;

    // Total number of active allocations (for testing purposes)
    // Incremented on alloc, decremented on free.
    size_t total_alloc_count;

    // Pointer returned for previous allocation (used for realloc)
    void *prev_alloc_ptr;
} arena_allocator_t;

// Initialize an arena allocator that will reserve memory from buffer at "arena".
void arena_allocator_init(arena_allocator_t *allocator, void *arena, size_t arena_size);

#endif
