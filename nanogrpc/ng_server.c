/* #include "nanogrpc.ng.h" */
#include "pb_encode.h"
#include "pb_decode.h"
#include "ng.h"
#include "ng_server.h"
#include <stdio.h>


static DEFINE_FILL_WITH_ZEROS_FUNCTION(GrpcRequest)
static DEFINE_FILL_WITH_ZEROS_FUNCTION(GrpcResponse)


#include <string.h>

const char* errorMessages[] = {
  "Callback failed",
  "Unable to decode message from request holder",
  "Unable to register call",
  "callback not found",
  "No method found"
};



enum grpcError  {
  GrpcErrorMsg_callback_failed = 0,
  GrpcErrorMsg_unable_to_decode_message,
  GrpcErrorMsg_unable_to_register_call,
  GrpcErrorMsg_callback_not_found,
  GrpcErrorMsg_no_method_found
};


static bool encodeErrorMessageCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
  uint32_t index = *(uint32_t*)arg;
  /* char *str = get_string_from_somewhere(); */
  if (!pb_encode_tag_for_field(stream, field))
      return false;
  /* we are encoding tag for bytes, but writeing submessage, */
  /* as long it is prepended with size same as bytes */
  /*return pb_encode_submessage(stream, method->response_fields, method->response_holder);*/
  return  pb_encode_string(stream, (pb_byte_t*) errorMessages[index], strlen(errorMessages[index]));
}

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

  while (service != NULL){ /* Iterate over services */
    method = service->method;
    while (method != NULL){ /* Iterate over methods in service */
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
 * @brief Returns method with given name.
 *
 * Function Iterates over all methods found in servies registered in
 * given grpc handle. Method name needs to start with '/' and have to be
 * delimited with '/'.
 * @param  handle pointer to grpc handle
 * @param  name   name of method to find
 * @return        pointer to first method whose hash match given one,
 *                Null if not found.
 */
static ng_method_t * getMethodByPath(ng_grpc_handle_t *handle, char * path){
  ng_method_t *method = NULL;
  ng_service_t *service = handle->serviceHolder;
  char * serviceName, * methodName;
  size_t serviceNameLenth, methodNameLength;

  if (path[0] != '/' || path[0] == '\0'){
    return NULL;
  }
  serviceName = path + 1;
  methodName = strpbrk(serviceName, "/");
  if (methodName != NULL){
    serviceNameLenth = methodName - serviceName;

    if (serviceNameLenth > 0 && strlen(methodName) > 1){
      methodName += 1;
      methodNameLength = strlen(methodName);
      if (strpbrk(methodName, "/") != NULL){ /* there should be no more delimiters in path*/
        return NULL;
      }
    } else { /* service name or method name without characters  */
      return NULL;
    }
  } else { /* no more slashes in path but should be one */
    return NULL;
  }

  /* in order to compare names we could use strcmp, but it requires
   * zero terminated string, but if we don't want to insert zeros into
   * given string we have to use strncmp and compare names lengths. */
  while (service != NULL){ /* Iterate over services */
    if (strncmp(service->name, serviceName, serviceNameLenth) == 0 &&
        strlen(service->name) == serviceNameLenth){ /* service name match */
      method = service->method;
      while (method != NULL){ /* Iterate over methods in service */
        if (strncmp(method->name, methodName, methodNameLength) == 0 &&
            strlen(method->name) == methodNameLength){ /* methodname match */
          return method;
        } else {
          method = method->next;
        }
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
  ng_methodContext_t *ctx = (ng_methodContext_t*)*arg;
  /* char *str = get_string_from_somewhere(); */
  if (!pb_encode_tag_for_field(stream, field))
      return false;
  /* we are encoding tag for bytes, but writeing submessage, */
  /* as long it is prepended with size same as bytes */
  /*return pb_encode_submessage(stream, method->response_fields, method->response_holder);*/
  return  pb_encode_submessage(stream, ctx->method->response_fields, ctx->response);
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
  ng_CallbackStatus_t status;
  ng_method_t *method = NULL;
  ng_methodContext_t* ctx = NULL;
  bool ret = true;

  if (handle->input == NULL || handle->output == NULL){
    return false;
  }

  GrpcRequest_fillWithZeros(&handle->request);
  GrpcResponse_fillWithZeros(&handle->response);

  if (pb_decode(handle->input, GrpcRequest_fields, &handle->request)){
    pb_istream_t input;
    input = pb_istream_from_buffer(handle->request.data->bytes, handle->request.data->size);

    handle->response.call_id = handle->request.call_id;

    /* look for method by hash only if it has been provided */
    if (handle->request.path_hash != 0){
      method = getMethodByHash(handle, handle->request.path_hash);
    }
    /* if hash hasn't been provided, we can still try to find by name */
    if (method == NULL && handle->request.path != NULL){
        method = getMethodByPath(handle, handle->request.path);
    }

    if (method != NULL) {
      if (method->callback != NULL &&
          ((method->context != NULL) ?
            (method->context->request && method->context->response):
            false)){ /* callback and context found */
        ctx = method->context;
        ctx->method = method;
        method->request_fillWithZeros(ctx->request);
        if (pb_decode(&input, method->request_fields, ctx->request)){
          method->response_fillWithZeros(ctx->response);
          status = method->callback(ctx);

          if (status == CallbackStatus_Ok){
            bool validResponse = false;
            size_t responseSize;
            /* try to encode method response to make sure, that it will be
            * possible to encode it callback.  */
            validResponse = pb_get_encoded_size(&responseSize,
                                                method->response_fields,
                                                ctx->response);
            if (validResponse){
              handle->response.grpc_status = GrpcStatus_OK;
              handle->response.data.funcs.encode = &encodeResponseCallback;
              /*ng_encodeMessageCallbackArgument_t arg;
              arg.method = method;
              arg.context = ctx;*/
              handle->response.data.arg = ctx;
            } else {
              handle->response.grpc_status = GrpcStatus_INTERNAL;
              /* TODO insert here message about not being able to
              * encode method request? */
            }
          } else { /* callback failed, we ony encode its status, streaming, non blocking not supported here. */
            handle->response.grpc_status = GrpcStatus_INTERNAL;
          }
         /* ret = GrpcStatus_OK; */
       } else { /* unable to decode message from request holder */
          handle->response.grpc_status = GrpcStatus_INVALID_ARGUMENT;
        }
      } else { /* handler not found or no context */
        handle->response.grpc_status = GrpcStatus_UNIMPLEMENTED;
      }
    } else { /* No method found*/
      handle->response.grpc_status = GrpcStatus_NOT_FOUND;
    }
    /* inser handle end */
  } else { /* Unable to decode GrpcRequest */
	  /* unable to pase GrpcRequest */
    /* return fasle // TODO ? */
    handle->response.call_id = 0;
    handle->response.grpc_status = GrpcStatus_DATA_LOSS;
  }
  if (!pb_encode(handle->output, GrpcResponse_fields, &handle->response)){
    /* TODO unable to encode */
    ret = false;
  }

  if (method != NULL){
    if (method->cleanup.callback != NULL){
      method->cleanup.callback(method->cleanup.arg);
    }
    #ifdef PB_ENABLE_MALLOC
    if (ctx != NULL){
      /* In case response would have dynamically allocated fields */
      pb_release(method->request_fields, ctx->request); /* TODO release always?*/
      pb_release(method->response_fields, ctx->response);
    }
    #endif
  }

  #ifdef PB_ENABLE_MALLOC
  pb_release(GrpcRequest_fields, &handle->request);
  pb_release(GrpcResponse_fields, &handle->response); /* TODO leave it here? */
  #endif
  return ret; /* default true */
}

/*!
 * @brief Adds given method to given service.
 * @param  service pointer to service
 * @param  method  pointer to method to be added to service
 * @return         true if successfully added method, fasle if not
 */
bool ng_addMethodToService(ng_service_t * service, ng_method_t * method){
  /* Check if method isn't already registered */
  ng_method_t* temp = service->method;
  while (temp != NULL){
    if (temp == method){
      return false;
    }
    temp = temp->next;
  }

  if (method != NULL && service != NULL){
    method->next = service->method;
    service->method = method;
    /* TODO set here method name hash (as endpoint) */
    return true;
  } else {
    return false;
  }
}


bool ng_setMethodContext(ng_method_t* method, ng_methodContext_t* ctx){
  ng_methodContext_t* temp = method->context;
  while (temp != NULL){
    if (temp == ctx){
      return false;
    }
    temp = temp->next;
  }
  if (ctx != NULL){
    method->context = ctx;
    ctx->method = method;
    return true;
  } else {
    return false;
  }
}

/*!
 * @brief Sets given callback to given method
 * @param  method   pointer to method
 * @param  callback pointer to callback
 * @return          true if success, false if not
 */
bool ng_setMethodCallback(ng_method_t *method,
		ng_CallbackStatus_t (*callback)(ng_methodContext_t* ctx)){
  if (callback != NULL){
    method->callback = callback;
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
  /* Check if service isn't already registered */
  ng_service_t* temp = handle->serviceHolder;
  while (temp != NULL){
    if (temp == service){
      return false;
    }
    temp = temp->next;
  }

  if (service != NULL && handle != NULL){
    service->next = handle->serviceHolder;
    handle->serviceHolder = service;
    return true;
  } else {
    return false;
  }
}

/*
 Non blocking, async, streaming
 */

/*!
 * @brief Registers call in handle with given method pointer and call_id.
 *
 * This concept allows for calling same method in paralell, but it would require
 * from service to hold multiple method holders, or even allocate them
 * dynamically? Or methods could have request, repsonse holders dynamicaly
 * allocated.
 * @param  handle pointer to grpc handle
 * @param  method pointer to method
 * @param  call_id id of current call
 * @return        true if managed to register call, false otherwise
 */
static bool ng_registerCall(ng_grpc_handle_t *handle, ng_methodContext_t *ctx, ng_callId_t id){
  uint32_t i;
  if (handle->callsHolder == NULL){
    return false;
  }
  for (i=0; i< handle->callsHolderSize; i++){
    if (handle->callsHolder[i].call_id == 0){
      handle->callsHolder[i].call_id = id;
      handle->callsHolder[i].context = ctx;
      return true;
    }
  }
  return false;
}


static bool ng_removeCall(ng_grpc_handle_t *handle, ng_callId_t id){
  uint32_t i;
  if (id == 0){
    return false;
  }

  for (i=0; i< handle->callsHolderSize; i++){
    if (handle->callsHolder[i].call_id == id){
      handle->callsHolder[i].call_id = 0;
      handle->callsHolder[i].context = NULL;
      return true;
    }
  }
  return false;
}

bool ng_isContextUsedByOngoingCall(ng_grpc_handle_t* handle, ng_methodContext_t* ctx){
  uint32_t i;
  if (handle == NULL || ctx == NULL){
    return false;
  }
  for (i=0; i< handle->callsHolderSize; i++){
    if (handle->callsHolder[i].call_id != 0 &&
        handle->callsHolder[i].context == ctx){
      return true;
    }
  }
  return false;
}

/*!
 * @brief  Returns pointer to valid context which is not used by ongoing call.
 * @param  handle pointer to grpc handle
 * @param  method pointer to method
 * @return        pointer to valid context, NULL if no such found
 */
static ng_methodContext_t* ng_getValidContext(ng_grpc_handle_t* handle, ng_method_t* method){
  /* For now we are returning only first context, but just in case we are
    checking if it is not being used by some ongoing call. In future in case
    of having more contexts per method this function will handle providing
    correct context (or something like this :) ) */
  if (method->context != NULL){
    if (method->context->request != NULL &&
        method->context->response != NULL){
      /*if (ng_isContextUsedByOngoingCall(handle, method->context)){
        ng_removeCall();
        return NULL;
      } else {
        return method->context;
      }*/

      /* We could chekc if context is not being used by some call,
         but let's remove it. Removeing call with id = 0 won't do anyhing.
         just return false, but that's fine. */
      ng_removeCall(handle, method->context->call_id);
      /*method->context->method = method; */ /* moved to ng_setMethodContext */
    	return method->context;
    }
  }

  return NULL;
}

/*!
 * Parsed input buffer in nonblocking manner
 * @param  handle pointer to grpc handle
 * @return        true if successfully paresed, false otherwise
 */
bool ng_GrpcParseNonBlocking(ng_grpc_handle_t* handle){
  ng_CallbackStatus_t status;
  ng_method_t *method = NULL;
  ng_methodContext_t* ctx = NULL;
  uint32_t error = 0;
  bool sendNow = true;
  bool ret = true;
  if (handle->input == NULL || handle->output == NULL ||
      handle->canIWriteToOutput == NULL || handle->outputReady == NULL){
    return false;
  }
  GrpcRequest_fillWithZeros(&handle->request);
  GrpcResponse_fillWithZeros(&handle->response);

  if (pb_decode(handle->input, GrpcRequest_fields, &handle->request)){
    ng_callId_t call_id = handle->response.call_id = handle->request.call_id;
    pb_istream_t input = pb_istream_from_buffer(handle->request.data->bytes, handle->request.data->size);

    /* look for method by hash only if it has been provided */
    if (handle->request.path_hash != 0){
      method = getMethodByHash(handle, handle->request.path_hash);
    }
    /* if hash hasn't been provided, we can still try to find by name */
    if (method == NULL && handle->request.path != NULL){
        method = getMethodByPath(handle, handle->request.path);
    }

    if (method != NULL) {
      if (method->callback != NULL){
        ctx = ng_getValidContext(handle, method);
        if (ctx != NULL){ /* context found */
          /* ctx = method->context;
          ctx->method = method; */ /* TODO should that be assigned here? theoretically
                                  context should all the time belong to method, but
                                  in case of creating contexts dynamically we should
                                  not bother here about setting it. Lets just obtain
                                  valid context for method? */
          if (ng_registerCall(handle, ctx, call_id)){
            method->request_fillWithZeros(ctx->request);
            if (pb_decode(&input, method->request_fields, ctx->request)){
              ctx->call_id = call_id;
              method->response_fillWithZeros(ctx->response);
              status = method->callback(ctx);

              if (status == CallbackStatus_Ok ||
                  status == CallbackStatus_WillContinueLater){
                bool validResponse = false;
                size_t responseSize;
                /* try to encode method response to make sure, that it will be
                * possible to encode it callback.  */
                validResponse = pb_get_encoded_size(&responseSize,
                                                    method->response_fields,
                                                    ctx->response);
                if (validResponse){
                  handle->response.grpc_status = GrpcStatus_OK;
                  handle->response.data.funcs.encode = &encodeResponseCallback;
                  handle->response.data.arg = ctx;
                } else {
                  handle->response.grpc_status = GrpcStatus_INTERNAL;
                  /* TODO insert here message about not being able to
                  * encode method request? */
                }
                if (status == CallbackStatus_Ok || method->server_streaming == false){
                  handle->response.has_response_type = true;
                  handle->response.response_type = GrpcResponseType_END_OF_CALL;
                  ng_removeCall(handle, call_id);
                }
              } else if (status == CallbackStatus_WillRespondLater){
                sendNow = false;
              } else { /* callback failed. */
                handle->response.grpc_status = GrpcStatus_INTERNAL;
                handle->response.grpc_mesage.funcs.encode = &encodeErrorMessageCallback;
                error = GrpcErrorMsg_callback_failed;
                handle->response.grpc_mesage.arg = &error;
              }
           } else { /* unable to decode message from request holder */
              /* We registered it previously but since decoding failed
                 we have to remove it since we won't be able to respond */
              ng_removeCall(handle, call_id);
              handle->response.grpc_status = GrpcStatus_INVALID_ARGUMENT;
              handle->response.grpc_mesage.funcs.encode = &encodeErrorMessageCallback;
              error = GrpcErrorMsg_unable_to_decode_message;
      		    handle->response.grpc_mesage.arg = &error;
            }
          } else { /* Unable to register call */
            handle->response.grpc_status = GrpcStatus_INTERNAL;
            handle->response.grpc_mesage.funcs.encode = &encodeErrorMessageCallback;
            error = GrpcErrorMsg_unable_to_register_call;
      		  handle->response.grpc_mesage.arg = &error;
          }
        } else { /* no available context */
          handle->response.grpc_status = GrpcStatus_INTERNAL;
        }
      } else { /* callback not found */
        handle->response.grpc_status = GrpcStatus_UNIMPLEMENTED;
      }
    } else { /* No method found*/
      handle->response.grpc_status = GrpcStatus_NOT_FOUND;
    }
    /* inser handle end */
  } else { /* Unable to decode GrpcRequest */
	  /* unable to pase GrpcRequest */
    /* return fasle // TODO ? */
    handle->response.call_id = 0;
    handle->response.grpc_status = GrpcStatus_DATA_LOSS;
  }
  if (sendNow){
    if (handle->canIWriteToOutput(handle)){
      if (!pb_encode(handle->output, GrpcResponse_fields, &handle->response)){
        /* TODO unable to encode */
        ret = false;
      }
      handle->outputReady(handle);
    } else {
      /* TODO handle error? */
    }
    #ifdef PB_ENABLE_MALLOC
    if (method != NULL && ctx != NULL){
      /* In case response would have dynamically allocated fields */
      if (ctx->request != NULL){
        pb_release(method->request_fields, ctx->request); /* TODO release always?*/
      }
      if (ctx->response){
        pb_release(method->response_fields, ctx->response);
      }
    }
    #endif
  }

  /*if (method != NULL){
    if (method->cleanup.callback != NULL){
      method->cleanup.callback(method->cleanup.arg);
    }
  } */

  #ifdef PB_ENABLE_MALLOC
  pb_release(GrpcRequest_fields, &handle->request);
  pb_release(GrpcResponse_fields, &handle->response); /* TODO leave it here? */
  #endif
  return ret; /* default true */
}

/*
static ng_methodContext_t* getContextByCallId(ng_grpc_handle_t *handle, ng_callId_t id){
  uint32_t i;
  for (i=0; i< handle->callsHolderSize; i++){
    if (handle->callsHolder[i].call_id == id){
      return handle->callsHolder[i].context;
    }
  }
  return NULL;
}*/


bool ng_isCallOngoing(ng_grpc_handle_t* handle, ng_callId_t id){
  uint32_t i;
  if (handle == NULL || id == 0){
    return false;
  }
  for (i=0; i< handle->callsHolderSize; i++){
    if (handle->callsHolder[i].call_id == id){
      return true;
    }
  }
  return false;
}


bool ng_AsyncResponse(ng_grpc_handle_t *handle, ng_methodContext_t* ctx, bool endOfCall){
  bool ret = true;
  /* Theoreticaly canIWriteToOutput and ctx should be verified previously */
  if (handle != NULL && ctx != NULL){
    if (ng_isCallOngoing(handle, ctx->call_id)){
      if (handle->canIWriteToOutput(handle)){
        bool validResponse;
        size_t responseSize;

        GrpcResponse_fillWithZeros(&handle->response);
        validResponse = pb_get_encoded_size(&responseSize,
                                            ctx->method->response_fields,
                                            ctx->response);
        if (validResponse){
          handle->response.call_id = ctx->call_id;
          handle->response.grpc_status = GrpcStatus_OK;
          handle->response.data.funcs.encode = &encodeResponseCallback;
          handle->response.data.arg = ctx;
          if (endOfCall || ctx->method->server_streaming == false){
            handle->response.has_response_type = true;
            handle->response.response_type = GrpcResponseType_END_OF_CALL;
          } else {
            handle->response.has_response_type = true;
            handle->response.response_type = GrpcResponseType_STREAM;
          }

          if (!pb_encode(handle->output, GrpcResponse_fields, &handle->response)){
            /* TODO unable to encode */
            ret = false;
          } else {
            if (handle->outputReady != NULL){
              handle->outputReady(handle);
            }
            if (endOfCall || ctx->method->server_streaming == false){
              #ifdef PB_ENABLE_MALLOC
              if (ctx != NULL){
                /* In case response would have dynamically allocated fields */
                if (ctx->request != NULL){
                  pb_release(ctx->method->request_fields, ctx->request);
                }
              }
              #endif
              ng_removeCall(handle, ctx->call_id);
            }
          }

          #ifdef PB_ENABLE_MALLOC
          if (ctx != NULL){
            /* In case response would have dynamically allocated fields */
            if (ctx->response != NULL){
              pb_release(ctx->method->response_fields, ctx->response);
            }
          }
          #endif

        } else { /* Response not valid */
          /* TODO unable response not valid. Drop it. Report? */
          ret = false;
        }
      } else { /* unable to stream */
        /* TODO inform user, that he should try to shedule sending later */
        ret = false;
      }
    } else { /* There is no such call onging. function shouldn't be called */
      ret = false;
    }
  } else { /* invalid handle and context */
    ret = false;
  }
  return ret;
}


bool ng_endOfCall(ng_grpc_handle_t* handle, ng_methodContext_t* ctx){
  bool ret = true;
  if(handle != NULL && ctx != NULL){
    if (ng_isCallOngoing(handle, ctx->call_id)){
      if (handle->canIWriteToOutput(handle)){
        GrpcResponse_fillWithZeros(&handle->response);

        handle->response.call_id = ctx->call_id;
        handle->response.grpc_status = GrpcStatus_OK;
        handle->response.has_response_type = true;
        handle->response.response_type = GrpcResponseType_END_OF_CALL;

        if (!pb_encode(handle->output, GrpcResponse_fields, &handle->response)){
          /* TODO unable to encode */
          ret = false;
        } else {
          if (handle->outputReady != NULL){
            handle->outputReady(handle);
          }
          ng_removeCall(handle, ctx->call_id);
          #ifdef PB_ENABLE_MALLOC
          if (ctx != NULL){
            /* In case response would have dynamically allocated fields */
            if (ctx->response){
              pb_release(ctx->method->request_fields, ctx->request);
              pb_release(ctx->method->response_fields, ctx->response);
            }
          }
          #endif
        }
      } else {
        ret = false;
      }
    } else {
      ret = false;
    }
  } else {
    ret = false;
  }
  return ret;
}
