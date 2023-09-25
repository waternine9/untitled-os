#ifndef MEM_H
#define MEM_H

#include "include.h"

void* volatile malloc(uint64_t _Bytes);
void volatile free(void* _Block);

#endif // MEM_H