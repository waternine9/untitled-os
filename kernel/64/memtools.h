#ifndef MEMTOOLS_H
#define MEMTOOLS_H

#include "../include.h"

void memcpy(void* _Dst, void* _Src, size_t _Bytes);
void memset(void* _Dst, char _Val, size_t _Bytes);

#endif // MEMTOOLS_H