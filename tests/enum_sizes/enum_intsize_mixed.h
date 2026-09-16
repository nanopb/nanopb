/* Shared by the C and the C++ translation unit of the mixed language test.
 * Both include the same generated header, but pick a different branch of
 * the enum_intsize #if, so this checks that the two branches agree on the
 * layout of the generated structs.
 */

#ifndef ENUM_INTSIZE_MIXED_H
#define ENUM_INTSIZE_MIXED_H

#include "enum_intsize.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Implemented in enum_intsize_mixed_cpp.cpp, called from the C side.
 * Returns 0 if the C++ side sees the same layout and values as the C side.
 */
int check_from_cpp(const IntSizeMessage *msg, unsigned c_sizeof_msg, unsigned c_offset_d);

#ifdef __cplusplus
}
#endif

#endif
