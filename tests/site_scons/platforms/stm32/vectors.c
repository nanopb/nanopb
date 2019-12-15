#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void _start();
extern void* __StackTop;

static void HardFaultHandler()
{
    asm("mov r12, %0\n" "mov r0, #24\n" "bkpt 0x00ab" : : "r"(0xDEADBEEF) : "r0");
    while(1);
}

extern uint32_t __data_start__, __data_end__, __etext;

void meminit() __attribute__((noreturn, naked));
void meminit()
{
    // For some reason newlib's crt0 doesn't initialize .data, so do it here.
    uint32_t *src = &__etext;
    uint32_t *dst = &__data_start__;
    while (dst < &__data_end__)
    {
        *dst++ = *src++;
    }

    _start();
    asm("mov r12, %0\n" "mov r0, #24\n" "bkpt 0x00ab" : : "r"(0xDEADBEEF) : "r0");
}

void* const g_vector_table[] __attribute__((section(".isr_vector"))) = {
    (void*)&__StackTop,
    (void*)&meminit,
    (void*)&HardFaultHandler,
    (void*)&HardFaultHandler,
    (void*)&HardFaultHandler,
    (void*)&HardFaultHandler,
    (void*)&HardFaultHandler,
};

#ifdef __cplusplus
}
#endif
