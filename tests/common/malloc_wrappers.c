/* The wrapper functions in this file work like regular malloc() and free(),
 * but store check values before and after the allocation. This helps to catch
 * any buffer overrun errors in the test cases.
 */

#include "malloc_wrappers.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

static size_t alloc_count = 0;

#define GUARD_SIZE (sizeof(size_t)*3)
#define PREFIX_SIZE (sizeof(size_t)*2)
#define CHECK1 ((size_t)0xDEADBEEF)
#define CHECK2 ((size_t)0x600DCAFE)

#ifndef MAX_REALLOC_SIZE
#define MAX_REALLOC_SIZE 1024*1024
#endif

#ifndef DEBUG_MALLOC
#define DEBUG_MALLOC 0
#endif

/* Allocate memory and place check values before and after. */
void* malloc_with_check(size_t size)
{
    char *buf = malloc(size + GUARD_SIZE);
    if (buf)
    {
        ((size_t*)buf)[0] = size;
        ((size_t*)buf)[1] = CHECK1;
        ((size_t*)(buf + size))[2] = CHECK2;
        alloc_count++;
        if (DEBUG_MALLOC) fprintf(stderr, "Alloc 0x%04x/%u\n", (unsigned)(uintptr_t)(buf + PREFIX_SIZE), (unsigned)size);
        return buf + PREFIX_SIZE;
    }
    else
    {
        if (DEBUG_MALLOC) fprintf(stderr, "malloc(%u) failed\n", (unsigned)size);
        return NULL;
    }
}

/* Free memory allocated with malloc_with_check() and do the checks. */
void free_with_check(void *mem)
{
    if (mem)
    {
        char *buf = (char*)mem - PREFIX_SIZE;
        size_t size = ((size_t*)buf)[0];
        assert(((size_t*)buf)[1] == CHECK1);
        assert(((size_t*)(buf + size))[2] == CHECK2);
        assert(alloc_count > 0);
        alloc_count--;
        if (DEBUG_MALLOC) fprintf(stderr, "Release 0x%04x/%u\n", (unsigned)(uintptr_t)mem, (unsigned)size);
        free(buf);
    }
}

/* Reallocate block and check / write guard values */
void* realloc_with_check(void *ptr, size_t size)
{
    /* Don't allocate crazy amounts of RAM when fuzzing */
    if (size > MAX_REALLOC_SIZE)
        return NULL;
    
    if (!ptr && size)
    {
        /* Allocate new block and write guard values */
        return malloc_with_check(size);
    }
    else if (ptr && size)
    {
        /* Change block size */
        char *buf = (char*)ptr - PREFIX_SIZE;
        size_t oldsize = ((size_t*)buf)[0];
        assert(((size_t*)buf)[1] == CHECK1);
        assert(((size_t*)(buf + oldsize))[2] == CHECK2);
        assert(alloc_count > 0);

        buf = realloc(buf, size + GUARD_SIZE);
        if (!buf)
        {
            if (DEBUG_MALLOC) fprintf(stderr, "Realloc 0x%04x/%u to %u failed\n", (unsigned)(uintptr_t)ptr, (unsigned)oldsize, (unsigned)size);
            return NULL;
        }

        ((size_t*)buf)[0] = size;
        ((size_t*)buf)[1] = CHECK1;
        ((size_t*)(buf + size))[2] = CHECK2;

        if (DEBUG_MALLOC) fprintf(stderr, "Realloc 0x%04x/%u to 0x%04x/%u\n", (unsigned)(uintptr_t)ptr, (unsigned)oldsize, (unsigned)(uintptr_t)(buf + PREFIX_SIZE), (unsigned)size);
        return buf + PREFIX_SIZE;
    }
    else if (ptr && !size)
    {
        /* Deallocate */
        free_with_check(ptr);
        return NULL;
    }
    else
    {
        /* No action */
        return NULL;
    }
}

/* Return total number of allocations not yet released */
size_t get_alloc_count()
{
    return alloc_count;
}

/* Return allocated size for a pointer returned from malloc(). */
size_t get_allocation_size(const void *mem)
{
    char *buf = (char*)mem - PREFIX_SIZE;
    return ((size_t*)buf)[0];
}
