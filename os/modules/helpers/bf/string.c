#include "string.h"

String StrFromCStr(const char *str)
{
    int size = 0;
    char* tmp = str;
    while (*tmp)
    {
        size++;
        tmp++;
    } 

    String out;
    out.data = malloc(size);
    out.size = size;
    memcpy(out.data, str, size);
    return out;
}

String StrFromArray(char *str, size_t size)
{
    String out;
    out.data = malloc(size);
    out.size = size;
    memcpy(out.data, str, size);
    return out;
}

String StrAppend(String x, String y)
{
    String out;
    out.data = malloc(x.size + y.size);
    memcpy(out.data, x.data, x.size);
    memcpy(out.data + x.size, y.data, y.size);
    out.size = x.size + y.size;
    return out;
}

bool StrStartsWith(String str, String startswith)
{
    if (startswith.size > str.size)
        return false;
    for (int i = 0; i < str.size; i++)
    {
        if (str.data[i] == ' ')
            return true;
        if (str.data[i] != startswith.data[i])
            return false;
    }
    return true;
}

bool StrEqualsWith(String x, String y)
{
    if (x.size != y.size)
        return false;
    for (int i = 0; i < x.size; i++)
    {
        if (x.data[i] != y.data[i])
            return false;
    }
    return true;
}

bool CStrEqualsWith(char* x, size_t xSize, char* y, size_t ySize)
{
    if (xSize != ySize) return false;
    while (xSize--)
    {
        if (x[xSize] != y[ySize]) return false;
    }
    return true;
}

bool StrHas(String str, char what)
{
    for (int I = 0;I < str.size;I++)
    {
        if (str.data[I] == what) return true;
    }
    return false;
}

String NewStr()
{
    String out;
    out.size = 0;
    out.data = malloc(256);
    return out;
}
void StrPushBack(String* str, char c)
{
    
    if ((str->size % 256) == 255)
    {
        void* new = malloc(str->size + 1 + 256);
        memcpy(new, str->data, str->size);
        free(str->data);
        str->data = new;
    }
    str->data[str->size++] = c;
}