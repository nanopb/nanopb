=========================
Nanopb: Encoding messages
=========================

The basic way to encode messages is to:

1) Create an `output stream`_.
2) Fill a structure with your data.
3) Call *pb_encode* with the stream, a pointer to *const pb_field_t* array and a pointer to your structure.

A few extra steps are necessary if you need to know the size of the message beforehand, or if you have dynamically sized fields.

.. _`output stream`: concepts.html#output-streams

Function: pb_encode
===================


