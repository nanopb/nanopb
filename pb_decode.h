#ifndef _PB_DECODE_H_
#define _PB_DECODE_H_

/* pb_decode.h: Functions to decode protocol buffers. Depends on pb_decode.c.
 * The main function is pb_decode. You will also need to create an input
 * stream, which is easiest to do with pb_istream_t.
 * 
 * You also need structures and their corresponding pb_field_t descriptions.
 * These are usually generated from .proto-files with a script.
 */

#include <stdbool.h>
#include "pb.h"

/* Lightweight input stream.
 * You can provide a callback function for reading or use
 * pb_istream_from_buffer.
 * 
 * Rules for callback:
 * 1) Return false on IO errors. This will cause decoding to abort.
 * 
 * 2) If buf is NULL, read but don't store bytes ("skip input").
 * 
 * 3) You can use state to store your own data (e.g. buffer pointer),
 * and rely on pb_read to verify that no-body reads past bytes_left.
 * 
 * 4) Your callback may be used with substreams, in which case bytes_left
 * is different than from the main stream. Don't use bytes_left to compute
 * any pointers.
 */
struct _pb_istream_t
{
    bool (*callback)(pb_istream_t *stream, uint8_t *buf, size_t count);
    void *state; /* Free field for use by callback implementation */
    size_t bytes_left;
};

pb_istream_t pb_istream_from_buffer(uint8_t *buf, size_t bufsize);
bool pb_read(pb_istream_t *stream, uint8_t *buf, size_t count);

/* Decode from stream to destination struct.
 * Returns true on success, false on any failure.
 * The actual struct pointed to by dest must match the description in fields.
 */
bool pb_decode(pb_istream_t *stream, const pb_field_t fields[], void *dest_struct);

/* --- Helper functions ---
 * You may want to use these from your caller or callbacks.
 */

bool pb_decode_tag(pb_istream_t *stream, pb_wire_type_t *wire_type, uint32_t *tag, bool *eof);
bool pb_skip_field(pb_istream_t *stream, pb_wire_type_t wire_type);

bool pb_decode_varint(pb_istream_t *stream, uint64_t *dest);

bool pb_skip_varint(pb_istream_t *stream);
bool pb_skip_string(pb_istream_t *stream);

/* --- Field decoders ---
 * Each decoder takes stream and field description, and a pointer to the field
 * in the destination struct (dest = struct_addr + field->data_offset).
 * For arrays, these functions are called repeatedly.
 */

bool pb_dec_varint(pb_istream_t *stream, const pb_field_t *field, void *dest);
bool pb_dec_svarint(pb_istream_t *stream, const pb_field_t *field, void *dest);
bool pb_dec_fixed32(pb_istream_t *stream, const pb_field_t *field, void *dest);
bool pb_dec_fixed64(pb_istream_t *stream, const pb_field_t *field, void *dest);

bool pb_dec_bytes(pb_istream_t *stream, const pb_field_t *field, void *dest);
bool pb_dec_string(pb_istream_t *stream, const pb_field_t *field, void *dest);
bool pb_dec_submessage(pb_istream_t *stream, const pb_field_t *field, void *dest);

#endif
