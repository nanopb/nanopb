#include "arena_allocator.h"
#include <string.h>

#define ALLOC_ALIGNMENT sizeof(double)

void* arena_realloc(pb_allocator_t *actx, void *ptr, size_t size)
{
    arena_allocator_t *arena = (arena_allocator_t*)actx;

    // Align to void* size
    if (size % ALLOC_ALIGNMENT != 0)
    {
        size += ALLOC_ALIGNMENT - (size % ALLOC_ALIGNMENT);
    }
    
    if (ptr == NULL || ptr != arena->prev_alloc_ptr)
    {
        // Make a new allocation
        size_t new_total = arena->total_alloc_size + size;
        if (new_total > arena->arena_size || new_total < arena->total_alloc_size)
        {
            return NULL; // Out of memory
        }

        void *result = (char*)arena->arena + arena->total_alloc_size;
        arena->total_alloc_size = new_total;
        
        if (ptr == NULL)
        {
            // No previous allocation
            arena->prev_alloc_ptr = result;
            arena->total_alloc_count++;
        }
        else
        {
            // Move data from previous allocation because we couldn't realloc in place.
            // We don't know the size of it, but can determine an upper bound.
            size_t cpy_size = ((char*)arena->prev_alloc_ptr - (char*)ptr);
            if (cpy_size > size) cpy_size = size;
            memcpy(result, ptr, cpy_size);
            arena->prev_alloc_ptr = result;
        }
        
        return result;
    }
    else
    {
        // Adjust size of latest allocation
        size_t old_size = ((char*)arena->arena + arena->total_alloc_size) - (char*)ptr;

        size_t new_total = arena->total_alloc_size - old_size + size;
        if (new_total > arena->arena_size || new_total < (arena->total_alloc_size - old_size))
        {
            return NULL; // Out of memory
        }
        
        arena->total_alloc_size = new_total;
        return ptr;
    }
}

void arena_free(pb_allocator_t *actx, void *ptr)
{
    arena_allocator_t *arena = (arena_allocator_t*)actx;

    if (ptr != NULL)
    {
        if (ptr == arena->prev_alloc_ptr)
        {
            // Latest allocation can be truly released
            arena->total_alloc_size = (char*)ptr - (char*)arena->arena;
        }

        arena->total_alloc_count--;
    }
}

void arena_allocator_init(arena_allocator_t *allocator, void *arena, size_t arena_size)
{
    memset(allocator, 0, sizeof(*allocator));
    allocator->allocator.realloc = &arena_realloc;
    allocator->allocator.free = &arena_free;
    allocator->arena = arena;
    allocator->arena_size = arena_size;

    // Align start pointer to void* size
    if ((uintptr_t)arena % ALLOC_ALIGNMENT != 0)
    {
        allocator->total_alloc_size += ALLOC_ALIGNMENT - ((uintptr_t)arena % ALLOC_ALIGNMENT);
    }
}
