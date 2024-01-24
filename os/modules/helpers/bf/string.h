#ifndef H_STRING
#define H_STRING

#include <stdint.h>
#include <stddef.h>
#include "linked_list.h"

typedef struct
{
   char* data;
   size_t size;
} String;

String NewStr();
void StrPushBack(String* str, char c);
String StrFromCStr(const char* str);
String StrFromArray(char* str, size_t size);
bool StrStartsWith(String str, String startswith);
bool StrHas(String str, char what);
bool StrEqualsWith(String x, String y);
bool CStrEqualsWith(char* x, size_t xSize, char* y, size_t ySize);
String StrAppend(String x, String y);

#endif // H_STRING