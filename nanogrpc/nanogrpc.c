#include "nanogrpc.h"
#include "nanogrpc.ng.h"
#include "pb_encode.h"
#include "pb_decode.h"


/*!
 * @brief Returns method with given hash.
 *
 * Function Iterates over all methods found in servies registered in
 * given grpc handle.
 * @param  handle pointer to grpc handle
 * @param  hash   hash of method to find
 * @return        pointer to first method whose hash match given one,
 *                Null if not found.
 */
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

/*!
 * @brief Callback for encoding response into output stream.
 *
 * This callback is called during encoding GrpcResponse. In case of fail
 * bytes will be alread written into stream, so that it is essential to
 * calculate size and ensuring that output will fit into it.
 * @param  stream pointer to stream
 * @param  field  pointer to response fields
 * @param  arg    pointer to current method
 * @return        true if successfully encoded message, false if not
 */
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


/*!
 * @brief Parses input stream and prepares ouptu stream.
 *
 * This method should be called when data to be parsed are stored in
 * input stream. Before calling user have to ensure, that ouptut stream
 * will be capable for encoding output.
 *
 * @param  handle pointer to grpc handle
 * @return        true if succed in encoding response to output stream
 *                false if didn't manage to encode output stream or in
 *                case of input problem
 */
bool ng_GrpcParseBlocking(ng_grpc_handle_t *handle){
  if (handle->input == NULL || handle->output == NULL){
    return false;
  }
  ng_GrpcStatus_t status;
 bool ret = true;
  GrpcRequest_fillWithZeros(&handle->request);
  GrpcResponse_fillWithZeros(&handle->response);
  if (pb_decode(handle->input, GrpcRequest_fields, &handle->request)){
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
          method->response_fillWithZeros(method->response_holder);
          status = method->callback(method->request_holder, method->response_holder);
          // try to encode method response to make sure, that it will be
          // possible to encode it callback.
          bool validResponse;
          size_t responseSize;
          validResponse = pb_get_encoded_size(&responseSize,
                                              method->response_fields,
                                              method->response_holder);
          if (status == GrpcStatus_OK && validResponse){
            handle->response.grpc_status = status;
            handle->response.data.funcs.encode = &encodeResponseCallback;
            handle->response.data.arg = method;
          } else if (status == GrpcStatus_OK && !validResponse){ // status ok, unable to encode
            handle->response.grpc_status = GrpcStatus_INTERNAL;
            // TODO insert here message about not being able to
            // encode method request
          } else { // callback failed, we ony encode its status.
            handle->response.grpc_status = status;
          }
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
    // TODO unable to encode
    ret = false;
  }

  #ifdef PB_ENABLE_MALLOC
  pb_release(GrpcRequest_fields, &handle->request);
  pb_release(GrpcResponse_fields, &handle->response); // TODO leave it here?
  #endif
  return ret; // default true
}

/*!
 * @brief Adds given method to given service.
 * @param  service pointer to service
 * @param  method  pointer to method to be added to service
 * @return         true if successfully added method, fasle if not
 */
bool ng_addMethodToService(ng_service_t * service, ng_method_t * method){
  if (method != NULL && service != NULL){
    method->next = service->method;
    service->method = method;
    // TODO set here method name hash (as endpoint)
    return true;
  } else {
    return false;
  }
}

/*!
 * @brief Sets handler in given method.
 *
 * Currently unused.
 * @param  method  [description]
 * @param  handler [description]
 * @return         [description]
 */
// ng_GrpcStatus_t ng_setMethodHandler(ng_method_t * method,
//                                 ng_GrpcStatus_t (*handler)(pb_istream_t * input,
//                                                       pb_ostream_t * output)){
//   method->handler = handler;
//   return GrpcStatus_OK;
// }


/*!
 * @brief Sets given callback to given method
 * @param  method   pointer to method
 * @param  callback pointer to callback
 * @return          true if success, false if not
 */
bool ng_setMethodCallback(ng_method_t *method,
                                      ng_GrpcStatus_t (*callback)(void* request,
                                                            void* response),
                                      void *request_holder,
                                      void *response_holder){
  if (callback != NULL && request_holder != NULL && response_holder != NULL){
    method->callback = callback;
    method->request_holder = request_holder;
    method->response_holder = response_holder;
    return true;
  } else {
    return false;
  }
}

/*!
 * @brief Registers service in given grpc handle.
 * @param  handle  pointer to grpc handle
 * @param  service pointer to service
 * @return         true if success, false if not
 */
bool ng_GrpcRegisterService(ng_grpc_handle_t *handle, ng_service_t * service){
  if (service != NULL && handle != NULL){
    service->next = handle->serviceHolder;
    handle->serviceHolder = service;
    return true;
  } else {
    return false;
  }
}
