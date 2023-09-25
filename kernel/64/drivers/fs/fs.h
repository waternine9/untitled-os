#ifndef FS_H
#define FS_H

#include "../../../include.h"
#include "../../../file.h"

void FSTryFormat();
bool FSMkdir(char* Dir);
bool FSMkfile(char* File);
bool FSRemove(char* AnyDir);
bool FSWriteFile(char* File, void* Data);
void* FSReadFile(char* File, size_t* BytesRead);
size_t FSFileSize(char* File);
FileListEntry* FSListFiles(char* Dir, size_t* NumFiles);

#endif // FS_H