#include "validation.h"
#include "alltypes_static.pb.h"
#include <assert.h>

/* Check the invariants defined in security model on decoded structure */
static void sanity_check_static(const alltypes_static_AllTypes *msg)
{
    bool truebool = true;
    bool falsebool = false;

    /* TODO: Add more checks, or rather, generate them automatically */
    assert(strlen(msg->req_string) < sizeof(msg->req_string));
    assert(strlen(msg->opt_string) < sizeof(msg->opt_string));
    if (msg->rep_string_count > 0)
    {
        assert(strlen(msg->rep_string[0]) < sizeof(msg->rep_string[0]));
    }
    assert(memcmp(&msg->req_bool, &truebool, sizeof(bool)) == 0 ||
           memcmp(&msg->req_bool, &falsebool, sizeof(bool)) == 0);
    assert(memcmp(&msg->has_opt_bool, &truebool, sizeof(bool)) == 0 ||
           memcmp(&msg->has_opt_bool, &falsebool, sizeof(bool)) == 0);
    assert(memcmp(&msg->opt_bool, &truebool, sizeof(bool)) == 0 ||
           memcmp(&msg->opt_bool, &falsebool, sizeof(bool)) == 0);
    assert(msg->rep_bool_count <= pb_arraysize(alltypes_static_AllTypes, rep_bool));
    if (msg->rep_bool_count > 0)
    {
        assert(memcmp(&msg->rep_bool[0], &truebool, sizeof(bool)) == 0 ||
               memcmp(&msg->rep_bool[0], &falsebool, sizeof(bool)) == 0);
    }
}

void validate_message(const void *msg, size_t structsize, const pb_msgdesc_t *msgtype)
{
    if (msgtype == alltypes_static_AllTypes_fields)
    {
        sanity_check_static(msg);
    }
}

