#ifndef _PB_DECODE_H_
#define _PB_DECODE_H_

#include <stdbool.h>
#include "pb.h"

/* Lightweight input stream.
 * If buf is NULL, read but don't store bytes.
 * You can to provide a callback function for reading or use
 * pb_istream_from_buffer.
 * 
 * You can use state to store your own data (e.g. buffer pointer),
 * and rely on pb_read to verify that no-body reads past bytes_left.
 * However, substreams may change bytes_left so don't use that to
 * compute any pointers.
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
 * The actual struct pointed to by dest must match the description in fields.
 */
bool pb_decode(pb_istream_t *stream, const pb_field_t fields[], void *dest_struct);

/* --- Helper functions ---
 * You may want to use these from your caller or callbacks.
 */

bool pb_decode_varint32(pb_istream_t *stream, uint32_t *dest);
bool pb_decode_varint64(pb_istream_t *stream, uint64_t *dest);

bool pb_skip_varint(pb_istream_t *stream);
bool pb_skip_string(pb_istream_t *stream);

/* --- Field decoders ---
 * Each decoder takes stream and field description, and a pointer to the field
 * in the destination struct (dest = struct_addr + field->data_offset).
 * For arrays, these functions are called repeatedly.
 */

bool pb_dec_uint32(pb_istream_t *stream, const pb_field_t *field, uint32_t *dest);
bool pb_dec_sint32(pb_istream_t *stream, const pb_field_t *field, int32_t *dest);
bool pb_dec_fixed32(pb_istream_t *stream, const pb_field_t *field, uint32_t *dest);
bool pb_dec_uint64(pb_istream_t *stream, const pb_field_t *field, uint64_t *dest);
bool pb_dec_sint64(pb_istream_t *stream, const pb_field_t *field, int64_t *dest);
bool pb_dec_fixed64(pb_istream_t *stream, const pb_field_t *field, uint64_t *dest);
bool pb_dec_bool(pb_istream_t *stream, const pb_field_t *field, bool *dest);
bool pb_dec_enum(pb_istream_t *stream, const pb_field_t *field, void *dest);

bool pb_dec_float(pb_istream_t *stream, const pb_field_t *field, float *dest);
bool pb_dec_double(pb_istream_t *stream, const pb_field_t *field, double *dest);

bool pb_dec_bytes(pb_istream_t *stream, const pb_field_t *field, uint8_t *dest);
bool pb_dec_string(pb_istream_t *stream, const pb_field_t *field, uint8_t *dest);
bool pb_dec_submessage(pb_istream_t *stream, const pb_field_t *field, void *dest);

typedef bool (*pb_decoder_t)(pb_istream_t *stream, const pb_field_t *field, void *dest);

/* --- Function pointers to field decoders ---
 * Order in the array must match pb_action_t LTYPE numbering.
 */
const pb_decoder_t PB_DECODERS[16];

#endif
