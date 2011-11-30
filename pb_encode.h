#ifndef _PB_ENCODE_H_
#define _PB_ENCODE_H_

/* pb_encode.h: Functions to encode protocol buffers. Depends on pb_encode.c.
 * The main function is pb_encode. You also need an output stream, structures
 * and their field descriptions (just like with pb_decode).
 */

#include <stdbool.h>
#include "pb.h"

/* Lightweight output stream.
 * You can provide callback for writing or use pb_ostream_from_buffer.
 * 
 * Alternatively, callback can be NULL in which case the stream will just
 * count the number of bytes that would have been written. In this case
 * max_size is not checked.
 *
 * Rules for callback:
 * 1) Return false on IO errors. This will cause encoding to abort.
 * 
 * 2) You can use state to store your own data (e.g. buffer pointer).
 * 
 * 3) pb_write will update bytes_written after your callback runs.
 * 
 * 4) Substreams will modify max_size and bytes_written. Don't use them to
 * calculate any pointers.
 */
struct _pb_ostream_t
{
    bool (*callback)(pb_ostream_t *stream, const uint8_t *buf, size_t count);
    void *state; /* Free field for use by callback implementation */
    size_t max_size; /* Limit number of output bytes written (or use SIZE_MAX). */
    size_t bytes_written;
};

pb_ostream_t pb_ostream_from_buffer(uint8_t *buf, size_t bufsize);
bool pb_write(pb_ostream_t *stream, const uint8_t *buf, size_t count);

/* Encode struct to given output stream.
 * Returns true on success, false on any failure.
 * The actual struct pointed to by src_struct must match the description in fields.
 * All required fields in the struct are assumed to have been filled in.
 */
bool pb_encode(pb_ostream_t *stream, const pb_field_t fields[], const void *src_struct);

/* --- Helper functions ---
 * You may want to use these from your caller or callbacks.
 */

bool pb_encode_varint(pb_ostream_t *stream, uint64_t value);
bool pb_encode_tag(pb_ostream_t *stream, pb_wire_type_t wiretype, int field_number);
/* Encode tag based on LTYPE and field number defined in the field structure. */
bool pb_encode_tag_for_field(pb_ostream_t *stream, const pb_field_t *field);
/* Write length as varint and then the contents of buffer. */
bool pb_encode_string(pb_ostream_t *stream, const uint8_t *buffer, size_t size);

/* --- Field encoders ---
 * Each encoder writes the content for the field.
 * The tag/wire type has been written already.
 */

bool pb_enc_varint(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_svarint(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_fixed32(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_fixed64(pb_ostream_t *stream, const pb_field_t *field, const void *src);

bool pb_enc_bytes(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_string(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_submessage(pb_ostream_t *stream, const pb_field_t *field, const void *src);

#endif
