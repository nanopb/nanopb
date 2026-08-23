/* pb_validate.h: Runtime support for nanopb validation
 *
 * This file provides the public API for validating protobuf messages
 * against declarative constraints defined using custom options.
 */

#ifndef PB_VALIDATE_H_INCLUDED
#define PB_VALIDATE_H_INCLUDED

#include "pb.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>  /* For timestamp validation */

#ifdef __cplusplus
extern "C"
{
#endif

/* Configuration options */
#ifndef PB_VALIDATE_MAX_VIOLATIONS
#define PB_VALIDATE_MAX_VIOLATIONS 16
#endif

#ifndef PB_VALIDATE_EARLY_EXIT
#define PB_VALIDATE_EARLY_EXIT 1
#endif

#ifndef PB_VALIDATE_MAX_PATH_LENGTH
#define PB_VALIDATE_MAX_PATH_LENGTH 128
#endif

#ifndef PB_VALIDATE_MAX_MESSAGE_LENGTH
#define PB_VALIDATE_MAX_MESSAGE_LENGTH 256
#endif

    /* Violation structure representing a single validation error */
    typedef struct pb_violation_s
    {
        const char *field_path;    /* Dotted path to the field, e.g., "user.email" */
        const char *constraint_id; /* Constraint identifier, e.g., "string.max_len" */
        const char *message;       /* Human-readable error message */
    } pb_violation_t;

    /* Violations collection structure */
    typedef struct pb_violations_s
    {
        pb_violation_t violations[PB_VALIDATE_MAX_VIOLATIONS];
        pb_size_t count; /* Number of violations recorded */
        bool truncated;  /* True if more violations were found than could be stored */
    } pb_violations_t;

    /* Initialize a violations structure */
    void pb_violations_init(pb_violations_t *violations);

    /* Add a violation to the collection */
    bool pb_violations_add(pb_violations_t *violations,
                           const char *field_path,
                           const char *constraint_id,
                           const char *message);

    /* Get the number of violations */
    static inline pb_size_t pb_violations_count(const pb_violations_t *violations)
    {
        return violations ? violations->count : 0;
    }

    /* Check if there are any violations */
    static inline bool pb_violations_has_any(const pb_violations_t *violations)
    {
        return pb_violations_count(violations) > 0;
    }

    /* Validation context structure (internal use) */
    typedef struct pb_validate_context_s
    {
        pb_violations_t *violations;
        char path_buffer[PB_VALIDATE_MAX_PATH_LENGTH];
        pb_size_t path_length;
        bool early_exit;
    } pb_validate_context_t;

        /* Convenience macros for generated validators
         *
         * These allow the code generator to emit compact, macro-based
         * validation code instead of repeating the same boilerplate in
         * every generated *_validate.c file, similar in spirit to the
         * PB_BIND macros used for field descriptors.
         */

    /* Begin / end of a validate function body.
     * Usage:
     *   bool pb_validate_Foo(const Foo *msg, pb_violations_t *violations)
     *   {
     *       PB_VALIDATE_BEGIN(ctx, Foo, msg, violations);
     *       ... field checks ...
     *       PB_VALIDATE_END(ctx, violations);
     *   }
     */
    #define PB_VALIDATE_BEGIN(ctx_var, MsgType, msg_ptr, violations_ptr) \
        pb_validate_context_t ctx_var = {0};                             \
        ctx_var.violations = (violations_ptr);                           \
        ctx_var.early_exit = PB_VALIDATE_EARLY_EXIT;                     \
        if (!(msg_ptr)) return false

    #define PB_VALIDATE_END(ctx_var, violations_ptr) \
        (void)(ctx_var);                                \
        return !pb_violations_has_any(violations_ptr)

    /* Per-field context helpers. These wrap push/pop of the field name
     * into the validation context path buffer.
     */
    #define PB_VALIDATE_FIELD_BEGIN(ctx_var, field_name_str)                 \
        if (!pb_validate_context_push_field(&(ctx_var), (field_name_str)))   \
            return false

    #define PB_VALIDATE_FIELD_END(ctx_var) \
        pb_validate_context_pop_field(&(ctx_var))

    /* Helper for guarding checks behind has_XXX flag for OPTIONAL fields.
     * The generator expands FIELD_RULES_ENUM to the field.rules symbol when
     * it knows the field is optional, so that checks are skipped when the
     * field is unset.
     */
    #define PB_VALIDATE_IF_OPTIONAL(has_flag_expr, CODE) do { \
            if (has_flag_expr) {                              \
                CODE                                          \
            }                                                 \
        } while (0)

    /* Generic numeric comparison helper. The generator supplies the
     * concrete C type, validator function and rule enum.
     */
    #define PB_VALIDATE_NUMERIC_GENERIC(ctx_var, msg_ptr, field_name, CTYPE, FUNC, RULE_ENUM, VALUE_EXPR, CONSTRAINT_ID) \
        do {                                                                                                            \
            CTYPE __pb_expected = (CTYPE)(VALUE_EXPR);                                                                  \
            if (!FUNC((msg_ptr)->field_name, &__pb_expected, (RULE_ENUM))) {                                            \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "Value constraint failed"); \
                if ((ctx_var).early_exit) return false;                                                                 \
            }                                                                                                           \
        } while (0)

    /* String length helpers for normal (non-callback) string fields. */
    #define PB_VALIDATE_STR_MIN_LEN(ctx_var, msg_ptr, field_name, MIN_LEN, CONSTRAINT_ID)                      \
        do {                                                                                                   \
            uint32_t __pb_min_len_v = (uint32_t)(MIN_LEN);                                                     \
            if (!pb_validate_string((msg_ptr)->field_name, (pb_size_t)strlen((msg_ptr)->field_name),          \
                                    &__pb_min_len_v, PB_VALIDATE_RULE_MIN_LEN)) {                              \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "String too short"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_STR_MAX_LEN(ctx_var, msg_ptr, field_name, MAX_LEN, CONSTRAINT_ID)                      \
        do {                                                                                                   \
            uint32_t __pb_max_len_v = (uint32_t)(MAX_LEN);                                                     \
            if (!pb_validate_string((msg_ptr)->field_name, (pb_size_t)strlen((msg_ptr)->field_name),          \
                                    &__pb_max_len_v, PB_VALIDATE_RULE_MAX_LEN)) {                              \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "String too long"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    /* String pattern helpers for normal (non-callback) string fields. */
    #define PB_VALIDATE_STR_PREFIX(ctx_var, msg_ptr, field_name, PREFIX_STR, CONSTRAINT_ID)                     \
        do {                                                                                                    \
            const char *__pb_prefix = (PREFIX_STR);                                                             \
            if (!pb_validate_string((msg_ptr)->field_name, (pb_size_t)strlen((msg_ptr)->field_name),           \
                                    __pb_prefix, PB_VALIDATE_RULE_PREFIX)) {                                   \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),                 \
                                  "String must start with specified prefix");                                 \
                if ((ctx_var).early_exit) return false;                                                         \
            }                                                                                                   \
        } while (0)

    #define PB_VALIDATE_STR_SUFFIX(ctx_var, msg_ptr, field_name, SUFFIX_STR, CONSTRAINT_ID)                     \
        do {                                                                                                    \
            const char *__pb_suffix = (SUFFIX_STR);                                                             \
            if (!pb_validate_string((msg_ptr)->field_name, (pb_size_t)strlen((msg_ptr)->field_name),           \
                                    __pb_suffix, PB_VALIDATE_RULE_SUFFIX)) {                                   \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),                 \
                                  "String must end with specified suffix");                                   \
                if ((ctx_var).early_exit) return false;                                                         \
            }                                                                                                   \
        } while (0)

    #define PB_VALIDATE_STR_CONTAINS(ctx_var, msg_ptr, field_name, NEEDLE_STR, CONSTRAINT_ID)                   \
        do {                                                                                                    \
            const char *__pb_needle = (NEEDLE_STR);                                                             \
            if (!pb_validate_string((msg_ptr)->field_name, (pb_size_t)strlen((msg_ptr)->field_name),           \
                                    __pb_needle, PB_VALIDATE_RULE_CONTAINS)) {                                 \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),                 \
                                  "String must contain specified substring");                                  \
                if ((ctx_var).early_exit) return false;                                                         \
            }                                                                                                   \
        } while (0)

    /* String format validation helpers for normal (non-callback) string fields.
     * These macros validate email, hostname, IP address, and ASCII format.
     */
    #define PB_VALIDATE_STR_FORMAT(ctx_var, msg_ptr, field_name, RULE_ENUM, CONSTRAINT_ID, ERR_MSG)              \
        do {                                                                                                    \
            if (!pb_validate_string((msg_ptr)->field_name, (pb_size_t)strlen((msg_ptr)->field_name),           \
                                    NULL, (RULE_ENUM))) {                                                       \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), (ERR_MSG));     \
                if ((ctx_var).early_exit) return false;                                                         \
            }                                                                                                   \
        } while (0)

    #define PB_VALIDATE_STR_ASCII(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STR_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_ASCII, CONSTRAINT_ID, "String must contain only ASCII characters")

    #define PB_VALIDATE_STR_EMAIL(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STR_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_EMAIL, CONSTRAINT_ID, "String format validation failed")

    #define PB_VALIDATE_STR_HOSTNAME(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STR_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_HOSTNAME, CONSTRAINT_ID, "String format validation failed")

    #define PB_VALIDATE_STR_IP(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STR_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_IP, CONSTRAINT_ID, "String format validation failed")

    #define PB_VALIDATE_STR_IPV4(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STR_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_IPV4, CONSTRAINT_ID, "String format validation failed")

    #define PB_VALIDATE_STR_IPV6(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STR_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_IPV6, CONSTRAINT_ID, "String format validation failed")

    /* Generic string validation macro for format rules (email, hostname, ip, etc.)
     * that don't require a parameter value. For callback-based string fields.
     */
    #define PB_VALIDATE_STRING_CALLBACK_FORMAT(ctx_var, msg_ptr, field_name, RULE_ENUM, CONSTRAINT_ID, ERR_MSG) \
        do {                                                                                                    \
            const char *__pb_s = NULL; pb_size_t __pb_l = 0;                                                    \
            if (pb_read_callback_string(&(msg_ptr)->field_name, &__pb_s, &__pb_l)) {                            \
                if (!pb_validate_string(__pb_s, __pb_l, NULL, (RULE_ENUM))) {                                   \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), (ERR_MSG)); \
                    if ((ctx_var).early_exit) return false;                                                     \
                }                                                                                               \
            }                                                                                                   \
        } while (0)

    /* Generic string validation macro for length rules (min_len, max_len).
     * For callback-based string fields.
     */
    #define PB_VALIDATE_STRING_CALLBACK_LEN(ctx_var, msg_ptr, field_name, RULE_ENUM, LEN_VALUE, CONSTRAINT_ID, ERR_MSG) \
        do {                                                                                                    \
            const char *__pb_s = NULL; pb_size_t __pb_l = 0;                                                    \
            if (pb_read_callback_string(&(msg_ptr)->field_name, &__pb_s, &__pb_l)) {                            \
                uint32_t __pb_len_v = (uint32_t)(LEN_VALUE);                                                    \
                if (!pb_validate_string(__pb_s, __pb_l, &__pb_len_v, (RULE_ENUM))) {                            \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), (ERR_MSG)); \
                    if ((ctx_var).early_exit) return false;                                                     \
                }                                                                                               \
            }                                                                                                   \
        } while (0)

    /* Generic string validation macro for pattern rules (prefix, suffix, contains).
     * For callback-based string fields.
     */
    #define PB_VALIDATE_STRING_CALLBACK_PATTERN(ctx_var, msg_ptr, field_name, RULE_ENUM, PATTERN_STR, CONSTRAINT_ID, ERR_MSG) \
        do {                                                                                                    \
            const char *__pb_s = NULL; pb_size_t __pb_l = 0;                                                    \
            if (pb_read_callback_string(&(msg_ptr)->field_name, &__pb_s, &__pb_l)) {                            \
                const char *__pb_pattern = (PATTERN_STR);                                                       \
                if (!pb_validate_string(__pb_s, __pb_l, __pb_pattern, (RULE_ENUM))) {                           \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), (ERR_MSG)); \
                    if ((ctx_var).early_exit) return false;                                                     \
                }                                                                                               \
            }                                                                                                   \
        } while (0)

    /* Unified generic string macro for any rule type with optional parameter.
     * RULE_DATA_EXPR should be a pointer to rule data or NULL for format rules.
     * For callback-based string fields.
     */
    #define PB_VALIDATE_STRING_GENERIC(ctx_var, msg_ptr, field_name, RULE_ENUM, RULE_DATA_EXPR, CONSTRAINT_ID, ERR_MSG) \
        do {                                                                                                    \
            const char *__pb_s = NULL; pb_size_t __pb_l = 0;                                                    \
            if (pb_read_callback_string(&(msg_ptr)->field_name, &__pb_s, &__pb_l)) {                            \
                if (!pb_validate_string(__pb_s, __pb_l, (RULE_DATA_EXPR), (RULE_ENUM))) {                       \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), (ERR_MSG)); \
                    if ((ctx_var).early_exit) return false;                                                     \
                }                                                                                               \
            }                                                                                                   \
        } while (0)

    /* Format validation macros for callback-based string fields */
    #define PB_VALIDATE_STRING_EMAIL(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_EMAIL, CONSTRAINT_ID, "String format validation failed")

    #define PB_VALIDATE_STRING_HOSTNAME(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_HOSTNAME, CONSTRAINT_ID, "String format validation failed")

    #define PB_VALIDATE_STRING_IP(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_IP, CONSTRAINT_ID, "String format validation failed")

    #define PB_VALIDATE_STRING_IPV4(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_IPV4, CONSTRAINT_ID, "String format validation failed")

    #define PB_VALIDATE_STRING_IPV6(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_IPV6, CONSTRAINT_ID, "String format validation failed")

    #define PB_VALIDATE_STRING_ASCII(ctx_var, msg_ptr, field_name, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_FORMAT(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_ASCII, CONSTRAINT_ID, "String must contain only ASCII characters")

    /* Length validation macros for callback-based string fields */
    #define PB_VALIDATE_STRING_MIN_LEN(ctx_var, msg_ptr, field_name, MIN_LEN, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_LEN(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_MIN_LEN, MIN_LEN, CONSTRAINT_ID, "String too short")

    #define PB_VALIDATE_STRING_MAX_LEN(ctx_var, msg_ptr, field_name, MAX_LEN, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_LEN(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_MAX_LEN, MAX_LEN, CONSTRAINT_ID, "String too long")

    /* Pattern validation macros for callback-based string fields */
    #define PB_VALIDATE_STRING_PREFIX(ctx_var, msg_ptr, field_name, PREFIX_STR, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_PATTERN(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_PREFIX, PREFIX_STR, CONSTRAINT_ID, "String must start with specified prefix")

    #define PB_VALIDATE_STRING_SUFFIX(ctx_var, msg_ptr, field_name, SUFFIX_STR, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_PATTERN(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_SUFFIX, SUFFIX_STR, CONSTRAINT_ID, "String must end with specified suffix")

    #define PB_VALIDATE_STRING_CONTAINS(ctx_var, msg_ptr, field_name, NEEDLE_STR, CONSTRAINT_ID) \
        PB_VALIDATE_STRING_CALLBACK_PATTERN(ctx_var, msg_ptr, field_name, PB_VALIDATE_RULE_CONTAINS, NEEDLE_STR, CONSTRAINT_ID, "String must contain specified substring")

    /* Repeated field size helpers working on *_count naming convention. */
    #define PB_VALIDATE_MIN_ITEMS(ctx_var, msg_ptr, field_name, MIN_ITEMS, CONSTRAINT_ID)                      \
        do {                                                                                                   \
            if (!pb_validate_min_items((msg_ptr)->field_name##_count, (MIN_ITEMS))) {                          \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "Too few items"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_MAX_ITEMS(ctx_var, msg_ptr, field_name, MAX_ITEMS, CONSTRAINT_ID)                      \
        do {                                                                                                   \
            if (!pb_validate_max_items((msg_ptr)->field_name##_count, (MAX_ITEMS))) {                          \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "Too many items"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    /* Repeated unique validation for scalar types (direct comparison). */
    #define PB_VALIDATE_REPEATED_UNIQUE_SCALAR(ctx_var, msg_ptr, field_name, CONSTRAINT_ID)                     \
        do {                                                                                                   \
            for (pb_size_t __pb_i = 0; __pb_i < (msg_ptr)->field_name##_count; ++__pb_i) {                     \
                for (pb_size_t __pb_j = __pb_i + 1; __pb_j < (msg_ptr)->field_name##_count; ++__pb_j) {        \
                    if ((msg_ptr)->field_name[__pb_i] == (msg_ptr)->field_name[__pb_j]) {                      \
                        pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),        \
                                          "Repeated field elements must be unique");                           \
                        if ((ctx_var).early_exit) return false;                                                \
                    }                                                                                          \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    /* Repeated unique validation for string types (strcmp comparison). */
    #define PB_VALIDATE_REPEATED_UNIQUE_STRING(ctx_var, msg_ptr, field_name, CONSTRAINT_ID)                     \
        do {                                                                                                   \
            for (pb_size_t __pb_i = 0; __pb_i < (msg_ptr)->field_name##_count; ++__pb_i) {                     \
                for (pb_size_t __pb_j = __pb_i + 1; __pb_j < (msg_ptr)->field_name##_count; ++__pb_j) {        \
                    if (strcmp((msg_ptr)->field_name[__pb_i], (msg_ptr)->field_name[__pb_j]) == 0) {           \
                        pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),        \
                                          "Repeated field elements must be unique");                           \
                        if ((ctx_var).early_exit) return false;                                                \
                    }                                                                                          \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    /* Repeated unique validation for bytes types (memcmp comparison). */
    #define PB_VALIDATE_REPEATED_UNIQUE_BYTES(ctx_var, msg_ptr, field_name, CONSTRAINT_ID)                      \
        do {                                                                                                   \
            for (pb_size_t __pb_i = 0; __pb_i < (msg_ptr)->field_name##_count; ++__pb_i) {                     \
                for (pb_size_t __pb_j = __pb_i + 1; __pb_j < (msg_ptr)->field_name##_count; ++__pb_j) {        \
                    if ((msg_ptr)->field_name[__pb_i].size == (msg_ptr)->field_name[__pb_j].size &&            \
                        memcmp((msg_ptr)->field_name[__pb_i].bytes, (msg_ptr)->field_name[__pb_j].bytes,       \
                               (msg_ptr)->field_name[__pb_i].size) == 0) {                                    \
                        pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),        \
                                          "Repeated field elements must be unique");                           \
                        if ((ctx_var).early_exit) return false;                                                \
                    }                                                                                          \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    /* Repeated items string length validation macros. */
    #define PB_VALIDATE_REPEATED_ITEMS_STR_MIN_LEN(ctx_var, msg_ptr, field_name, MIN_LEN, CONSTRAINT_ID)        \
        do {                                                                                                   \
            for (pb_size_t __pb_i = 0; __pb_i < (msg_ptr)->field_name##_count; ++__pb_i) {                     \
                pb_validate_context_push_index(&(ctx_var), __pb_i);                                            \
                uint32_t __pb_min_len_v = (uint32_t)(MIN_LEN);                                                 \
                if (!pb_validate_string((msg_ptr)->field_name[__pb_i],                                         \
                                        (pb_size_t)strlen((msg_ptr)->field_name[__pb_i]),                      \
                                        &__pb_min_len_v, PB_VALIDATE_RULE_MIN_LEN)) {                          \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),            \
                                      "String too short");                                                     \
                    if ((ctx_var).early_exit) { pb_validate_context_pop_index(&(ctx_var)); return false; }     \
                }                                                                                              \
                pb_validate_context_pop_index(&(ctx_var));                                                     \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_REPEATED_ITEMS_STR_MAX_LEN(ctx_var, msg_ptr, field_name, MAX_LEN, CONSTRAINT_ID)        \
        do {                                                                                                   \
            for (pb_size_t __pb_i = 0; __pb_i < (msg_ptr)->field_name##_count; ++__pb_i) {                     \
                pb_validate_context_push_index(&(ctx_var), __pb_i);                                            \
                uint32_t __pb_max_len_v = (uint32_t)(MAX_LEN);                                                 \
                if (!pb_validate_string((msg_ptr)->field_name[__pb_i],                                         \
                                        (pb_size_t)strlen((msg_ptr)->field_name[__pb_i]),                      \
                                        &__pb_max_len_v, PB_VALIDATE_RULE_MAX_LEN)) {                          \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),            \
                                      "String too long");                                                      \
                    if ((ctx_var).early_exit) { pb_validate_context_pop_index(&(ctx_var)); return false; }     \
                }                                                                                              \
                pb_validate_context_pop_index(&(ctx_var));                                                     \
            }                                                                                                  \
        } while (0)

    /* Repeated items numeric validation macro. */
    #define PB_VALIDATE_REPEATED_ITEMS_NUMERIC(ctx_var, msg_ptr, field_name, CTYPE, FUNC, RULE_ENUM, VALUE_EXPR, CONSTRAINT_ID) \
        do {                                                                                                   \
            for (pb_size_t __pb_i = 0; __pb_i < (msg_ptr)->field_name##_count; ++__pb_i) {                     \
                pb_validate_context_push_index(&(ctx_var), __pb_i);                                            \
                CTYPE __pb_expected = (CTYPE)(VALUE_EXPR);                                                     \
                if (!FUNC((msg_ptr)->field_name[__pb_i], &__pb_expected, (RULE_ENUM))) {                       \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),            \
                                      "Value constraint failed");                                              \
                    if ((ctx_var).early_exit) { pb_validate_context_pop_index(&(ctx_var)); return false; }     \
                }                                                                                              \
                pb_validate_context_pop_index(&(ctx_var));                                                     \
            }                                                                                                  \
        } while (0)

    /* Bytes length validation macros. */
    #define PB_VALIDATE_BYTES_MIN_LEN(ctx_var, msg_ptr, field_name, MIN_LEN, CONSTRAINT_ID)                     \
        do {                                                                                                   \
            if ((msg_ptr)->field_name.size < (MIN_LEN)) {                                                      \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "Bytes too short"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_BYTES_MAX_LEN(ctx_var, msg_ptr, field_name, MAX_LEN, CONSTRAINT_ID)                     \
        do {                                                                                                   \
            if ((msg_ptr)->field_name.size > (MAX_LEN)) {                                                      \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "Bytes too long"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    /* Enum defined_only validation macro.
     * values_arr must be an array of valid enum values.
     */
    #define PB_VALIDATE_ENUM_DEFINED_ONLY(ctx_var, msg_ptr, field_name, values_arr, CONSTRAINT_ID)              \
        do {                                                                                                   \
            if (!pb_validate_enum_defined_only((int)(msg_ptr)->field_name, (values_arr),                       \
                    (pb_size_t)(sizeof(values_arr)/sizeof((values_arr)[0])))) {                                \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),                \
                                  "Value must be a defined enum value");                                       \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    /* Nested message validation macros. */
    #define PB_VALIDATE_NESTED_MSG(ctx_var, validate_func, msg_ptr, field_name, violations_ptr)                 \
        do {                                                                                                   \
            bool __pb_ok_nested = validate_func(&(msg_ptr)->field_name, (violations_ptr));                     \
            if (!__pb_ok_nested && (ctx_var).early_exit) {                                                     \
                pb_validate_context_pop_field(&(ctx_var));                                                     \
                return false;                                                                                  \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_NESTED_MSG_OPTIONAL(ctx_var, validate_func, msg_ptr, field_name, violations_ptr)        \
        do {                                                                                                   \
            if ((msg_ptr)->has_##field_name) {                                                                 \
                bool __pb_ok_nested = validate_func(&(msg_ptr)->field_name, (violations_ptr));                 \
                if (!__pb_ok_nested && (ctx_var).early_exit) {                                                 \
                    pb_validate_context_pop_field(&(ctx_var));                                                 \
                    return false;                                                                              \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_NESTED_MSG_POINTER(ctx_var, validate_func, msg_ptr, field_name, violations_ptr)         \
        do {                                                                                                   \
            if ((msg_ptr)->field_name) {                                                                       \
                bool __pb_ok_nested = validate_func((msg_ptr)->field_name, (violations_ptr));                  \
                if (!__pb_ok_nested && (ctx_var).early_exit) {                                                 \
                    pb_validate_context_pop_field(&(ctx_var));                                                 \
                    return false;                                                                              \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    /* Oneof member validation macros.
     * These operate on oneof member fields accessed via msg->oneof_name.field_name
     */
    #define PB_VALIDATE_ONEOF_NUMERIC(ctx_var, msg_ptr, oneof_name, field_name, CTYPE, FUNC, RULE_ENUM, VALUE_EXPR, CONSTRAINT_ID) \
        do {                                                                                                   \
            CTYPE __pb_expected = (CTYPE)(VALUE_EXPR);                                                         \
            if (!FUNC((msg_ptr)->oneof_name.field_name, &__pb_expected, (RULE_ENUM))) {                        \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),                \
                                  "Value constraint failed");                                                  \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_ONEOF_STR_MIN_LEN(ctx_var, msg_ptr, oneof_name, field_name, MIN_LEN, CONSTRAINT_ID)     \
        do {                                                                                                   \
            uint32_t __pb_min_len = (MIN_LEN);                                                                 \
            if (!pb_validate_string((msg_ptr)->oneof_name.field_name,                                          \
                    (pb_size_t)strlen((msg_ptr)->oneof_name.field_name),                                       \
                    &__pb_min_len, PB_VALIDATE_RULE_MIN_LEN)) {                                                \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "String too short"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_ONEOF_STR_MAX_LEN(ctx_var, msg_ptr, oneof_name, field_name, MAX_LEN, CONSTRAINT_ID)     \
        do {                                                                                                   \
            uint32_t __pb_max_len = (MAX_LEN);                                                                 \
            if (!pb_validate_string((msg_ptr)->oneof_name.field_name,                                          \
                    (pb_size_t)strlen((msg_ptr)->oneof_name.field_name),                                       \
                    &__pb_max_len, PB_VALIDATE_RULE_MAX_LEN)) {                                                \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "String too long"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_ONEOF_STR_PREFIX(ctx_var, msg_ptr, oneof_name, field_name, PREFIX_STR, CONSTRAINT_ID)   \
        do {                                                                                                   \
            const char *__pb_prefix = (PREFIX_STR);                                                            \
            if (!pb_validate_string((msg_ptr)->oneof_name.field_name,                                          \
                    (pb_size_t)strlen((msg_ptr)->oneof_name.field_name),                                       \
                    __pb_prefix, PB_VALIDATE_RULE_PREFIX)) {                                                   \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),                \
                                  "String must start with specified prefix");                                  \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_ONEOF_STR_SUFFIX(ctx_var, msg_ptr, oneof_name, field_name, SUFFIX_STR, CONSTRAINT_ID)   \
        do {                                                                                                   \
            const char *__pb_suffix = (SUFFIX_STR);                                                            \
            if (!pb_validate_string((msg_ptr)->oneof_name.field_name,                                          \
                    (pb_size_t)strlen((msg_ptr)->oneof_name.field_name),                                       \
                    __pb_suffix, PB_VALIDATE_RULE_SUFFIX)) {                                                   \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),                \
                                  "String must end with specified suffix");                                    \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_ONEOF_STR_CONTAINS(ctx_var, msg_ptr, oneof_name, field_name, NEEDLE_STR, CONSTRAINT_ID) \
        do {                                                                                                   \
            const char *__pb_needle = (NEEDLE_STR);                                                            \
            if (!pb_validate_string((msg_ptr)->oneof_name.field_name,                                          \
                    (pb_size_t)strlen((msg_ptr)->oneof_name.field_name),                                       \
                    __pb_needle, PB_VALIDATE_RULE_CONTAINS)) {                                                 \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),                \
                                  "String must contain specified substring");                                  \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_ONEOF_STR_FORMAT(ctx_var, msg_ptr, oneof_name, field_name, RULE_ENUM, CONSTRAINT_ID, ERR_MSG) \
        do {                                                                                                   \
            if (!pb_validate_string((msg_ptr)->oneof_name.field_name,                                          \
                    (pb_size_t)strlen((msg_ptr)->oneof_name.field_name),                                       \
                    NULL, (RULE_ENUM))) {                                                                      \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), (ERR_MSG));    \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_ONEOF_BYTES_MIN_LEN(ctx_var, msg_ptr, oneof_name, field_name, MIN_LEN, CONSTRAINT_ID)   \
        do {                                                                                                   \
            if ((msg_ptr)->oneof_name.field_name.size < (MIN_LEN)) {                                           \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "Bytes too short"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_ONEOF_BYTES_MAX_LEN(ctx_var, msg_ptr, oneof_name, field_name, MAX_LEN, CONSTRAINT_ID)   \
        do {                                                                                                   \
            if ((msg_ptr)->oneof_name.field_name.size > (MAX_LEN)) {                                           \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID), "Bytes too long"); \
                if ((ctx_var).early_exit) return false;                                                        \
            }                                                                                                  \
        } while (0)

    /* Any field validation macros - these check type_url against allowed/disallowed lists.
     * Note: type_urls is an array of const char*, count is the number of entries.
     */
    #define PB_VALIDATE_ANY_IN(ctx_var, msg_ptr, field_name, type_urls, count, CONSTRAINT_ID)                   \
        do {                                                                                                   \
            if ((msg_ptr)->has_##field_name) {                                                                 \
                const char *__pb_type_url = (const char *)(msg_ptr)->field_name.type_url;                      \
                bool __pb_valid = false;                                                                       \
                for (size_t __pb_i = 0; __pb_i < (count); ++__pb_i) {                                          \
                    if (__pb_type_url && strcmp(__pb_type_url, (type_urls)[__pb_i]) == 0) {                    \
                        __pb_valid = true;                                                                     \
                        break;                                                                                 \
                    }                                                                                          \
                }                                                                                              \
                if (!__pb_valid) {                                                                             \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),            \
                                      "type_url not in allowed list");                                         \
                    if ((ctx_var).early_exit) return false;                                                    \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_ANY_NOT_IN(ctx_var, msg_ptr, field_name, type_urls, count, CONSTRAINT_ID)               \
        do {                                                                                                   \
            if ((msg_ptr)->has_##field_name) {                                                                 \
                const char *__pb_type_url = (const char *)(msg_ptr)->field_name.type_url;                      \
                for (size_t __pb_i = 0; __pb_i < (count); ++__pb_i) {                                          \
                    if (__pb_type_url && strcmp(__pb_type_url, (type_urls)[__pb_i]) == 0) {                    \
                        pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),        \
                                          "type_url in disallowed list");                                      \
                        if ((ctx_var).early_exit) return false;                                                \
                        break;                                                                                 \
                    }                                                                                          \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    /* Timestamp field validation macros - these check google.protobuf.Timestamp against current time.
     * Note: These require <time.h> and may not work on all embedded platforms.
     */
    #define PB_VALIDATE_TIMESTAMP_GT_NOW(ctx_var, msg_ptr, field_name, CONSTRAINT_ID)                         \
        do {                                                                                                   \
            if ((msg_ptr)->has_##field_name) {                                                                 \
                time_t __pb_now = time(NULL);                                                                  \
                int64_t __pb_now_seconds = (int64_t)__pb_now;                                                  \
                if ((msg_ptr)->field_name.seconds <= __pb_now_seconds) {                                       \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),            \
                                      "timestamp must be after current time");                                 \
                    if ((ctx_var).early_exit) return false;                                                    \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_TIMESTAMP_LT_NOW(ctx_var, msg_ptr, field_name, CONSTRAINT_ID)                         \
        do {                                                                                                   \
            if ((msg_ptr)->has_##field_name) {                                                                 \
                time_t __pb_now = time(NULL);                                                                  \
                int64_t __pb_now_seconds = (int64_t)__pb_now;                                                  \
                if ((msg_ptr)->field_name.seconds >= __pb_now_seconds) {                                       \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),            \
                                      "timestamp must be before current time");                                \
                    if ((ctx_var).early_exit) return false;                                                    \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    #define PB_VALIDATE_TIMESTAMP_WITHIN(ctx_var, msg_ptr, field_name, seconds_val, CONSTRAINT_ID)            \
        do {                                                                                                   \
            if ((msg_ptr)->has_##field_name) {                                                                 \
                time_t __pb_now = time(NULL);                                                                  \
                int64_t __pb_now_seconds = (int64_t)__pb_now;                                                  \
                int64_t __pb_diff = (msg_ptr)->field_name.seconds - __pb_now_seconds;                          \
                if (__pb_diff < 0) __pb_diff = -__pb_diff;                                                     \
                if (__pb_diff > (seconds_val)) {                                                               \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),            \
                                      "timestamp not within specified duration from now");                     \
                    if ((ctx_var).early_exit) return false;                                                    \
                }                                                                                              \
            }                                                                                                  \
        } while (0)

    /* String in/not_in validation macros.
     * These check if a string value is in or not in a list of allowed/forbidden values.
     */
    #define PB_VALIDATE_STR_IN(ctx_var, msg_ptr, field_name, values_arr, count, CONSTRAINT_ID)                   \
        do {                                                                                                    \
            bool __pb_valid = false;                                                                            \
            for (size_t __pb_i = 0; __pb_i < (count); ++__pb_i) {                                               \
                if (strcmp((msg_ptr)->field_name, (values_arr)[__pb_i]) == 0) {                                 \
                    __pb_valid = true;                                                                          \
                    break;                                                                                      \
                }                                                                                               \
            }                                                                                                   \
            if (!__pb_valid) {                                                                                  \
                pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),                 \
                                  "Value must be one of allowed set");                                          \
                if ((ctx_var).early_exit) return false;                                                         \
            }                                                                                                   \
        } while (0)

    #define PB_VALIDATE_STR_NOT_IN(ctx_var, msg_ptr, field_name, values_arr, count, CONSTRAINT_ID)               \
        do {                                                                                                    \
            for (size_t __pb_i = 0; __pb_i < (count); ++__pb_i) {                                               \
                if (strcmp((msg_ptr)->field_name, (values_arr)[__pb_i]) == 0) {                                 \
                    pb_violations_add((ctx_var).violations, (ctx_var).path_buffer, (CONSTRAINT_ID),             \
                                      "Value must not be one of forbidden set");                                \
                    if ((ctx_var).early_exit) return false;                                                     \
                    break;                                                                                      \
                }                                                                                               \
            }                                                                                                   \
        } while (0)

    /* Oneof switch/case macros for cleaner generated code. */
    #define PB_VALIDATE_ONEOF_BEGIN(ctx_var, msg_ptr, oneof_name)                                                \
        switch ((msg_ptr)->which_##oneof_name) {

    #define PB_VALIDATE_ONEOF_CASE(tag_name)                                                                     \
        case tag_name:

    #define PB_VALIDATE_ONEOF_CASE_END()                                                                         \
            break;

    #define PB_VALIDATE_ONEOF_DEFAULT()                                                                          \
        default:                                                                                                \
            break;

    #define PB_VALIDATE_ONEOF_END()                                                                              \
        }

    /* Rule types for internal use */
    typedef enum
    {
        PB_VALIDATE_RULE_REQUIRED,
        PB_VALIDATE_RULE_LT,
        PB_VALIDATE_RULE_LTE,
        PB_VALIDATE_RULE_GT,
        PB_VALIDATE_RULE_GTE,
        PB_VALIDATE_RULE_EQ,
        PB_VALIDATE_RULE_IN,
        PB_VALIDATE_RULE_NOT_IN,
        PB_VALIDATE_RULE_MIN_LEN,
        PB_VALIDATE_RULE_MAX_LEN,
        PB_VALIDATE_RULE_ASCII,
        PB_VALIDATE_RULE_PREFIX,
        PB_VALIDATE_RULE_SUFFIX,
        PB_VALIDATE_RULE_CONTAINS,
        PB_VALIDATE_RULE_EMAIL,
        PB_VALIDATE_RULE_HOSTNAME,
        PB_VALIDATE_RULE_IP,
        PB_VALIDATE_RULE_IPV4,
        PB_VALIDATE_RULE_IPV6,
        PB_VALIDATE_RULE_MIN_ITEMS,
        PB_VALIDATE_RULE_MAX_ITEMS,
        PB_VALIDATE_RULE_UNIQUE,
        PB_VALIDATE_RULE_ENUM_DEFINED,
        PB_VALIDATE_RULE_ONEOF_REQUIRED,
        PB_VALIDATE_RULE_REQUIRES,
        PB_VALIDATE_RULE_MUTEX,
        PB_VALIDATE_RULE_AT_LEAST
    } pb_validate_rule_type_t;

    /* Validation functions for primitive types */
    bool pb_validate_int32(int32_t value, const void *rule_data, pb_validate_rule_type_t rule_type);
    bool pb_validate_int64(int64_t value, const void *rule_data, pb_validate_rule_type_t rule_type);
    bool pb_validate_uint32(uint32_t value, const void *rule_data, pb_validate_rule_type_t rule_type);
    bool pb_validate_uint64(uint64_t value, const void *rule_data, pb_validate_rule_type_t rule_type);
    bool pb_validate_float(float value, const void *rule_data, pb_validate_rule_type_t rule_type);
    bool pb_validate_double(double value, const void *rule_data, pb_validate_rule_type_t rule_type);
    bool pb_validate_bool(bool value, const void *rule_data, pb_validate_rule_type_t rule_type);
    bool pb_validate_string(const char *value, pb_size_t length, const void *rule_data, pb_validate_rule_type_t rule_type);
    bool pb_validate_bytes(const pb_bytes_array_t *value, const void *rule_data, pb_validate_rule_type_t rule_type);
    bool pb_validate_enum(int value, const void *rule_data, pb_validate_rule_type_t rule_type);

    /* Convenience helper for enum defined_only validation.
     * Returns true if 'value' is one of the provided enumeration values.
     */
    bool pb_validate_enum_defined_only(int value, const int *values, pb_size_t count);

    /* Helper function for validating string/bytes length during decode.
     * This is used by decode callbacks when the full string/bytes data is not stored.
     * For strings: validates length constraints (min_len, max_len).
     * For bytes: validates length constraints (min_len, max_len).
     * Returns true if the length passes validation, false otherwise.
     */
    bool pb_validate_string_length(pb_size_t length, pb_size_t min_len, pb_size_t max_len);
    bool pb_validate_bytes_length(pb_size_t length, pb_size_t min_len, pb_size_t max_len);

    /* Helper to obtain const char* and length from a pb_callback_t string field for validation.
     * Usage model: The application should set the callback's arg pointer to a buffer that
     * contains a NUL-terminated string after decoding. This helper simply exposes that
     * pointer for validation (no copying). Returns true if a valid pointer was obtained.
     * If the callback is NULL or arg is NULL, returns false (treated as empty / missing).
     */
    struct pb_callback_s; /* forward decl from pb.h */
    bool pb_read_callback_string(const struct pb_callback_s *callback, const char **out_str, pb_size_t *out_len);

    /* Helper functions for validation context */
    bool pb_validate_context_push_field(pb_validate_context_t *ctx, const char *field_name);
    void pb_validate_context_pop_field(pb_validate_context_t *ctx);
    bool pb_validate_context_push_index(pb_validate_context_t *ctx, pb_size_t index);
    void pb_validate_context_pop_index(pb_validate_context_t *ctx);

    /* Repeated field count helpers */
    static inline bool pb_validate_min_items(pb_size_t count, pb_size_t min_required)
    {
        return count >= min_required;
    }

    static inline bool pb_validate_max_items(pb_size_t count, pb_size_t max_allowed)
    {
        return count <= max_allowed;
    }

/* Encode/decode hook macros (optional) */
#ifdef PB_VALIDATE_BEFORE_ENCODE
#define pb_validate_encode(stream, msg_type, msg) \
    (pb_validate_##msg_type(msg, NULL) ? pb_encode(stream, msg_type##_fields, msg) : false)
#endif

#ifdef PB_VALIDATE_AFTER_DECODE
#define pb_validate_decode(stream, msg_type, msg) \
    (pb_decode(stream, msg_type##_fields, msg) && pb_validate_##msg_type(msg, NULL))
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PB_VALIDATE_H_INCLUDED */
