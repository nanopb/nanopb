#include "nanogrpc.h"
#include "nanogrpc.ng.h"
#include "pb_encode.h"
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

static bool encodeResponseCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    ng_method_t *method = (ng_method_t*)*arg;
    // char *str = get_string_from_somewhere();
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    // we are encoding tag for bytes, but writeing submessage,
    // as long it is prepended with size same as bytes
    return pb_encode_submessage(stream, method->response_fields, method->response_holder);
}



// static ng_GrpcStatus_t HandleRequest(ng_grpc_handle_t *handle){
//   if (handle->request == NULL){
//     return GrpcStatus_INTERNAL;
//   }
//   pb_istream_t input = pb_istream_from_buffer(handle->request->data->bytes, handle->request->data->size);
//   ng_GrpcStatus_t status;
//   ng_method_t *method = NULL;
//
//   method = getMethodByHash(handle, handle->request->path_crc);
//   if (method != NULL) {
//     if (method->handler != NULL){
//       status = method->handler(&input, NULL);
//       if (!status){
//         return status;
//       }
//     } else if (method->callback != NULL){
//       method->request_init_zero(method->request_holder);
//       if (pb_decode(&input, method->request_fields, method->request_holder))
//       {
//         method->response_init_zero(method->request_holder);
//         status = method->callback(method->request_holder, method->response_holder);
//         // pb_encode(handle->output, )
//         handle->response = GrpcResponse_init_zero;
//         handle->response.grpc_status = GrpcStatus_OK;
//         handle->response.data.encode = &encodeResponseCallback;
//         handle->response.data.arg = method;
//
//         // encoding needs to be here, because we cannot clean (release) after
//         // method outside this function. Or we can store pointer to last method
//         // in grpc handle
//         status = pb_encode(handle->output, GrpcResponse_fields, handle->response);
//         // for now we don't need to release GrpcResponse as long it dosn't
//         // have dynamically allocated fields
//         pb_release(method->request_fields, method->request_holder); // TODO release always?
//         pb_release(method->response_fields, method->response_holder);
//         return GrpcStatus_OK;
//       } else {
//         return GrpcStatus_INTERNAL;
//       }
//     } else {
//       return GrpcStatus_UNIMPLEMENTED;
//     }
//   } else {
//     return GrpcStatus_NOT_FOUND;
//   }
//   return GrpcStatus_INTERNAL; // shouldn't be reached
// }

/**
 * [ng_GrpcParse description]
 * @param  handle [description]
 * @return        true if succed in encoding response to output stream
 *                false if didn't manage to encode output stream or in
 *                case of input problem
 */
bool ng_GrpcParse(ng_grpc_handle_t *handle){
  if (handle->input == NULL || handle->output == NULL){
    return false;
  }
  ng_GrpcStatus_t status;
 bool ret = true;

  // handle->request = GrpcRequest_init_zero;
  // handle->request = {0, NULL, NULL};
  // handle->response = GrpcResponse_init_zero;
  GrpcRequest_fillWithZeros(&handle->request);
  GrpcResponse_fillWithZeros(&handle->response);
  if (pb_decode(handle->input, GrpcRequest_fields, &handle->request)){
    // code from function below desn't neet to b in separate function
    // moreover it is confusing which of those will clean after
    // calling method
    // HandleRequest(handle);
    // inser handle start
    pb_istream_t input = pb_istream_from_buffer(handle->request.data->bytes, handle->request.data->size);
    ng_method_t *method = NULL;

    method = getMethodByHash(handle, handle->request.path_crc);
    if (method != NULL) {
      // currently using handler is unimplemented
      /* if (method->handler != NULL){ // handler found
        status = method->handler(&input, NULL);
        if (!status){
          return status;
        }
      } else */ if (method->callback != NULL){ // callback found
        method->request_fillWithZeros(method->request_holder);
        if (pb_decode(&input, method->request_fields, method->request_holder)){
          method->response_fillWithZeros(method->request_holder);
          status = method->callback(method->request_holder, method->response_holder);
          handle->response.grpc_status = GrpcStatus_OK;
          handle->response.data.funcs.encode = &encodeResponseCallback;
          handle->response.data.arg = method;
          //ready to encode and return
          #ifdef PB_ENABLE_MALLOC
          // In case response would have dynamically allocated fields
          pb_release(method->request_fields, method->request_holder); // TODO release always?
          pb_release(method->response_fields, method->response_holder);
          #endif
//          ret = GrpcStatus_OK;
        } else { // unable to decode message from request holder
          handle->response.grpc_status = GrpcStatus_INVALID_ARGUMENT;
        }
      } else { // handler or callback not found
        handle->response.grpc_status = GrpcStatus_UNIMPLEMENTED;
      }
    } else { // No method found
      handle->response.grpc_status = GrpcStatus_NOT_FOUND;
    }
    // inser handle end
  } else { // Unable to decode GrpcRequest
	  // unable to pase GrpcRequest
    // return fasle // TODO ?
    handle->response.grpc_status = GrpcStatus_DATA_LOSS;
  }

  status = pb_encode(handle->output, GrpcResponse_fields, &handle->response);
  if (!status){
    // unable to encode
    ret = false;
  }

  #ifdef PB_ENABLE_MALLOC
  pb_release(GrpcRequest_fields, &handle->request);
  pb_release(GrpcResponse_fields, &handle->response); // TODO leave it here?
  #endif
  return ret; // default true
}


ng_GrpcStatus_t GrpcParsebuffer(ng_grpc_handle_t *handle, uint8_t *buf, uint32_t len){
  // GrpcRequest request;
  pb_istream_t input = pb_istream_from_buffer(buf, len);
  pb_ostream_t output = PB_OSTREAM_SIZING; // empty stream
  handle->input = &input;
  handle->output = &output;
  ng_GrpcParse(handle);


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
