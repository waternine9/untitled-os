[BITS 64]

section .helper

global DrawCall
DrawCall:
    mov rbx, 4
    mov rsi, rdi
    int 0x80
    ret

global GetMS
GetMS:
    mov rbx, 5
    mov rsi, rdi
    int 0x80
    ret

global PollScancode
PollScancode:
    mov rbx, 6
    mov rsi, rdi
    int 0x80
    ret

global _malloc
_malloc:
    mov rbx, 1
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    ret

global _free
_free:
    mov rbx, 0
    mov rsi, rdi
    int 0x80
    ret

global _StartProc
_StartProc:
    mov rbx, 2
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    ret

global _ExitProc
_ExitProc:
    mov rbx, 3
    int 0x80

global ReadFile
ReadFile:
    mov rbx, 11
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    ret

global WriteFile
WriteFile:
    mov rbx, 10
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    ret
global GetFileSize
GetFileSize:
    mov rbx, 12
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    ret
global ListFiles
ListFiles:
    mov rbx, 13
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    ret
global MakeDir 
MakeDir:
    mov rbx, 7
    mov rsi, rdi
    int 0x80
    ret
global MakeFile
MakeFile:
    mov rbx, 8
    mov rsi, rdi
    int 0x80
    ret
global RemoveFileOrDir
RemoveFileOrDir:
    mov rbx, 9
    mov rsi, rdi
    int 0x80
    ret