[BITS 64]

extern PageFault
extern GeneralProtectionFault
extern UnknownFault
extern Syscall

section .handlers

global LoadIDT
global SyscallS
global OSStartS
global PageFaultS
global GeneralProtectionFaultS
global TSSFaultS
global UnknownFaultS

global HandlerIRQ0
global HandlerIRQ1
global HandlerIRQ2
global HandlerIRQ3
global HandlerIRQ4
global HandlerIRQ5
global HandlerIRQ6 
global HandlerIRQ7
global HandlerIRQ8
global HandlerIRQ9
global HandlerIRQ10 
global HandlerIRQ11
global HandlerIRQ12 
global HandlerIRQ13 
global HandlerIRQ14 
global HandlerIRQ15
global HandlerIVT70

extern CHandlerIRQ0
extern CHandlerIRQ1
extern CHandlerIRQ2
extern CHandlerIRQ3
extern CHandlerIRQ4
extern CHandlerIRQ5
extern CHandlerIRQ6
extern CHandlerIRQ7
extern CHandlerIRQ8
extern CHandlerIRQ9
extern CHandlerIRQ10
extern CHandlerIRQ11
extern CHandlerIRQ12
extern CHandlerIRQ13
extern CHandlerIRQ14
extern CHandlerIRQ15
extern CHandlerIVT70

%macro PUSHA64 0
    
    push rax
    mov ax, ds
    push rax

    mov ax, 0x10
    mov es, ax
    mov ds, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push r10
    push r11

%endmacro

%macro POPA64 0
    pop r11
    pop r10
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    mov es, ax
    mov ds, ax
    mov fs, ax
    mov gs, ax
    pop rax
%endmacro

%macro POPA64NOSWITCH 0
    pop r11
    pop r10
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    pop rax
%endmacro

%macro RESTORESTATE 0
    add rsp, 36

    ; Restore state
    push qword [rdi + 136 + 16 * 16 + 8]
    push qword [rdi + 48]
    push qword [rdi + 56]
    push qword [rdi + 136 + 16 * 16]
    push qword [rdi + 64]

    mov rbx, [rdi + 8]
    mov rcx, [rdi + 16]
    mov rdx, [rdi + 24]
    mov rsi, [rdi + 40]
    mov rbp, [rdi + 72]
    mov r9, [rdi + 80]
    mov r10, [rdi + 88]
    mov r11, [rdi + 96]
    mov r12, [rdi + 104]
    mov r13, [rdi + 112]
    mov r14, [rdi + 120]
    mov r15, [rdi + 128]
    movdqu xmm0, [rdi + 136]
    movdqu xmm1, [rdi + 136 + 16 * 1]
    movdqu xmm2, [rdi + 136 + 16 * 2]
    movdqu xmm3, [rdi + 136 + 16 * 3]
    movdqu xmm4, [rdi + 136 + 16 * 4]
    movdqu xmm5, [rdi + 136 + 16 * 5]
    movdqu xmm6, [rdi + 136 + 16 * 6]
    movdqu xmm7, [rdi + 136 + 16 * 7]
    movdqu xmm8, [rdi + 136 + 16 * 8]
    movdqu xmm9, [rdi + 136 + 16 * 9]
    movdqu xmm10, [rdi + 136 + 16 * 10]
    movdqu xmm11, [rdi + 136 + 16 * 11]
    movdqu xmm12, [rdi + 136 + 16 * 12]
    movdqu xmm13, [rdi + 136 + 16 * 13]
    movdqu xmm14, [rdi + 136 + 16 * 14]
    movdqu xmm15, [rdi + 136 + 16 * 15]

    mov rax, [rdi + 136 + 16 * 16 + 8]
    mov es, ax
    mov ds, ax
    mov fs, ax
    mov gs, ax

    mov rax, [rdi]
    mov rdi, [rdi + 32] ; I didn't forget you!
%endmacro

align 16

PageFaultS:
    pop r12
    PUSHA64
    mov word [0xFFFFFFFF90000000], 0x0F00 | 'P'
    mov word [0xFFFFFFFF90000002], 0x0F00 | 'A'
    mov word [0xFFFFFFFF90000004], 0x0F00 | 'G'
    mov word [0xFFFFFFFF90000006], 0x0F00 | 'E'
    cli
    hlt
    call PageFault
    POPA64
    iretq

GeneralProtectionFaultS:
    PUSHA64
    mov word [0xFFFFFFFF90000000], 0x0F00 | 'G'
    mov word [0xFFFFFFFF90000002], 0x0F00 | 'P'
    cli
    hlt
    call GeneralProtectionFault
    POPA64
    add rsp, 8
    iretq

UnknownFaultS:
    pop r12
    PUSHA64
    mov word [0xFFFFFFFF90000000], 0x0F00 | 'U'
    mov word [0xFFFFFFFF90000002], 0x0F00 | 'K'
    cli
    hlt
    call UnknownFault
    POPA64
    iretq

TSSFaultS:
    PUSHA64
    mov word [0xFFFFFFFF90000000], 0x0F00 | 'T'
    hlt
    POPA64
    iretq

SyscallS:
    PUSHA64
    mov rdi, rbx
    mov rdx, rcx
    call Syscall
    cmp rax, 0
    je .dont_switch
    mov rdi, rax

    RESTORESTATE
    
    iretq

.dont_switch:
    POPA64
.done:
    iretq

; rdx is the OSes stack pointer
; rcx is the OSes entrypoint
OSStartS:
    ; TO BE REMOVED, BETTER SOLUTION FOUND

HandlerIRQ0:
    push rax
    mov ax, 0x10
    mov es, ax
    mov ds, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    pop rax

    ; Save state
    mov [.tss], rax
    mov [.tss + 8], rbx
    mov [.tss + 16], rcx
    mov [.tss + 24], rdx
    mov [.tss + 32], rdi
    mov [.tss + 40], rsi
    mov rax, [rsp + 24]
    mov [.tss + 48], rax
    mov rax, [rsp + 16]
    mov [.tss + 56], rax
    mov rax, [rsp]
    mov [.tss + 64], rax
    mov rax, [rsp + 32]
    cmp rax, 0
    jg .done_seg
    mov rax, 0x10
.done_seg:
    mov [.tss + 136 + 16 * 16 + 8], rax
    mov rax, [rsp + 8]
    mov [.tss + 136 + 16 * 16 + 0], rax
    mov [.tss + 72], rbp
    mov [.tss + 80], r9
    mov [.tss + 88], r10
    mov [.tss + 96], r11
    mov [.tss + 104], r12
    mov [.tss + 112], r13
    mov [.tss + 120], r14
    mov [.tss + 128], r15
    movdqu [.tss + 136], xmm0
    movdqu [.tss + 136 + 16 * 1], xmm1
    movdqu [.tss + 136 + 16 * 2], xmm2
    movdqu [.tss + 136 + 16 * 3], xmm3
    movdqu [.tss + 136 + 16 * 4], xmm4
    movdqu [.tss + 136 + 16 * 5], xmm5
    movdqu [.tss + 136 + 16 * 6], xmm6
    movdqu [.tss + 136 + 16 * 7], xmm7
    movdqu [.tss + 136 + 16 * 8], xmm8
    movdqu [.tss + 136 + 16 * 9], xmm9
    movdqu [.tss + 136 + 16 * 10], xmm10
    movdqu [.tss + 136 + 16 * 11], xmm11
    movdqu [.tss + 136 + 16 * 12], xmm12
    movdqu [.tss + 136 + 16 * 13], xmm13
    movdqu [.tss + 136 + 16 * 14], xmm14
    movdqu [.tss + 136 + 16 * 15], xmm15
    
    cld
    mov rdi, .tss
    call CHandlerIRQ0
    mov rdi, rax

    RESTORESTATE
    iretq
.tss:
    times 0x400 / 8 dq 0
HandlerIRQ1:
    cld
    PUSHA64
    call CHandlerIRQ1
    POPA64
    iretq

HandlerIRQ2:
    cld
    PUSHA64
    call CHandlerIRQ2
    POPA64
    iretq

HandlerIRQ3:
    cld
    PUSHA64
    call CHandlerIRQ3
    POPA64
    iretq

HandlerIRQ4:
    cld
    PUSHA64
    call CHandlerIRQ4
    POPA64
    iretq

HandlerIRQ5:
    cld
    PUSHA64
    call CHandlerIRQ5
    POPA64
    iretq

HandlerIRQ6:
    cld
    PUSHA64
    call CHandlerIRQ6
    POPA64
    iretq

HandlerIRQ7:
    cld
    PUSHA64
    call CHandlerIRQ7
    POPA64
    iretq

HandlerIRQ8:
    cld
    PUSHA64
    call CHandlerIRQ8
    POPA64
    iretq

HandlerIRQ9:
    cld
    PUSHA64
    call CHandlerIRQ9
    POPA64
    iretq

HandlerIRQ10:
    cld
    PUSHA64
    call CHandlerIRQ10
    POPA64
    iretq

HandlerIRQ11:
    cld
    PUSHA64
    call CHandlerIRQ11
    POPA64
    iretq

HandlerIRQ12:
    cld
    PUSHA64
    call CHandlerIRQ12
    POPA64
    iretq

HandlerIRQ13:
    cld
    PUSHA64
    call CHandlerIRQ13
    POPA64
    iretq

HandlerIRQ14:
    cld
    PUSHA64
    call CHandlerIRQ14
    POPA64
    iretq

HandlerIRQ15:
    cld
    PUSHA64
    call CHandlerIRQ15
    POPA64
    iretq

HandlerIVT70:
    cld
    PUSHA64
    call CHandlerIVT70
    POPA64
    iretq

LoadIDT:
    lidt [0x7E50]
    sti
    ret