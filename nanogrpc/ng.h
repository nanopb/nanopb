#ifndef __NG_H__
#define __NG_H__

#define DECLARE_FILL_WITH_ZEROS_FUNCTION(X) void X ## _fillWithZeros(void *ptr);

#define DEFINE_FILL_WITH_ZEROS_FUNCTION(X) void X ## _fillWithZeros(void *ptr){ \
  X zeroStruct = X ## _init_zero;         \
  if (ptr != NULL){                       \
    *(X *) ptr = zeroStruct;              \
  }                                       \
}

#define FILL_WITH_ZEROS_FUNCTION_NAME(X) X ## _fillWithZeros

#endif 
