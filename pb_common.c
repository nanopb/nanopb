/* pb_common.c: Common support functions for pb_encode.c and pb_decode.c.
 *
 * 2014 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb_common.h"

static bool load_descriptor_values(pb_field_iter_t *iter)
{
    pb_size_t index = iter->index;

    iter->tag = iter->descriptor[index].tag;
    iter->data_size = iter->descriptor[index].data_size;
    iter->array_size = iter->descriptor[index].array_size;
    iter->type = iter->descriptor[index].type;

    if (iter->descriptor[index].size_offset)
    {
        iter->pSize = (char*)iter->pField + iter->descriptor[index].size_offset;
    }
    else if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED &&
             (PB_ATYPE(iter->type) == PB_ATYPE_STATIC || PB_ATYPE(iter->type) == PB_ATYPE_POINTER))
    {
        /* Fixed count array */
        iter->pSize = &iter->array_size;
    }
    else
    {
        iter->pSize = NULL;
    }

    if (PB_ATYPE(iter->type) == PB_ATYPE_POINTER)
    {
        iter->pData = *(void**)iter->pField;
    }
    else
    {
        iter->pData = iter->pField;
    }

    if (PB_LTYPE(iter->type) == PB_LTYPE_SUBMESSAGE)
    {
        iter->submsg_desc = (const pb_field_t*)iter->descriptor[index].ptr;
        iter->default_value = NULL;
    }
    else
    {
        iter->submsg_desc = NULL;
        iter->default_value = iter->descriptor[index].ptr;
    }

    return iter->tag != 0;
}

static bool advance_iterator(pb_field_iter_t *iter)
{
    iter->index++;

    if (iter->descriptor[iter->index].tag == 0)
    {
        if (iter->index == 0)
        {
            return false;
        }
        else
        {
            /* Restart */
            iter->index = 0;
            iter->submessage_index = 0;
            iter->required_field_index = 0;
            iter->pField = (char*)iter->message + iter->descriptor[0].data_offset;
        }
    }
    else
    {
        /* Increment indexes based on previous field type */

        if (PB_HTYPE(iter->type) == PB_HTYPE_REQUIRED)
        {
            iter->required_field_index++;
        }

        if (PB_LTYPE(iter->type) == PB_LTYPE_SUBMESSAGE)
        {
            iter->submessage_index++;
        }

        /* Increment pointers based on previous field type & size */
        if (PB_HTYPE(iter->type) == PB_HTYPE_ONEOF &&
            PB_HTYPE(iter->descriptor[iter->index].type) == PB_HTYPE_ONEOF &&
            iter->descriptor[iter->index].data_offset == PB_SIZE_MAX)
        {
            /* Don't advance pointers inside unions */
        }
        else if (PB_ATYPE(iter->type) == PB_ATYPE_STATIC &&
                PB_HTYPE(iter->type) == PB_HTYPE_REPEATED)
        {
            /* In static arrays, the data_size tells the size of a single entry and
            * array_size is the number of entries */
            iter->pField = (char*)iter->pField + iter->data_size * iter->array_size + iter->descriptor[iter->index].data_offset;
        }
        else if (PB_ATYPE(iter->type) == PB_ATYPE_POINTER)
        {
            /* Pointer fields always have a constant size in the main structure.
            * The data_size only applies to the dynamically allocated area. */
            iter->pField = (char*)iter->pField + sizeof(void*) + iter->descriptor[iter->index].data_offset;
        }
        else
        {
            /* Static fields and callback fields */
            iter->pField = (char*)iter->pField + iter->data_size + iter->descriptor[iter->index].data_offset;
        }
    }

    return load_descriptor_values(iter);
}

bool pb_field_iter_begin(pb_field_iter_t *iter, const pb_field_t *fields, void *message)
{
    memset(iter, 0, sizeof(*iter));

    iter->descriptor = fields;
    iter->message = message;
    iter->pField = (char*)iter->message + iter->descriptor[0].data_offset;

    return load_descriptor_values(iter);
}

bool pb_field_iter_next(pb_field_iter_t *iter)
{
    if (iter->tag == 0)
    {
        /* Handle empty message types. In other cases, iter->tag is never 0. */
        return false;
    }

    (void)advance_iterator(iter);

    return iter->index != 0;
}

bool pb_field_iter_find(pb_field_iter_t *iter, uint32_t tag)
{
    pb_size_t start = iter->index;
    
    do {
        if (iter->tag == tag &&
            PB_LTYPE(iter->type) != PB_LTYPE_EXTENSION)
        {
            /* Found the wanted field */
            return true;
        }
        
        (void)advance_iterator(iter);
    } while (iter->index != start);
    
    /* Searched all the way back to start, and found nothing. */
    return false;
}


