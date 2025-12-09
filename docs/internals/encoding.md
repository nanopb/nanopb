# Internals of nanopb encoding functions

The traversal of message structure for encoding is implemented using `pb_walk()`.
This permits operations on recursive structures without directly using recursive C functions.

The problem with C recursion is the large stack usage, as all local variables and return addresses
get preserved. To navigate the structure, nanopb really only needs a couple of pointers and index
values for each message level.

To conserve stack, `pb_walk()` implements a statically sized, stack-allocated array for storage.
If the array fills up, C recursion is then used to get another array by `pb_walk()` calling itself.
This C recursion can optionally be disabled.

This approach does result in extra complexity in the encoder code when it hits a submessage field.
Instead of calling `pb_encode_submessage()` directly, it must instead return `PB_WALK_IN`.
The encoder function then gets called again with a new stackframe and with a field iterator pointing to the submessage contents.

After the submessage encoding finishes, `PB_WALK_OUT` is returned and the previous stack frame is restored.
The outer level can then continue processing its fields.

In the protobuf format, submessages must be preceded by their length.
To calculate the length, each message level has two states: pass1 and pass2.
The message is processed twice, where the first pass only calculates encoded data size but doesn't write it out.
Second pass first writes out the size, and then starts encoding all of the fields.

Finally, a `PB_WALK_OUT` from the outermost level causes an exit from `pb_walk()`.

## State diagram

The field types that require recursive processing are submessages and proto2 extension fields.
Other fields are processed one-by-one inside the "calculating" and "encoding" states.

The processing state on each level is stored using `PASS1` and `PASS2` flags.
An extra flag `SIZE_ONLY` is used when the outer message level is in the `PASS1` state.
Because it only needs to know the total size, the `PASS2` on submessages can be skipped.

![State diagram of encoding on each message level](encode_levels.drawio.svg)

## Code flow diagram

The encoder code is divided to two main functions: `pb_encode_walk_cb()` and `encode_all_fields()`.
The walk callback handles the main state machine with `PASS1` and `PASS2` states.
Helper function `encode_all_fields()` is used for iterating through the fields, which happens the same way in both states.

When a submessage or extension is encoutered, `encode_all_fields()` returns `PB_WALK_IN`.
The `pb_encode_walk_cb()` then returns this to `pb_walk()`, which will call it again on a new stackframe.

When `encode_all_fields()` is done, it returns `PB_WALK_OUT`.
The `pb_encode_walk_cb()` then checks whether `PASS1` or `PASS2` is active, and either returns or calls `encode_all_fields()` again.

After `PB_WALK_OUT` is returned to `pb_walk()`, the next invocation of the callback immediately returns `PB_WALK_NEXT_ITEM`. This is used to go to either next field or next array item after one submessage has been completely encoded. It could be done just as well inside the callback, but the `NEXT_ITEM` return value allows code sharing between encoder and decoder.

![Code flow diagram of encoding process in pb_walk, pb_encode_walk_cb and encode_all_fields](encode_flow.drawio.svg)
