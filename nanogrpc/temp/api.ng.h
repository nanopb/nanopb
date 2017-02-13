#ifndef PB_API_NG_H_INCLUDED
#define PB_API_NG_H_INCLUDED
#include "api.pb.h"
#include "nanogrpc.h"

extern ng_method_t SayHello_method;
extern ng_service_t Greeter_service;

uint32_t Greeter_init();

#endif // PB_API_NG_H_INCLUDED
