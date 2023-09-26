#ifndef FILECALL_H
#define FILECALL_H

#include "../kernel/file.h"

extern volatile void ReadFile(char* Dir, FileResponse* Out);
extern volatile void WriteFile(char* Dir, FileRequest* Data);
extern volatile void GetFileSize(char* Dir, size_t* Size);
extern volatile void ListFiles(char* Dir, FileListResponse* Out);
extern volatile void MakeDir(char* Dir);
extern volatile void MakeFile(char* File);
extern volatile void RemoveFileOrDir(char* Any);

#endif // FILECALL_H