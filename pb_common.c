/* pb_common.c: Common support functions for pb_encode.c and pb_decode.c.
 *
 * 2014 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb_common.h"

// Get the pointer to start of data, based on if the field is a pointer
static inline void* field_data_base_ptr(const pb_field_iter_t *iter)
{
    if (PB_ATYPE(iter->type) == PB_ATYPE_POINTER)
    {
        return *(void**)iter->pField;
    }
    else
    {
        return iter->pField;
    }
}

// Load field iterator values from the descriptor array and setup pointers
static bool load_descriptor_values(pb_field_iter_t *iter)
{
    if (iter->index >= iter->descriptor->field_count)
    {
        /* This is used to indicate end of message.
         * It is used in pb_walk() and also for empty message types.
         */
        iter->tag = 0;
        iter->type = 0;
        iter->data_size = iter->array_size = 0;
        iter->pData = iter->pField = iter->pSize = 0;
        return false;
    }

    const uint32_t *field_info = &iter->descriptor->field_info[iter->field_info_index];
    pb_size_t size_offset, data_offset;

    if (iter->descriptor->msg_flags & PB_MSGFLAG_LARGEDESC)
    {
        // Load descriptor values for large format (5 words)
        uint32_t word0 = PB_PROGMEM_READU32(field_info[0]);
        uint32_t word1 = PB_PROGMEM_READU32(field_info[1]);

        iter->tag = (pb_tag_t)(word0 & 0x1FFFFFFF);
        iter->type = (pb_type_t)(((word0 >> 21) & 0x700) | (word1 >> 24));
        iter->array_size = (pb_size_t)(word1 & 0xFFFFFF);
        iter->data_size = (pb_size_t)PB_PROGMEM_READU32(field_info[2]);

        size_offset = (pb_size_t)PB_PROGMEM_READU32(field_info[3]);
        data_offset = (pb_size_t)PB_PROGMEM_READU32(field_info[4]);
    }
    else
    {
        // Load descriptor values for small descriptor format (2 words)
        uint32_t word0 = PB_PROGMEM_READU32(field_info[0]);
        uint32_t word1 = PB_PROGMEM_READU32(field_info[1]);

        iter->tag = (pb_tag_t)(word0 & 0xFFF);
        iter->type = (pb_type_t)(word1 >> 24);
        iter->data_size = (pb_size_t)(word1 & 0xFFF);
        iter->array_size = (pb_size_t)(word0 >> 24);

        data_offset = (pb_size_t)((word0 >> 12) & 0xFFF);
        size_offset = (pb_size_t)((word1 >> 12) & 0xFFF);
    }

    // Get pointer to submessage type
    if (PB_LTYPE_IS_SUBMSG(iter->type))
    {
        iter->submsg_desc = iter->descriptor->submsg_info[iter->submessage_index];
    }
    else
    {
        iter->submsg_desc = NULL;
    }

    // Calculate pField and pSize pointers from the offsets given by descriptor
    if (!iter->message)
    {
        /* Avoid doing arithmetic on null pointers, it is undefined */
        iter->pField = NULL;
        iter->pSize = NULL;
    }
    else
    {
        iter->pField = (char*)iter->message + data_offset;

        if (size_offset != data_offset)
        {
            iter->pSize = (char*)iter->message + size_offset;
        }
        else if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED &&
                 (PB_ATYPE(iter->type) == PB_ATYPE_STATIC ||
                  PB_ATYPE(iter->type) == PB_ATYPE_POINTER))
        {
            /* Fixed count array */
            iter->pSize = &iter->array_size;
        }
        else
        {
            // No size field
            iter->pSize = NULL;
        }

        iter->pData = field_data_base_ptr(iter);
    }

    return true;
}

// Get field type without loading rest of the descriptor
static inline pb_type_t get_type_quick(const uint32_t *field_info)
{
    uint32_t word1 = PB_PROGMEM_READU32(field_info[1]);
    return (pb_type_t)(word1 >> 24);
}

// Get field tag without loading rest of the descriptor
static inline pb_tag_t get_tag_quick(const pb_field_iter_t *iter)
{
    uint32_t word0 = PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index]);

    if (iter->descriptor->msg_flags & PB_MSGFLAG_LARGEDESC)
        return (pb_tag_t)(word0 & 0x1FFFFFFF);
    else
        return (pb_tag_t)(word0 & 0xFFF);
}

// Return number of words per descriptor entry
static inline pb_fieldidx_t descsize(const pb_field_iter_t *iter)
{
    return (iter->descriptor->msg_flags & PB_MSGFLAG_LARGEDESC) ? 5 : 2;
}

// Go to next field but do not load descriptor data yet
static void advance_iterator(pb_field_iter_t *iter, bool wrap)
{
    iter->index++;

    if (wrap && iter->index >= iter->descriptor->field_count)
    {
        /* Restart */
        (void)pb_field_iter_reset(iter);
    }
    else
    {
        /* Increment indexes based on previous field type.
         * All field info formats have the following fields:
         * - lowest 12 bits of the first word contain the tag number
         * - highest 8 bits of the second word contain the field type
         */
        pb_type_t prev_type = get_type_quick(&iter->descriptor->field_info[iter->field_info_index]);

        /* Add to fields.
         * The cast to pb_fieldidx_t is needed to avoid -Wconversion warning.
         * Because the data is is constants from generator, there is no danger of overflow.
         */
        iter->field_info_index = (pb_fieldidx_t)(iter->field_info_index + descsize(iter));
        iter->required_field_index = (pb_fieldidx_t)(iter->required_field_index + (PB_HTYPE(prev_type) == PB_HTYPE_REQUIRED));
        iter->submessage_index = (pb_fieldidx_t)(iter->submessage_index + PB_LTYPE_IS_SUBMSG(prev_type));
    }
}

bool pb_field_iter_begin(pb_field_iter_t *iter, const pb_msgdesc_t *desc, void *message)
{
    memset(iter, 0, sizeof(*iter));

    iter->descriptor = desc;
    iter->message = message;

    return load_descriptor_values(iter);
}

bool pb_field_iter_begin_extension(pb_field_iter_t *iter, pb_extension_t *extension)
{
    const pb_msgdesc_t *msg = (const pb_msgdesc_t*)extension->type;
    bool status;

    void *data = pb_get_extension_data_ptr(extension);
    status = pb_field_iter_begin(iter, msg, data);
    iter->pSize = &extension->found;
    return status;
}

void *pb_get_extension_data_ptr(pb_extension_t *extension)
{
    const pb_msgdesc_t *msg = (const pb_msgdesc_t*)extension->type;

    pb_type_t type = get_type_quick(msg->field_info);
    if (PB_ATYPE(type) == PB_ATYPE_POINTER)
    {
        /* For pointer extensions, the pointer is stored directly
         * in the extension structure. This avoids having an extra
         * indirection. */
        return &extension->dest;
    }
    else
    {
        return extension->dest;
    }
}

bool pb_field_iter_reset(pb_field_iter_t *iter)
{
    iter->index = 0;
    iter->field_info_index = 0;
    iter->submessage_index = 0;
    iter->required_field_index = 0;
    return load_descriptor_values(iter);
}

bool pb_field_iter_next(pb_field_iter_t *iter)
{
    advance_iterator(iter, true);
    (void)load_descriptor_values(iter);
    return iter->index != 0;
}

bool pb_field_iter_find(pb_field_iter_t *iter, pb_tag_t tag)
{
    if (iter->tag == tag)
    {
        return true; /* Nothing to do, correct field already. */
    }
    else if (tag > iter->descriptor->largest_tag)
    {
        return false;
    }
    else
    {
        pb_fieldidx_t field_count = iter->descriptor->field_count;

        if (tag < iter->tag)
        {
            /* Fields are in tag number order, so we know that tag is between
             * 0 and our start position. Setting index to end forces
             * advance_iterator() call below to restart from beginning. */
            iter->index = field_count;
        }

        pb_tag_t itertag = 0;
        do
        {
            /* Advance iterator but don't load values yet */
            advance_iterator(iter, true);

            /* Compare tag and load values if match */
            itertag = get_tag_quick(iter);
            if (itertag == tag)
            {
                return load_descriptor_values(iter);
            }
        } while (itertag < tag && iter->index < field_count - 1);

        /* Searched until first tag number beyond, and found nothing. */
        (void)load_descriptor_values(iter);
        return false;
    }
}

bool pb_field_iter_find_extension(pb_field_iter_t *iter)
{
    if (PB_LTYPE(iter->type) == PB_LTYPE_EXTENSION)
    {
        return true;
    }
    else
    {
        pb_size_t start = iter->index;

        do
        {
            /* Advance iterator but don't load values yet */
            advance_iterator(iter, true);

            /* Do fast check for field type */
            pb_type_t type = get_type_quick(&iter->descriptor->field_info[iter->field_info_index]);
            if (PB_LTYPE(type) == PB_LTYPE_EXTENSION)
            {
                return load_descriptor_values(iter);
            }
        } while (iter->index != start);

        /* Searched all the way back to start, and found nothing. */
        (void)load_descriptor_values(iter);
        return false;
    }
}

static void *pb_const_cast(const void *p)
{
    /* Note: this casts away const, in order to use the common field iterator
     * logic for both encoding and decoding. The cast is done using union
     * to avoid spurious compiler warnings. */
    union {
        void *p1;
        const void *p2;
    } t;
    t.p2 = p;
    return t.p1;
}

bool pb_field_iter_begin_const(pb_field_iter_t *iter, const pb_msgdesc_t *desc, const void *message)
{
    return pb_field_iter_begin(iter, desc, pb_const_cast(message));
}

bool pb_field_iter_begin_extension_const(pb_field_iter_t *iter, const pb_extension_t *extension)
{
    return pb_field_iter_begin_extension(iter, (pb_extension_t*)pb_const_cast(extension));
}

bool pb_default_field_callback(pb_decode_ctx_t *decctx, pb_encode_ctx_t *encctx, const pb_field_t *field)
{
    if (field->data_size == sizeof(pb_callback_t))
    {
        pb_callback_t *pCallback = (pb_callback_t*)field->pData;

        if (pCallback != NULL)
        {
            if (decctx != NULL && pCallback->funcs.decode != NULL)
            {
                return pCallback->funcs.decode(decctx, field, &pCallback->arg);
            }

            if (encctx != NULL && pCallback->funcs.encode != NULL)
            {
                return pCallback->funcs.encode(encctx, field, &pCallback->arg);
            }
        }
    }

    return true; /* Success, but didn't do anything */

}

/* Information stored internally by pb_walk() for each submessage level. */
typedef struct {
    /* Message descriptor and structure */
    const pb_msgdesc_t *descriptor;
    void *message;

    /* pb_field_iter_t state */
    pb_fieldidx_t index;
    pb_fieldidx_t required_field_index;
    pb_fieldidx_t submessage_index;

    /* Size of the previous user stackframe */
    pb_walk_stacksize_t prev_stacksize;
} pb_walk_stackframe_t;

#ifndef PB_WALK_STACK_ALIGN_OVERRIDE
typedef void* pb_walk_stack_align_t;
#else
typedef PB_WALK_STACK_ALIGN_OVERRIDE pb_walk_stack_align_t;
#endif

#define PB_WALK_ALIGNSIZE sizeof(pb_walk_stack_align_t)

// Round byte count upwards to a multiple of sizeof(void*) to retain alignment
#define ALIGN_BYTES(x) (pb_walk_stacksize_t)((((x) - 1) / PB_WALK_ALIGNSIZE + 1) * PB_WALK_ALIGNSIZE)

// Configured stack size must also be divisible by alignment requirement
PB_STATIC_ASSERT(ALIGN_BYTES(PB_WALK_STACK_SIZE) == PB_WALK_STACK_SIZE, PB_WALK_STACK_SIZE_not_aligned)

bool pb_walk_init(pb_walk_state_t *state, const pb_msgdesc_t *desc, const void *message, pb_walk_cb_t callback)
{
    state->callback = callback;
    state->stack = NULL;
    state->stacksize = 0;
    state->next_stacksize = 0;
    state->ctx = NULL;
    state->flags = 0;
    state->depth = 0;
    state->max_depth = PB_MESSAGE_NESTING_MAX;
    state->retval = PB_WALK_EXIT_OK;

#ifndef PB_NO_ERRMSG
    state->errmsg = NULL;
#endif

    return pb_field_iter_begin_const(&state->iter, desc, message);
}

bool pb_walk(pb_walk_state_t *state)
{
    // Manually managed stack for storing only the necessary data for
    // walking the message tree.
    union {
        pb_walk_stack_align_t alignment;
        char buf[PB_WALK_STACK_SIZE];
    } aligned_storage;
    char *storage = aligned_storage.buf;
    pb_walk_stacksize_t pos = PB_WALK_STACK_SIZE;

    // Check if this is recursive pb_walk() call
    if (state->retval != PB_WALK_IN)
    {
        // Allocate first frame
        state->stacksize = ALIGN_BYTES(state->next_stacksize);
        if (state->stacksize > pos) PB_RETURN_ERROR(state, "PB_WALK_STACK_SIZE exceeded");
        pos -= state->stacksize;
        state->stack = &storage[pos];
        memset(&storage[pos], 0, state->stacksize);

        // Invoke the first callback
        state->retval = state->callback(state);
    }

    while (state->retval > 0)
    {
        pb_field_iter_t *iter = &state->iter;

        if (state->retval == PB_WALK_IN)
        {
            /* Enter into a submessage */

            pb_walk_stacksize_t cb_stacksize = ALIGN_BYTES(state->next_stacksize);
            pb_walk_stacksize_t our_stacksize = ALIGN_BYTES(sizeof(pb_walk_stackframe_t));

            if (pos < cb_stacksize + our_stacksize)
            {
                /* Not enough space for new stackframe.
                 * Use recursion to allocate more stack.
                 */
#ifndef PB_NO_RECURSION
                if (!pb_walk(state)) return false;
                state->stack = &storage[pos];
#else
                PB_RETURN_ERROR(state, "recursion disabled");
#endif
            }
            else
            {
                if (state->depth >= state->max_depth)
                {
                    PB_RETURN_ERROR(state, "max_depth exceeded");
                }
                state->depth++;

                // Store iterator state so that we can restore it after return
                pos -= our_stacksize;
                pb_walk_stackframe_t *frame = (pb_walk_stackframe_t*)&storage[pos];
                frame->descriptor = iter->descriptor;
                frame->message = iter->message;
                frame->index = iter->index;
                frame->required_field_index = iter->required_field_index;
                frame->submessage_index = iter->submessage_index;
                frame->prev_stacksize = state->stacksize;

                // Setup stack frame for callback
                state->stacksize = cb_stacksize;
                pos -= cb_stacksize;
                state->stack = &storage[pos];
                memset(&storage[pos], 0, cb_stacksize);

                // Reset iterator to submessage
                (void)pb_field_iter_begin(&state->iter, iter->submsg_desc, iter->pData);
            }
        }
        else if (state->retval == PB_WALK_OUT)
        {
            /* Return from submessage */

            if (state->depth == 0)
            {
                // Exit from topmost level
                state->retval = PB_WALK_EXIT_OK;
                break;
            }
            state->depth--;

            // Restore iterator state
            pos += state->stacksize;
            const void *old_desc = iter->descriptor;
            void *old_msg = iter->message;
            const pb_walk_stackframe_t *frame = (const pb_walk_stackframe_t*)&storage[pos];
            iter->descriptor = frame->descriptor;
            iter->message = frame->message;
            iter->index = frame->index;
            iter->required_field_index = frame->required_field_index;
            iter->submessage_index = frame->submessage_index;
            iter->field_info_index = descsize(&state->iter) * frame->index;
            (void)load_descriptor_values(&state->iter);

            // Restore pData value for PB_WALK_NEXT_ITEM to work
            if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED)
            {
                iter->pData = old_msg;
            }
            else if (PB_LTYPE(iter->type) == PB_LTYPE_EXTENSION)
            {
                // Find extension that contained this dest
                // Both type and data pointer need to match
                pb_extension_t **ext = (pb_extension_t**)iter->pData;

                while (ext != NULL && *ext != NULL)
                {
                    if ((*ext)->type == old_desc)
                    {
                        if ((*ext)->dest == old_msg || &(*ext)->dest == old_msg)
                        {
                            break; // Found the matching extension
                        }
                    }
                    ext = &(*ext)->next;
                }

                if (ext)
                    iter->pData = ext;
                else
                    iter->pData = NULL;
            }

            // Restore previous stack frame
            state->stacksize = frame->prev_stacksize;
            pos += ALIGN_BYTES(sizeof(pb_walk_stackframe_t));
            state->stack = &storage[pos];

            if (pos >= PB_WALK_STACK_SIZE)
            {
                // Exit from recursion
                return true;
            }
        }
        else
        {
            bool go_next_field = true;

            if (state->retval == PB_WALK_NEXT_ITEM)
            {
                if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED)
                {
                    pb_size_t count = *(pb_size_t*)iter->pSize;
                    if (count > iter->array_size && PB_ATYPE(iter->type) != PB_ATYPE_POINTER)
                    {
                        PB_RETURN_ERROR(state, "array max size exceeded");
                    }

                    // Check that old pData is within the array bounds and that
                    // new pointer is not past the end.
                    char *baseptr = (char*)field_data_base_ptr(iter);
                    char *oldptr = (char*)iter->pData;
                    char *newptr = oldptr + iter->data_size;
                    char *endptr = baseptr + iter->data_size * count;

                    if (oldptr >= baseptr && newptr < endptr)
                    {
                        iter->pData = newptr;
                        go_next_field = false;
                    }
                }
                else if (PB_LTYPE(iter->type) == PB_LTYPE_EXTENSION)
                {
                    if (iter->data_size != sizeof(pb_extension_t*))
                    {
                        PB_RETURN_ERROR(state, "invalid extension");
                    }

                    // Go to next linked list node
                    pb_extension_t *ext = *(pb_extension_t**)iter->pData;
                    if (ext)
                    {
                        iter->pData = &ext->next;
                        go_next_field = false;
                    }
                }
            }

            if (go_next_field)
            {
                // Go to next field, do not wrap at the end of message
                advance_iterator(&state->iter, false);
                (void)load_descriptor_values(&state->iter);
            }
        }

        state->retval = state->callback(state);
    }

    return state->retval == PB_WALK_EXIT_OK;
}

#ifdef PB_VALIDATE_UTF8

/* This function checks whether a string is valid UTF-8 text.
 *
 * Algorithm is adapted from https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c
 * Original copyright: Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/> 2005-03-30
 * Licensed under "Short code license", which allows use under MIT license or
 * any compatible with it.
 */

bool pb_validate_utf8(const char *str)
{
    const pb_byte_t *s = (const pb_byte_t*)str;
    while (*s)
    {
        if (*s < 0x80)
        {
            /* 0xxxxxxx */
            s++;
        }
        else if ((s[0] & 0xe0) == 0xc0)
        {
            /* 110XXXXx 10xxxxxx */
            if ((s[1] & 0xc0) != 0x80 ||
                (s[0] & 0xfe) == 0xc0)                        /* overlong? */
                return false;
            else
                s += 2;
        }
        else if ((s[0] & 0xf0) == 0xe0)
        {
            /* 1110XXXX 10Xxxxxx 10xxxxxx */
            if ((s[1] & 0xc0) != 0x80 ||
                (s[2] & 0xc0) != 0x80 ||
                (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) ||    /* overlong? */
                (s[0] == 0xed && (s[1] & 0xe0) == 0xa0) ||    /* surrogate? */
                (s[0] == 0xef && s[1] == 0xbf &&
                (s[2] & 0xfe) == 0xbe))                 /* U+FFFE or U+FFFF? */
                return false;
            else
                s += 3;
        }
        else if ((s[0] & 0xf8) == 0xf0)
        {
            /* 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx */
            if ((s[1] & 0xc0) != 0x80 ||
                (s[2] & 0xc0) != 0x80 ||
                (s[3] & 0xc0) != 0x80 ||
                (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||    /* overlong? */
                (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) /* > U+10FFFF? */
                return false;
            else
                s += 4;
        }
        else
        {
            return false;
        }
    }

    return true;
}

#endif

