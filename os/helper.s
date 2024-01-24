[BITS 64]

section .helper

global DrawCall
DrawCall:
    push rbx
    push rcx
    push rsi
    mov rbx, 4
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret

global GetMS
GetMS:
    push rbx
    push rcx
    push rsi
    mov rbx, 5
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret

global PollScancode
PollScancode:
    push rbx
    push rcx
    push rsi
    mov rbx, 6
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret

global _malloc
_malloc:
    push rbx
    push rcx
    push rsi
    mov rbx, 1
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret

global _free
_free:
    push rbx
    push rcx
    push rsi
    mov rbx, 0
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret

global _StartProc
_StartProc:
    push rbx
    push rcx
    push rsi
    mov rbx, 2
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret

global _ExitProc
_ExitProc:
    mov rbx, 3
    int 0x80

global ReadFile
ReadFile:
    push rbx
    push rcx
    push rsi
    mov rbx, 11
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret

global WriteFile
WriteFile:
    push rbx
    push rcx
    push rsi
    mov rbx, 10
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret
global GetFileSize
GetFileSize:
    push rbx
    push rcx
    push rsi
    mov rbx, 12
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret
global ListFiles
ListFiles:
    push rbx
    push rcx
    push rsi
    mov rbx, 13
    mov rcx, rsi
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret
global MakeDir 
MakeDir:
    push rbx
    push rcx
    push rsi
    mov rbx, 7
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret
global MakeFile
MakeFile:
    push rbx
    push rcx
    push rsi
    mov rbx, 8
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret
global RemoveFileOrDir
RemoveFileOrDir:
    push rbx
    push rcx
    push rsi
    mov rbx, 9
    mov rsi, rdi
    int 0x80
    pop rsi
    pop rcx
    pop rbx
    ret