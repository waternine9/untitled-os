#include "panic.h"

void KernelPanic(const char* Message)
{
    // Need to display the message as well
    asm volatile ("cli\nhlt");
}