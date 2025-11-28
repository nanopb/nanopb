/* Prints out a cyclic protobuf message as a JSON-like string
 * using nanopb callbacks.
 */

#include <pb_decode.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "test_helpers.h"
#include "cyclic_callback.pb.h"

static bool print_stringvalue(pb_istream_t *stream)
{
    char buf[32];

    uint32_t len;
    if (!pb_decode_varint32(stream, &len))
        return false;
    
    if (len > sizeof(buf) - 1)
        PB_RETURN_ERROR(stream, "string too large");
    
    if (!pb_read(stream, (pb_byte_t*)buf, len))
        return false;
    
    buf[len] = '\0';

    printf("'%s'", buf);
    return true;
}

static bool decode_treenode(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    if (field && field->tag == TreeNode_left_tag)
        printf("[");
    
    TreeNode node = TreeNode_init_zero;
    node.left.funcs.decode = decode_treenode;
    node.right.funcs.decode = decode_treenode;

    if (!pb_decode(stream, TreeNode_fields, &node))
        return false;
    
    if (node.has_leaf)
    {
        printf("%d", node.leaf);
    }

    if (field && field->tag == TreeNode_left_tag)
        printf(",");
    else if (field && field->tag == TreeNode_right_tag)
        printf("]");
    
    return true;
}

static bool decode_dictionary(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    pb_wire_type_t wt;
    pb_tag_t tag;
    bool eof;
    while (pb_decode_tag(stream, &wt, &tag, &eof))
    {
        if (tag == KeyValuePair_key_tag)
        {
            if (*arg != NULL) printf("%s", (char*)*arg);
            *arg = ", ";

            print_stringvalue(stream);
            printf(": ");
        }
        else if (tag == KeyValuePair_stringValue_tag)
        {
            print_stringvalue(stream);
        }
        else if (tag == KeyValuePair_intValue_tag)
        {
            int32_t val;
            if (!pb_decode_varint32(stream, (uint32_t*)&val))
                return false;
            
            printf("%d", val);
        }
        else if (tag == KeyValuePair_dictValue_tag)
        {
            Dictionary dict = Dictionary_init_zero;
            dict.dictItem.funcs.decode = decode_dictionary;

            printf("{");
            size_t old_length;
            if (!pb_decode_open_substream(stream, &old_length) ||
                !pb_decode(stream, Dictionary_fields, &dict) ||
                !pb_decode_close_substream(stream, old_length))
            {
                return false;
            }
            printf("}");
        }
        else if (tag == KeyValuePair_treeValue_tag)
        {
            size_t old_length;
            if (!pb_decode_open_substream(stream, &old_length) ||
                !decode_treenode(stream, NULL, NULL) ||
                !pb_decode_close_substream(stream, old_length))
            {
                return false;
            }
        }
        else
        {
            if (!pb_skip_field(stream, wt))
                return false;
        }
    }
    
    return true;
}

int main(int argc, char *argv[])
{
    uint8_t buffer[1024];
    
    SET_BINARY_MODE(stdin);
    size_t count = fread(buffer, 1, sizeof(buffer), stdin);

    pb_istream_t stream = pb_istream_from_buffer(buffer, count);

    Dictionary dict = Dictionary_init_zero;
    dict.dictItem.funcs.decode = decode_dictionary;

    printf("{");
    if (!pb_decode(&stream, Dictionary_fields, &dict))
    {
        fprintf(stderr, "Decoding error: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }
    printf("}\n");

    return 0;
}