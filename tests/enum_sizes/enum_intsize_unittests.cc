#include <stdio.h>
#include "enum_intsize.pb.hpp"
#include "unittests.h"

extern "C" int main()
{
  int status = 0;

  TEST(sizeof(IntSizeInt8) == sizeof(uint8_t));
  TEST(sizeof(IntSizeInt16) == sizeof(uint16_t));
  TEST(sizeof(IntSizeInt32) == sizeof(uint32_t));
  TEST(sizeof(IntSizeInt64) == sizeof(uint64_t));

  if (status != 0) fprintf(stdout, "\n\nSome tests FAILED!\n");

  return status;
}
