#include "memtools.h"

void memcpy(void* _Dst, void* _Src, size_t _Bytes)
{
    for (size_t i = 0;i < _Bytes;i++)
    {
        ((char*)_Dst)[i] = ((char*)_Src)[i];
    }
}

void memset(void* _Dst, char _Val, size_t _Bytes)
{
    for (size_t i = 0;i < _Bytes;i++)
    {
        ((char*)_Dst)[i] = _Val;
    }
}