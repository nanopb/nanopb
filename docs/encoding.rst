=========================
Nanopb: Encoding messages
=========================

The basic way to encode messages is to:

1) Write a callback function for whatever stream you want to write the message to.
2) Fill a structure with your data.
3) Call pb_encode with the stream, a pointer to *const pb_field_t* array and a pointer to your structure.

A few extra steps are necessary if you need to know the size of the message beforehand, or if you have dynamically sized fields.

Output streams
==============

This is the contents of *pb_ostream_t* structure::

 typedef struct _pb_ostream_t pb_ostream_t;
 struct _pb_ostream_t
 {
    bool (*callback)(pb_ostream_t *stream, const uint8_t *buf, size_t count);
    void *state;
    size_t max_size;
    size_t bytes_written;
 };

This, combined with the pb_write function, provides a light-weight abstraction
for whatever destination you want to write data to.

*callback* should be a pointer to your callback function. These are the rules for it:

1) Return false on IO errors. This will cause encoding to abort.
 * 
 * 2) You can use state to store your own data (e.g. buffer pointer).
 * 
 * 3) pb_write will update bytes_written after your callback runs.
 * 
 * 4) Substreams will modify max_size and bytes_written. Don't use them to
 * calculate any pointers.