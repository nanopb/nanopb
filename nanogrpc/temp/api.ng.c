#include "api.ng.h"
#include "ng.h"

DEFINE_FILL_WITH_ZEROS_FUNCTION(HelloRequest)
DEFINE_FILL_WITH_ZEROS_FUNCTION(HelloResponse)
//
// static void HelloRequest_fillWithZeros(void * ptr){
//   HelloReqeust zeroStruct = HelloReqeust_init_zero;
//   if (ptr != NULL){
//     *(HelloReqeust *)ptr = zeroStruct;
//   }
// }
//
// static void HelloResponse_fillWithZeros(void * ptr){
//   HelloResponse zeroStruct = HelloResponse_init_zero;
//   if (ptr != NULL){
//     *(HelloResponse *)ptr = zeroStruct;
//   }
// }

ng_method_t SayHello_method = {
  "SayHello",                     // name
  1,                              // name_hash
  NULL,                           // handler
  NULL,                           // callback
  NULL,                           // request_holder
  HelloRequest_fields,            // request_fields
  &FILL_WITH_ZEROS_FUNCTION_NAME(HelloRequest),   // fill with zeros

  NULL,
  HelloResponse_fields,
  &FILL_WITH_ZEROS_FUNCTION_NAME(HelloResponse),
  NULL
};
ng_service_t Greeter_service = {
  "Greeter",
  3,
  NULL
};

uint32_t Greeter_init(){
  ng_addMethodToService(&Greeter_service, &SayHello_method);
}
