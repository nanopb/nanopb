/* pb_common.c: Common support functions for pb_encode.c and pb_decode.c.
 *
 * 2014 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb_common.h"

static bool load_descriptor_values(pb_field_iter_t *iter)
{
    uint32_t word0;
    uint32_t data_offset;
    uint8_t format;
    int8_t size_offset;

    if (iter->index >= iter->descriptor->field_count)
        return false;

    word0 = iter->descriptor->field_info[iter->field_info_index];
    format = word0 & 3;
    iter->tag = (pb_size_t)((word0 >> 2) & 0x3F);
    iter->type = (pb_type_t)((word0 >> 8) & 0xFF);

    if (format == 0)
    {
        /* 1-word format */
        iter->array_size = 1;
        size_offset = (int8_t)((word0 >> 24) & 0x0F);
        data_offset = (word0 >> 16) & 0xFF;
        iter->data_size = (pb_size_t)((word0 >> 28) & 0x0F);
    }
    else if (format == 1)
    {
        /* 2-word format */
        uint32_t word1 = iter->descriptor->field_info[iter->field_info_index + 1];

        iter->array_size = (pb_size_t)((word0 >> 16) & 0x0FFF);
        iter->tag = (pb_size_t)(iter->tag | ((word1 >> 28) << 6));
        size_offset = (int8_t)((word0 >> 28) & 0x0F);
        data_offset = word1 & 0xFFFF;
        iter->data_size = (pb_size_t)((word1 >> 16) & 0x0FFF);
    }
    else if (format == 2)
    {
        /* 4-word format */
        uint32_t word1 = iter->descriptor->field_info[iter->field_info_index + 1];
        uint32_t word2 = iter->descriptor->field_info[iter->field_info_index + 2];
        uint32_t word3 = iter->descriptor->field_info[iter->field_info_index + 3];

        iter->array_size = (pb_size_t)(word0 >> 16);
        iter->tag = (pb_size_t)(iter->tag | ((word1 >> 8) << 6));
        size_offset = (int8_t)(word1 & 0xFF);
        data_offset = word2;
        iter->data_size = (pb_size_t)word3;
    }
    else
    {
        /* 8-word format */
        uint32_t word1 = iter->descriptor->field_info[iter->field_info_index + 1];
        uint32_t word2 = iter->descriptor->field_info[iter->field_info_index + 2];
        uint32_t word3 = iter->descriptor->field_info[iter->field_info_index + 3];
        uint32_t word4 = iter->descriptor->field_info[iter->field_info_index + 4];

        iter->array_size = (pb_size_t)word4;
        iter->tag = (pb_size_t)(iter->tag | ((word1 >> 8) << 6));
        size_offset = (int8_t)(word1 & 0xFF);
        data_offset = word2;
        iter->data_size = (pb_size_t)word3;
    }

    iter->pField = (char*)iter->message + data_offset;

    if (size_offset)
    {
        iter->pSize = (char*)iter->pField - size_offset;
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

    if (PB_LTYPE(iter->type) == PB_LTYPE_SUBMESSAGE)
    {
        iter->submsg_desc = iter->descriptor->submsg_info[iter->submessage_index];
    }
    else
    {
        iter->submsg_desc = NULL;
    }

    return true;
}

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
         * - lowest 2 bits tell the amount of words in the descriptor (2^n words)
         * - bits 2..7 give the lowest bits of tag number.
         * - bits 8..15 give the field type.
         */
        uint32_t prev_descriptor = iter->descriptor->field_info[iter->field_info_index];
        pb_type_t prev_type = (prev_descriptor >> 8) & 0xFF;
        pb_size_t descriptor_len = (pb_size_t)(1 << (prev_descriptor & 3));

        iter->field_info_index = (pb_size_t)(iter->field_info_index + descriptor_len);

        if (PB_HTYPE(prev_type) == PB_HTYPE_REQUIRED)
        {
            iter->required_field_index++;
        }

        if (PB_LTYPE(prev_type) == PB_LTYPE_SUBMESSAGE)
        {
            iter->submessage_index++;
        }
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

    if (PB_ATYPE(msg->field_info[0] >> 8) == PB_ATYPE_POINTER)
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
    else
    {
        pb_size_t start = iter->index;
        uint32_t fieldinfo;

        do
        {
            /* Advance iterator but don't load values yet */
            advance_iterator(iter);

            /* Do fast check for tag number match */
            fieldinfo = iter->descriptor->field_info[iter->field_info_index];

            if (((fieldinfo >> 2) & 0x3F) == (tag & 0x3F))
            {
                /* Good candidate, check further */
                (void)load_descriptor_values(iter);

                if (iter->tag == tag &&
                    PB_LTYPE(iter->type) != PB_LTYPE_EXTENSION)
                {
                    /* Found it */
                    return true;
                }
            }
        } while (iter->index != start);

        /* Searched all the way back to start, and found nothing. */
        (void)load_descriptor_values(iter);
        return false;
    }
}

bool pb_default_field_callback(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_t *field)
{
    if (field->data_size == sizeof(pb_callback_t))
    {
        pb_callback_t *pCallback = (pb_callback_t*)field->pData;

        if (pCallback != NULL)
        {
            if (istream != NULL && pCallback->funcs.decode != NULL)
            {
#ifdef PB_OLD_CALLBACK_STYLE
                return pCallback->funcs.decode(istream, field, pCallback->arg);
#else
                return pCallback->funcs.decode(istream, field, &pCallback->arg);
#endif
            }

            if (ostream != NULL && pCallback->funcs.encode != NULL)
            {
#ifdef PB_OLD_CALLBACK_STYLE
                return pCallback->funcs.encode(ostream, field, pCallback->arg);
#else
                return pCallback->funcs.encode(ostream, field, &pCallback->arg);
#endif
            }
        }
    }

    return true; /* Success, but didn't do anything */

}


