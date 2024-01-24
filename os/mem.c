#include "mem.h"

extern void _malloc(void** _Ptr, uint64_t _Bytes);
extern void _free(void* _Ptr);

void* malloc(uint64_t _Bytes)
{
    void* _Ptr;
    _malloc(&_Ptr, _Bytes);
    return _Ptr;
}

void free(void* _Block)
{
    _free(_Block);
}

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