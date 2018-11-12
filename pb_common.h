/* pb_common.h: Common support functions for pb_encode.c and pb_decode.c.
 * These functions are rarely needed by applications directly.
 */

#ifndef PB_COMMON_H_INCLUDED
#define PB_COMMON_H_INCLUDED

#include "pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Iterator for pb_field_t list */
struct pb_field_iter_s {
    const pb_field_t *descriptor;    /* Start of the pb_field_t array */
    void *message;                   /* Pointer to start of the structure */

    pb_size_t index;                 /* Zero-based index of the field */
    pb_size_t required_field_index;  /* Zero-based index that counts only the required fields */
    pb_size_t submessage_index;      /* Zero-based index that counts only submessages */

    pb_size_t tag;                   /* Tag of current field */
    pb_size_t data_size;             /* sizeof() of a single item */
    pb_size_t array_size;            /* Number of array entries */
    pb_type_t type;                  /* Type of current field */

    void *pField;                    /* Pointer to current field in struct */
    void *pData;                     /* Pointer to current data contents. Different than pField for arrays and pointers. */
    void *pSize;                     /* Pointer to count/has field */

    const pb_field_t *submsg_desc;   /* For submessage fields, pointer to field descriptor for the submessage. */
    const void *default_value;       /* Pointer to default value for the field */
};
typedef struct pb_field_iter_s pb_field_iter_t;

/* Initialize the field iterator structure to beginning.
 * Returns false if the message type is empty. */
bool pb_field_iter_begin(pb_field_iter_t *iter, const pb_field_t *fields, void *message);

/* Advance the iterator to the next field.
 * Returns false when the iterator wraps back to the first field. */
bool pb_field_iter_next(pb_field_iter_t *iter);

/* Advance the iterator until it points at a field with the given tag.
 * Returns false if no such field exists. */
bool pb_field_iter_find(pb_field_iter_t *iter, uint32_t tag);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

