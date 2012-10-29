#ifndef _PB_ENCODE_H_
#define _PB_ENCODE_H_

/* pb_encode.h: Functions to encode protocol buffers. Depends on pb_encode.c.
 * The main function is pb_encode. You also need an output stream, structures
 * and their field descriptions (just like with pb_decode).
 */

#include <stdbool.h>
#include "pb.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/* Encode field header based on LTYPE and field number defined in the field structure.
 * Call this from the callback before writing out field contents. */
bool pb_encode_tag_for_field(pb_ostream_t *stream, const pb_field_t *field);

/* Encode field header by manually specifing wire type. You need to use this if
 * you want to write out packed arrays from a callback field. */
bool pb_encode_tag(pb_ostream_t *stream, pb_wire_type_t wiretype, uint32_t field_number);

/* Encode an integer in the varint format.
 * This works for bool, enum, int32, int64, uint32 and uint64 field types. */
bool pb_encode_varint(pb_ostream_t *stream, uint64_t value);

/* Encode an integer in the zig-zagged svarint format.
 * This works for sint32 and sint64. */
bool pb_encode_svarint(pb_ostream_t *stream, int64_t value);

/* Encode a string or bytes type field. For strings, pass strlen(s) as size. */
bool pb_encode_string(pb_ostream_t *stream, const uint8_t *buffer, size_t size);

/* Encode a fixed32, sfixed32 or float value.
 * You need to pass a pointer to a 4-byte wide C variable. */
bool pb_encode_fixed32(pb_ostream_t *stream, const void *value);

/* Encode a fixed64, sfixed64 or double value.
 * You need to pass a pointer to a 8-byte wide C variable. */
bool pb_encode_fixed64(pb_ostream_t *stream, const void *value);

/* Encode a submessage field.
 * You need to pass the pb_field_t array and pointer to struct, just like with pb_encode().
 * This internally encodes the submessage twice, first to calculate message size and then to actually write it out.
 */
bool pb_encode_submessage(pb_ostream_t *stream, const pb_field_t fields[], const void *src_struct);

/* --- Internal functions ---
 * These functions are not terribly useful for the average library user, but
 * are exported to make the unit testing and extending nanopb easier.
 */

#ifdef NANOPB_INTERNALS
bool pb_enc_varint(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_svarint(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_fixed32(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_fixed64(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_bytes(pb_ostream_t *stream, const pb_field_t *field, const void *src);
bool pb_enc_string(pb_ostream_t *stream, const pb_field_t *field, const void *src);
#endif

/* This function is not recommended for new programs. Use pb_encode_submessage()
 * instead, it has the same functionality with a less confusing interface. */
bool pb_enc_submessage(pb_ostream_t *stream, const pb_field_t *field, const void *src);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
