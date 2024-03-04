#ifndef PANIC_H
#define PANIC_H

#include "../include.h"

#define KernelPanic(Msg) _KernelPanic(__LINE__, __FILE__, Msg)

void _KernelPanic(int Line, const char* File, const char* Message);

#endif // PANIC_H