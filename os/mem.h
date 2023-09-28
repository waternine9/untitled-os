#ifndef MEM_H
#define MEM_H

#include "include.h"

void* volatile malloc(uint64_t _Bytes);
void volatile free(void* _Block);
void memcpy(void* _Dst, void* _Src, size_t _Bytes);
void memset(void* _Dst, char _Val, size_t _Bytes);

#endif // MEM_H