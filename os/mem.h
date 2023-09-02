#ifndef MEM_H
#define MEM_H

#include "include.h"

void* malloc(uint64_t _Bytes);
void free(void* _Block);

#endif // MEM_H