#include <stdio.h>
#include "message.pb.h"
#include "unittests.h"

extern "C" int main() {
  using namespace nanopb;

#if __cplusplus >= 201103L
  static_assert(MessageDescriptor<MyEmptyMessage>::fields_array_length == 0,
                "Unexpected field length");
  static_assert(MessageDescriptor<MyNonEmptyMessage>::fields_array_length == 1,
                "Unexpected field length");
#endif // C++11 & newer

  int status = 0;

  TEST(MessageDescriptor<MyEmptyMessage>::fields_array_length ==
       MyEmptyMessage_msg.field_count);
  TEST(MessageDescriptor<MyNonEmptyMessage>::fields_array_length ==
       MyNonEmptyMessage_msg.field_count);

  TEST(MessageDescriptor<MyNonEmptyMessage>::size ==
       MyNonEmptyMessage_size);
  TEST(MessageDescriptor<MyMessageWithSizeBoundRepeatedFields>::size ==
       MyMessageWithSizeBoundRepeatedFields_size);

  TEST(MessageDescriptor<MyEmptyMessage>::fields() == MyEmptyMessage_fields);
  TEST(MessageDescriptor<MyNonEmptyMessage>::fields() ==
       MyNonEmptyMessage_fields);

  TEST(MessageDescriptor<MyMessageWithMsgid>::has_msgid() == true);
  TEST(MessageDescriptor<MyMessageWithMsgid>::msgid() == 42);

  TEST(MessageDescriptor<MyMessageWithoutMsgid>::has_msgid() == false);


  if (status != 0) fprintf(stdout, "\n\nSome tests FAILED!\n");

  return status;
}
