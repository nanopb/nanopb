#include "nanogrpc.ng.h"

DEFINE_FILL_WITH_ZEROS_FUNCTION(GrpcRequest)
DEFINE_FILL_WITH_ZEROS_FUNCTION(GrpcResponse)

// void GrpcRequest_fillWithZeros(void *ptr){
//   GrpcRequest zeroStruct = GrpcRequest_init_zero;
//   if (ptr != NULL){
//     *(GrpcRequest *) ptr = zeroStruct;
//   }
// }


// void GrpcResponse_fillWithZeros(void *ptr){
//   GrpcResponse zeroStruct = GrpcResponse_init_zero;
//   if (ptr != NULL){
//     *(GrpcResponse *) ptr = zeroStruct;
//   }
// }
