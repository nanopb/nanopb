#ifndef __NG_H__
#define __NG_H__

#include "nanogrpc.pb.h"

#define DECLARE_FILL_WITH_ZEROS_FUNCTION(X) void X ## _fillWithZeros(void *ptr);

#define DEFINE_FILL_WITH_ZEROS_FUNCTION(X) void X ## _fillWithZeros(void *ptr){ \
  X zeroStruct = X ## _init_zero;         \
  if (ptr != NULL){                       \
    *(X *) ptr = zeroStruct;              \
  }                                       \
}

#define FILL_WITH_ZEROS_FUNCTION_NAME(X) X ## _fillWithZeros

#define NG_PATH_DELIMITER   '/'

typedef uint32_t ng_hash_t;
typedef uint32_t ng_GrpcStatus_t;

typedef struct ng_cleanupCallback_s ng_callback_t;
struct ng_cleanupCallback_s{
  void (*callback)(void *arg);
  void *arg;
};

typedef struct ng_method_s ng_method_t;
struct ng_method_s {
    const char *name;
    ng_hash_t nameHash;
    /* pointers to method which needs to parse input stream in itself. */
    ng_GrpcStatus_t (*handler)(pb_istream_t * input, pb_ostream_t * output);
    /* callback, if not NULL, then it will be aclled */
    ng_GrpcStatus_t (*callback)(void* request, void* response);
    void *request_holder;
    const void * request_fields;
    void (*request_fillWithZeros)(void *ptr);
    void *response_holder;
    const void * response_fields;
    void (*response_fillWithZeros)(void *ptr);
    ng_callback_t cleanup;
    ng_method_t * next; /**< Holder for next method */
};

typedef struct ng_service_s ng_service_t;
struct ng_service_s {
  const char *name;
  ng_hash_t name_hash;
  ng_method_t *method;
  ng_service_t *next;
};

typedef struct ng_grpc_handle_s ng_grpc_handle_t;
struct ng_grpc_handle_s {
    ng_service_t *serviceHolder;
    pb_istream_t *input;
    pb_ostream_t *output;
    GrpcRequest request;
    GrpcResponse response;
    ng_GrpcStatus_t lastStatus;
};

#endif
