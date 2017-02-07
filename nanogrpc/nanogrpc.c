#include "nanogrpc.h"
#include "pb_decode.h"

static ng_method_t * getMethodByHash(ng_grpc_handle_t *handle, ng_hash_t hash){
  ng_method_t *method = NULL;
  ng_service_t *service = handle->serviceHolder;

  while (service != NULL){ // Iterate over services
    method = service->method;
    while (method != NULL){ // Iterate over methods in service
      if (method->nameHash == hash){
        return method;
      } else {
        method = method->next;
      }
    }
    service = service->next;
  }
  return NULL;
}


static ng_GrpcStatus_t ParseRequest(ng_grpc_handle_t *handle, GrpcRequest * request){
  if (request == NULL){
    return GrpcStatus_INTERNAL;
  }
  pb_istream_t input = pb_istream_from_buffer(request->data->bytes, request->data->size);
  ng_GrpcStatus_t status;
  ng_method_t *method = NULL;

  method = getMethodByHash(handle, request->path_crc);
  if (method != NULL) {
    if (method->handler != NULL){
      status = method->handler(&input, NULL);
      if (!status){
        return status;
      }
    } else if (method->callback != NULL){
      if (pb_decode(&input, method->request_fields, method->request_holder))
      {
        status = method->callback(method->request_holder, method->response_holder);
        // TODO handle
        pb_release(method->request_fields, method->request_holder); // TODO release always?
        return GrpcStatus_OK;
      } else {
        return GrpcStatus_INTERNAL;
      }
    } else {
      return GrpcStatus_UNIMPLEMENTED;
    }
  } else {
    return GrpcStatus_NOT_FOUND;
  }
  return GrpcStatus_INTERNAL; // shouldn't be reached
}

ng_GrpcStatus_t GrpcParsebuffer(ng_grpc_handle_t *handle, uint8_t *buf, uint32_t len){
  GrpcRequest request;
  pb_istream_t input = pb_istream_from_buffer(buf, len);
  if (pb_decode(&input, GrpcRequest_fields, &request))
  {
    ParseRequest(handle, &request);
	  pb_release(GrpcRequest_fields, &request);
  } else {
	  while(1);
    // return GrpcStatus_INTERNAL;
  }

  return 0;
}


ng_GrpcStatus_t ng_addMethodToService(ng_service_t * service, ng_method_t * method){
  if (method != NULL && service != NULL){
    method->next = service->method;
    service->method = method;
    // TODO set here method name hash (as endpoint)
    return GrpcStatus_OK;
  } else {
    return GrpcStatus_INTERNAL;
  }
}

ng_GrpcStatus_t ng_setMethodHandler(ng_method_t * method,
                                ng_GrpcStatus_t (*handler)(pb_istream_t * input,
                                                      pb_ostream_t * output)){
  method->handler = handler;
  return GrpcStatus_OK;
}

ng_GrpcStatus_t ng_setMethodCallback(ng_method_t *method,
                                      ng_GrpcStatus_t (*callback)(void* request,
                                                            void* response),
                                      void *request_holder,
                                      void *response_holder){
  if (callback != NULL && request_holder != NULL, response_holder != NULL){
    method->callback = callback;
    method->request_holder = request_holder;
    method->response_holder = response_holder;
    return GrpcStatus_OK;
  } else {
    return GrpcStatus_INTERNAL;
  }
}

ng_GrpcStatus_t ng_GrpcRegisterService(ng_grpc_handle_t *handle, ng_service_t * service){
  if (service != NULL && handle != NULL){
    service->next = handle->serviceHolder;
    handle->serviceHolder = service;
    return GrpcStatus_OK;
  } else {
    return GrpcStatus_INTERNAL;
  }
}
