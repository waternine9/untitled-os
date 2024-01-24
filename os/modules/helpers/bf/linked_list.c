#include "linked_list.h"

LinkedList LinkedListCreate(size_t elemSize)
{
    LinkedList list;
    list.data = malloc(elemSize * 256);
    list.elemSize = elemSize;
    list.size = 0;
    return list;
}

void LinkedListPushBack(LinkedList* me, void* val)
{
    if ((me->size % 256) == 255)
    {
        void* new = malloc(me->size * me->elemSize + 1 + 256);
        memcpy(new, me->data, me->size * me->elemSize);
        free(me->data);
        me->data = new;
    }
    memcpy(me->data + me->elemSize * (me->size++), val, me->elemSize);
}

void LinkedListPopBack(LinkedList* me)
{
    if (me->size == 0)
    {
        return;
    }
    me->size--;
}

void* LinkedListGet(LinkedList* me, int I)
{
    return me->data + me->elemSize * I;
}