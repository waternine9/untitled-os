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