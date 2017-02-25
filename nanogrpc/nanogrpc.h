#ifndef __GRPC_H__
#define __GRPC_H__

#include <inttypes.h>
#include "pb.h"
#include "nanogrpc.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"


typedef uint32_t ng_GrpcStatus_t;
#define NG_PATH_DELIMITER   '/'
typedef uint32_t ng_hash_t;

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

/*typedef struct ng_method_holder_s ng_method_holder_t;
struct ng_method_holder_s {
    ng_method_t * method;
    ng_method_holder_t *next;
}; */

typedef struct ng_service_s ng_service_t;
struct ng_service_s {
  const char *name;
  ng_hash_t name_hash;
  ng_method_t *method;
  ng_service_t *next;
};

/* typedef struct ng_service_holder_s ng_service_holder_t;
struct ng_service_holder_s {
  ng_service_t *service;
  ng_service_holder_t *next;
}; */

typedef struct ng_grpc_handle_s ng_grpc_handle_t;
struct ng_grpc_handle_s {
    ng_service_t *serviceHolder;
    pb_istream_t *input;
    pb_ostream_t *output;
    GrpcRequest request;
    GrpcResponse response;
    ng_GrpcStatus_t lastStatus;
};

bool ng_GrpcParseBlocking(ng_grpc_handle_t *handle);
bool ng_GrpcRegisterService(ng_grpc_handle_t *handle, ng_service_t * service);
bool ng_addMethodToService(ng_service_t *service, ng_method_t * method);
/* ng_GrpcStatus_t ng_setMethodHandler(ng_method_t *method,
                                 ng_GrpcStatus_t (*handler)(pb_istream_t * input,
                                                       pb_ostream_t * output)); */
bool ng_setMethodCallback(ng_method_t *method,
                                      ng_GrpcStatus_t (*callback)(void* request,
                                                            void* response),
                                      void *request_holder,
                                      void *response_holder);

#endif
