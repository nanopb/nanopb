/* pb_common.c: Common support functions for pb_encode.c and pb_decode.c.
 *
 * 2014 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb_common.h"

static bool load_descriptor_values(pb_field_iter_t *iter)
{
    if (iter->index >= iter->descriptor->field_count)
        return false;

    const uint32_t *field_info = &iter->descriptor->field_info[iter->field_info_index];
    pb_size_t size_offset, data_offset;

    if (iter->descriptor->msg_flags & PB_MSGFLAG_LARGEDESC)
    {
        // Load descriptor values for large format (5 words)
        uint32_t word0 = PB_PROGMEM_READU32(field_info[0]);
        uint32_t word1 = PB_PROGMEM_READU32(field_info[1]);

        iter->tag = (pb_size_t)(word0 & 0x1FFFFFFF);
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

        iter->tag = (pb_size_t)(word0 & 0xFFF);
        iter->type = (pb_type_t)(word1 >> 24);
        iter->data_size = (pb_size_t)(word1 & 0xFFF);
        iter->array_size = (pb_size_t)(word0 >> 24);

        data_offset = (pb_size_t)((word0 >> 12) & 0xFFF);
        size_offset = (pb_size_t)((word1 >> 12) & 0xFFF);
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

        if (PB_ATYPE(iter->type) == PB_ATYPE_POINTER && iter->pField != NULL)
        {
            iter->pData = *(void**)iter->pField;
        }
        else
        {
            iter->pData = iter->pField;
        }
    }

    if (PB_LTYPE_IS_SUBMSG(iter->type))
    {
        iter->submsg_desc = iter->descriptor->submsg_info[iter->submessage_index];
    }
    else
    {
        iter->submsg_desc = NULL;
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
static inline pb_size_t get_tag_quick(const pb_field_iter_t *iter)
{
    uint32_t word0 = PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index]);

    if (iter->descriptor->msg_flags & PB_MSGFLAG_LARGEDESC)
        return (pb_size_t)(word0 & 0x1FFFFFFF);
    else
        return (pb_size_t)(word0 & 0xFFF);
}

// Go to next field but do not load descriptor data yet
static void advance_iterator(pb_field_iter_t *iter)
{
    iter->index++;

    if (iter->index >= iter->descriptor->field_count)
    {
        /* Restart */
        iter->index = 0;
        iter->field_info_index = 0;
        iter->submessage_index = 0;
        iter->required_field_index = 0;
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
         * The cast to pb_size_t is needed to avoid -Wconversion warning.
         * Because the data is is constants from generator, there is no danger of overflow.
         */
        pb_size_t desc_len = (iter->descriptor->msg_flags & PB_MSGFLAG_LARGEDESC) ? 5 : 2;
        iter->field_info_index = (pb_size_t)(iter->field_info_index + desc_len);
        iter->required_field_index = (pb_size_t)(iter->required_field_index + (PB_HTYPE(prev_type) == PB_HTYPE_REQUIRED));
        iter->submessage_index = (pb_size_t)(iter->submessage_index + PB_LTYPE_IS_SUBMSG(prev_type));
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
    const pb_msgdesc_t *msg = (const pb_msgdesc_t*)extension->type->arg;
    bool status;

    pb_type_t type = get_type_quick(msg->field_info);
    if (PB_ATYPE(type) == PB_ATYPE_POINTER)
    {
        /* For pointer extensions, the pointer is stored directly
         * in the extension structure. This avoids having an extra
         * indirection. */
        status = pb_field_iter_begin(iter, msg, &extension->dest);
    }
    else
    {
        status = pb_field_iter_begin(iter, msg, extension->dest);
    }

    iter->pSize = &extension->found;
    return status;
}

bool pb_field_iter_next(pb_field_iter_t *iter)
{
    advance_iterator(iter);
    (void)load_descriptor_values(iter);
    return iter->index != 0;
}

bool pb_field_iter_find(pb_field_iter_t *iter, uint32_t tag)
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
        pb_size_t field_count = iter->descriptor->field_count;

        if (tag < iter->tag)
        {
            /* Fields are in tag number order, so we know that tag is between
             * 0 and our start position. Setting index to end forces
             * advance_iterator() call below to restart from beginning. */
            iter->index = field_count;
        }

        pb_size_t itertag = 0;
        do
        {
            /* Advance iterator but don't load values yet */
            advance_iterator(iter);

            /* Compare tag and load values if match */
            itertag = get_tag_quick(iter);
            if (itertag == tag)
            {
                return load_descriptor_values(iter);
            }
        } while (itertag < tag);

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
            advance_iterator(iter);

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

