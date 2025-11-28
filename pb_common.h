/* pb_common.h: Common support functions for pb_encode.c and pb_decode.c.
 * These functions are rarely needed by applications directly.
 */

#ifndef PB_COMMON_H_INCLUDED
#define PB_COMMON_H_INCLUDED

#include "pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the field iterator structure to beginning.
 * Returns false if the message type is empty. */
bool pb_field_iter_begin(pb_field_iter_t *iter, const pb_msgdesc_t *desc, void *message);

/* Get a field iterator for extension field. */
bool pb_field_iter_begin_extension(pb_field_iter_t *iter, pb_extension_t *extension);

/* Get pointer to the destination data for an extension field */
void *pb_get_extension_data_ptr(pb_extension_t *extension);

/* Same as pb_field_iter_begin(), but for const message pointer.
 * Note that the pointers in pb_field_iter_t will be non-const but shouldn't
 * be written to when using these functions. */
bool pb_field_iter_begin_const(pb_field_iter_t *iter, const pb_msgdesc_t *desc, const void *message);
bool pb_field_iter_begin_extension_const(pb_field_iter_t *iter, const pb_extension_t *extension);

/* Reset iterator back to beginning.
 * Returns false if the message type is empty.
 */
bool pb_field_iter_reset(pb_field_iter_t *iter);

/* Advance the iterator to the next field.
 * Returns false when the iterator wraps back to the first field. */
bool pb_field_iter_next(pb_field_iter_t *iter);

/* Advance the iterator until it points at a field with the given tag.
 * Returns false if no such field exists. */
bool pb_field_iter_find(pb_field_iter_t *iter, pb_tag_t tag);

/* Find a field with type PB_LTYPE_EXTENSION, or return false if not found.
 * There can be only one extension range field per message. */
bool pb_field_iter_find_extension(pb_field_iter_t *iter);

// Return value type for pb_walk() callback function
typedef enum {
    // Negative values cause pb_walk() to exit.
    // Any value other than PB_WALK_EXIT_OK is regarded as error.
    PB_WALK_EXIT_OK = 0,
    PB_WALK_EXIT_ERR = -1,

    // Move iterator to next field in the message.
    // At the end of message, iter->tag is set to 0.
    PB_WALK_NEXT = 1,

    // Go into submessage defined by iter->pData and iter->submsg_desc
    // Reserves new stack frame with state->next_stacksize bytes
    PB_WALK_IN = 2,

    // Return from the submessage and restore previous stack frame
    PB_WALK_OUT = 3,
} pb_walk_retval_t;

typedef struct pb_walk_state_s pb_walk_state_t;
typedef uint_least16_t pb_walk_stacksize_t;

// Callback function type that is called by pb_walk()
typedef pb_walk_retval_t (*pb_walk_cb_t)(pb_walk_state_t *state);

// This structure stores state associated with pb_walk() operation.
// Pointer to it is passed unchanged to callbacks, so it is allowed
// to extend this structure in user code.
struct pb_walk_state_s {
    // Field iterator for the message at current level.
    // Callback can advance it with pb_field_iter_next() and _find().
    pb_field_iter_t iter;

    // Callback function.
    // This can be modified on the fly.
    pb_walk_cb_t callback;

    // Stack area for current level, size is in bytes.
    // The pointer is aligned to sizeof(void*)
    // Do not modify stacksize value in callback.
    void *stack;
    pb_walk_stacksize_t stacksize;

    // Amount of bytes to reserve for next stack level when
    // callback returns PB_WALK_IN.
    // Can be modified by callback.
    pb_walk_stacksize_t next_stacksize;

    // Free pointer that can be used to store e.g. decode or encode context.
    // Not modified or used by pb_walk().
    void *ctx;

    // Free flags variable that can be used by callbacks.
    // Not modified or used by pb_walk().
    uint32_t flags;

    // Depth in message hierarchy and recursion limit.
    // Do not modify in callback.
    pb_fieldidx_t depth;
    pb_fieldidx_t max_depth;

    // Previous callback return value
    pb_walk_retval_t retval;

#ifndef PB_NO_ERRMSG
    // Pointer to constant (ROM) string used to indicate error cause
    const char *errmsg;
#endif
};

/* Initialize state for pb_walk() function.
 * After this the caller can modify fields in state before calling pb_walk().
 */
bool pb_walk_init(pb_walk_state_t *state, const pb_msgdesc_t *desc, const void *message, pb_walk_cb_t callback);

/* Perform a recursive tree walking operation.
 * Each time the callback is called it returns a direction:
 *
 * PB_WALK_EXIT:     Exit completely from pb_walk().
 * PB_WALK_IN:       Go into submessage.
 * PB_WALK_OUT:      Return out of submessage.
 *
 * For the submessage hierarchy, each level has its own stackframe with adjustable size.
 * This allows precise control over what to preserve, to limit stack usage in recursive
 * operations. Number of bytes available to first level is set by stacksize argument.
 * It can changed for further levels in pb_walk_state_t.
 *
 * The created stack frame is zeroed on PB_WALK_IN command.
 * Stack frames are aligned to sizeof(void*).
 *
 * Callback is allowed to do advance and find operations on the iterator.
 *
 * It is also allowed to PB_WALK_IN on a submessage that is not actually part of the parent
 * message descriptor. This is done by the callback setting iter->submsg_desc and iter->pData
 * before returning. After PB_WALK_OUT from the submessage, iter goes into next item of the
 * original descriptor.
 */
bool pb_walk(pb_walk_state_t *state);

#ifdef PB_VALIDATE_UTF8
/* Validate UTF-8 text string */
bool pb_validate_utf8(const char *s);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

