/*#include <stdio.h> */
#include "unittests.h"
#include "unittests.ng.h"

#include "ng_server.c"

/* This declaration stays here (instead of main) because
   "ISO C90 forbids mixed declarations and code"  error */
ng_grpc_handle_t hGrpc;

extern DummyRequest FirstTestService_FirstTestMethod_DefaultRequest;
extern DummyResponse FirstTestService_FirstTestMethod_DefaultResponse;
extern ng_methodContext_t FirstTestService_FirstTestMethod_DefaultContext;

extern AnotherDummyRequest FirstTestService_SecondTestMethod_DefaultRequest;
extern AnotherDummyResponse FirstTestService_SecondTestMethod_DefaultResponse;
extern ng_methodContext_t FirstTestService_SecondTestMethod_DefaultContext;


ng_CallbackStatus_t Dummy_methodCallback(ng_methodContext_t* ctx){
  return CallbackStatus_Ok;
}


int main()
{
    int status = 0;
    bool ret;

    /* Test adding methods to services */
    TEST(FirstTestService_service.method == NULL);
    ret = ng_addMethodToService(&FirstTestService_service, &FirstTestService_FirstTestMethod_method);
    TEST(ret == true);
    TEST(FirstTestService_service.method == &FirstTestService_FirstTestMethod_method);
    TEST(FirstTestService_service.method->next == NULL);
    /* We shouldn't be able to add the same method again */
    ret = ng_addMethodToService(&FirstTestService_service, &FirstTestService_FirstTestMethod_method);
    TEST(ret == false);
    /* Add second method */
    ret = ng_addMethodToService(&FirstTestService_service, &FirstTestService_SecondTestMethod_method);
    TEST(ret == true);
    /* Check all pointers */
    TEST(FirstTestService_service.method == &FirstTestService_SecondTestMethod_method);
    TEST(FirstTestService_service.method->next == &FirstTestService_FirstTestMethod_method);
    TEST(FirstTestService_service.method->next->next == NULL);
    /* Lets try one more time to add both methods to service (in reverse order? why not) */
    ret = ng_addMethodToService(&FirstTestService_service, &FirstTestService_SecondTestMethod_method);
    TEST(ret == false);
    ret = ng_addMethodToService(&FirstTestService_service, &FirstTestService_FirstTestMethod_method);
    TEST(ret == false);
    /* And lets check once more if pointers remain the same */
    TEST(FirstTestService_service.method == &FirstTestService_SecondTestMethod_method);
    TEST(FirstTestService_service.method->next == &FirstTestService_FirstTestMethod_method);
    TEST(FirstTestService_service.method->next->next == NULL);

    /* This would be called in default service init */
    FirstTestService_FirstTestMethod_DefaultContext.request = (void*)&FirstTestService_FirstTestMethod_DefaultRequest;
    FirstTestService_FirstTestMethod_DefaultContext.response = (void*)&FirstTestService_FirstTestMethod_DefaultResponse;
    FirstTestService_SecondTestMethod_DefaultContext.request = (void*)&FirstTestService_SecondTestMethod_DefaultRequest;
    FirstTestService_SecondTestMethod_DefaultContext.response = (void*)&FirstTestService_SecondTestMethod_DefaultResponse;

    /* Set default context for first method */
    ret = ng_setMethodContext(&FirstTestService_FirstTestMethod_method, &FirstTestService_FirstTestMethod_DefaultContext);
    TEST(ret == true);
    TEST(FirstTestService_FirstTestMethod_method.context == &FirstTestService_FirstTestMethod_DefaultContext);
    TEST(FirstTestService_FirstTestMethod_method.context->next == NULL);
    /* We shouldn't be able to add the same context one more time */
    ret = ng_setMethodContext(&FirstTestService_FirstTestMethod_method, &FirstTestService_FirstTestMethod_DefaultContext);
    TEST(ret == false);
    /* And pointers should remain the same */
    TEST(FirstTestService_FirstTestMethod_method.context == &FirstTestService_FirstTestMethod_DefaultContext);
    TEST(FirstTestService_FirstTestMethod_method.context->next == NULL);
    /* This is repetition of test above, but we have to set it anyway so lets test it. */
    ret = ng_setMethodContext(&FirstTestService_SecondTestMethod_method, &FirstTestService_SecondTestMethod_DefaultContext);
    TEST(ret == true);

    /* Test getting context from method */
    TEST(ng_getValidContext(&hGrpc, &FirstTestService_FirstTestMethod_method) == &FirstTestService_FirstTestMethod_DefaultContext);









    /* For second service we call generated init */
    SecondTestService_service_init();
    /* Test registering services in grpc handle */
    memset(&hGrpc, 0, sizeof(ng_grpc_handle_t));
    ret = ng_GrpcRegisterService(&hGrpc, &FirstTestService_service);
    TEST(ret == true);
    TEST(hGrpc.serviceHolder == &FirstTestService_service);
    TEST(hGrpc.serviceHolder->next == NULL);
    ret = ng_GrpcRegisterService(&hGrpc, &SecondTestService_service);
    TEST(ret == true);
    TEST(hGrpc.serviceHolder == &SecondTestService_service);
    TEST(hGrpc.serviceHolder->next == &FirstTestService_service);
    TEST(hGrpc.serviceHolder->next->next == NULL);
    /* We shouldn't be able to register those services again (in changed order)*/
    ret = ng_GrpcRegisterService(&hGrpc, &SecondTestService_service);
    TEST(ret == false);
    ret = ng_GrpcRegisterService(&hGrpc, &FirstTestService_service);
    TEST(ret == false);
    /* And pointers shouldn't change */
    TEST(hGrpc.serviceHolder == &SecondTestService_service);
    TEST(hGrpc.serviceHolder->next == &FirstTestService_service);
    TEST(hGrpc.serviceHolder->next->next == NULL);

    /* test getting method by path */
    /* Valid path */
    TEST(getMethodByPath(&hGrpc, "/FirstTestService/FirstTestMethod") == &FirstTestService_FirstTestMethod_method);
    TEST(getMethodByPath(&hGrpc, "/FirstTestService/SecondTestMethod") == &FirstTestService_SecondTestMethod_method);
    /* And some invalid paths */
    TEST(getMethodByPath(&hGrpc, "/FirstTestService//FirstTestMethod") == NULL);
    TEST(getMethodByPath(&hGrpc, "//FirstTestService//FirstTestMethod") == NULL);
    TEST(getMethodByPath(&hGrpc, "//FirstTestMethod") == NULL);
    TEST(getMethodByPath(&hGrpc, "/FirstTestService/") == NULL);
    TEST(getMethodByPath(&hGrpc, "/FirstTestService") == NULL);
    TEST(getMethodByPath(&hGrpc, "FirstTestService/FirstTestMethod") == NULL);
    TEST(getMethodByPath(&hGrpc, "FirstTestService//FirstTestMethod") == NULL);
    TEST(getMethodByPath(&hGrpc, "/FirstTestService/FirstTestMethod/") == NULL);
    TEST(getMethodByPath(&hGrpc, "/FirstTestService/FirstTestMethod//") == NULL);
    TEST(getMethodByPath(&hGrpc, "//FirstTestService//FirstTestMethod//") == NULL);
    TEST(getMethodByPath(&hGrpc, "/FirstTestServic/FirstTestMethod") == NULL);
    TEST(getMethodByPath(&hGrpc, "/FirstTestService/FirstTestMetho") == NULL);
    TEST(getMethodByPath(&hGrpc, "/RirstTestService/FirstTestMethod") == NULL);
    TEST(getMethodByPath(&hGrpc, "/FirstTestService/FirstTestMethou") == NULL);
    TEST(getMethodByPath(&hGrpc, "/FirstTestService/FirstTestMeth") == NULL);

    /* Test setting method callback */
    ret = ng_setMethodCallback(&FirstTestService_FirstTestMethod_method, &Dummy_methodCallback);
    TEST(ret == true);
    TEST(FirstTestService_FirstTestMethod_method.callback == &Dummy_methodCallback);


    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");

    return status;
}
