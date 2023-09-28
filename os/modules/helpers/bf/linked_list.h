#ifndef H_LINKED_LIST
#define H_LINKED_LIST

#include <stdint.h>
#include <stddef.h>
#include "../../../mem.h"

typedef struct
{
    void* data;
    size_t elemSize;
    size_t size;
} LinkedList;

LinkedList LinkedListCreate(size_t elemSize);
void LinkedListPushBack(LinkedList* me, void* val);
void LinkedListPopBack(LinkedList* me);
void* LinkedListGet(LinkedList* me, int I);

#endif // H_LINKED_LIST