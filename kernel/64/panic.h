#ifndef PANIC_H
#define PANIC_H

#include "../include.h"

#define KernelPanic(...) _KernelPanic(__LINE__, __FILE__, __VA_ARGS__)

void _KernelPanic(int Line, const char* File, const char* Message, ...);

#endif // PANIC_H