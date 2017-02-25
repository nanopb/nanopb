#ifndef __GRPC_H__
#define __GRPC_H__

#include <inttypes.h>
#include "pb.h"
#include "nanogrpc.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

#include "ng.h"


/*typedef struct ng_method_holder_s ng_method_holder_t;
struct ng_method_holder_s {
    ng_method_t * method;
    ng_method_holder_t *next;
}; */


/* typedef struct ng_service_holder_s ng_service_holder_t;
struct ng_service_holder_s {
  ng_service_t *service;
  ng_service_holder_t *next;
}; */


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
