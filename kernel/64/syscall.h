#ifndef SYSCALL_H
#define SYSCALL_H

#include "../include.h"
#include "softtss.h"
uint64_t Syscall(uint64_t Code, uint64_t rsi, uint64_t Selector, SoftTSS* SaveState);

#endif // SYSCALL_H