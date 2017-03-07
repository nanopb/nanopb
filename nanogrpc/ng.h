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
typedef uint32_t ng_callId_t;

typedef enum _CallbackStatus {
  CallbackStatus_Ok, /*!< If streamin callback returns it, Response will have Enf of Call set */
  CallbackStatus_WillRespondLater, /*!< Status ok for streaming callback, no response prepared for encoding */
  CallbackStatus_WillContinueLater, /*!< Status ok for streaming callbcack, response prepared for encoding */
  CallbackStatus_Failed
} ng_CallbackStatus_t;

typedef enum
{
  NG_UNLOCKED = 0x00U,
  NG_LOCKED   = 0x01U
} ng_lock_t;


/* You can define custom functions for strings operations
 * without includint whole string library */
#ifndef ng_strcmp
#include <string.h>
#define ng_strncmp(X, Y) strncmp(X, Y)
#else
extern int ng_strncmp( const char * str1, const char * str2 );
#endif

#ifndef ng_strnlen
#include <string.h>
#define ng_strnlen(X) strnlen(X)
#else
extern size_t ng_strnlen(const char *s, size_t maxlen);
#endif

#ifndef ng_strpbrk
#include <string.h>
#define ng_strpbrk(X, Y) strpbrk(x, Y)
#else
extern char * ng_strpbrk(char * str1, const char * str2 );
#endif


typedef struct ng_cleanupCallback_s ng_callback_t;
struct ng_cleanupCallback_s{
  void (*callback)(void *arg);
  void *arg;
};

typedef struct ng_method_s ng_method_t;

typedef struct ng_methodContext_s ng_methodContext_t;
struct ng_methodContext_s {
  void* request;
  void* response;
  ng_callId_t call_id;
  ng_method_t* method;
  ng_methodContext_t* next;
};

struct ng_method_s {
    const char *name;
    ng_hash_t nameHash;
    /* pointers to method which needs to parse input stream in itself. */
    /*ng_GrpcStatus_t (*handler)(pb_istream_t * input, pb_ostream_t * output); */
    /* callback, if not NULL, then it will be called */
    ng_CallbackStatus_t (*callback)(ng_methodContext_t* ctx);
    /* void* request_holder; */
    ng_methodContext_t* context;
    const void* request_fields;
    void (*request_fillWithZeros)(void *ptr);
    /* void *response_holder; */
    const void * response_fields;
    void (*response_fillWithZeros)(void *ptr);
    const bool server_streaming;
    const bool client_streaming;
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

typedef struct ng_call_s ng_call_t;
struct ng_call_s{
  ng_callId_t call_id;
  ng_methodContext_t * context;
  /* Some timeout related data? */
};

typedef struct ng_grpc_handle_s ng_grpc_handle_t;
struct ng_grpc_handle_s {
    ng_service_t *serviceHolder;
    pb_istream_t *input;
    pb_ostream_t *output;
    GrpcRequest request;
    GrpcResponse response;
    ng_GrpcStatus_t lastStatus;
    bool (* canIWriteToOutput)(ng_grpc_handle_t* handle);
    void (* outputReady)(ng_grpc_handle_t* handle);
    ng_call_t *callsHolder;
    uint32_t callsHolderSize;
    ng_lock_t lock;
};

/* I don't have idea for better name for this structure, but still it is better
 * ti have this than passing array of void pointers to callback function */
typedef struct ng_encodeMessageCallbackArgument_s ng_encodeMessageCallbackArgument_t;
struct ng_encodeMessageCallbackArgument_s {
  ng_method_t* method;
  ng_methodContext_t* context;
};

#endif
