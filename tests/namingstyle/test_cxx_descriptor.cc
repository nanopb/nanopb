#include <stdio.h>
#include "naming_style.pb.h"
#include "unittests.h"

int main() {
  using namespace nanopb;

  int status = 0;

  TEST(MessageDescriptor<main_message_t>::fields_array_length ==
       main_message_t_msg.field_count);

  if (status != 0) fprintf(stdout, "\n\nSome tests FAILED!\n");

  return status;
}
