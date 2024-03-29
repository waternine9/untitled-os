#ifndef FILE_H
#define FILE_H

#include "include.h"

typedef struct
{
    void* Data;

    // For read
    size_t BytesRead;
} FileResponse;

typedef struct
{
    char* Name;
    bool IsDir;  
} FileListEntry;

typedef struct
{
    FileListEntry* Data;
    size_t NumEntries;
} FileListResponse;

typedef struct
{
    void* Data;
    size_t Bytes;
} FileRequest;

#endif // FILE_H