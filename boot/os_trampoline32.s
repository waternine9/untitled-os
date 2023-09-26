; Include the bootloader
section .boot

[BITS 32]
[ORG 0x8400]

kstart:
; Set up segments and stack pointer
    mov ax, 16
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
 
    mov eax, 0x7E00
    mov esp, eax

    cld

; Reprogram the PIT to be 1.193182 / 16 MHz
    cli 
    mov al, 0b00110110
    out 0x43, al
    mov al, 0
    out 0x40, al
    mov al, 16
    out 0x40, al

    mov dword [CurrentLBA], 4 + (0x2000 / 512)
    mov dword [CurrentLBAOs], 4 + (0xB000 / 512)

ReadLoop:
    mov eax, [CurrentLBA]
    cmp eax, 0x200
    jl ReadSectorsV8086

; Now call the paging initializer
    call 0x8800

    ; Enable PAE
    mov edx, cr4
    or  edx, (1 << 5)
    mov cr4, edx
    
    ; Set LME (long mode enable)
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr
    
    mov eax, 0x10000000
    mov cr3, eax
    
    ; Enable paging
    mov ebx, cr0
    or ebx, (1 << 31)
    mov cr0, ebx

    jmp 0x18:.fabled_64

    ; Don't proceed to the fabled 64-bit land before loading the 64-bit GDT
[BITS 64] ; The fabled 64-bit
.fabled_64:
    mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
    mov rsp, 0xFFFFFFFFC0200000

    
; Now call the kernel
    call 0xFFFFFFFFC0000000
; Flush the pages
    mov rbx, 0x10000000
    mov cr3, rbx
    

; Now call the operating system
    mov rcx, rax
    mov rdx, rcx

    add rdx, 0x200000
    mov word [GDTTSS], 0x67
    mov dword [GDTTSS + 2], TSS
    or dword [GDTTSS + 2], (0b10001001) << 24

    mov ax, 0x30
    ltr ax


    mov dword [TSS + 0x4], esp
    mov dword [TSS + 0x8], 0xFFFFFFFF

	mov ax, 0x28 | 3 ; ring 3 data with bottom 2 bits set for ring 3
	mov ds, ax
	mov es, ax 
	mov fs, ax 
	mov gs, ax

    cli
    push 0x28 | 3
	push rdx
    pushfq
    pop rax
    or rax, 0x200
    push rax
    push 0x20 | 3
    push rcx
    mov al, 0
    mov dx, 0x21
    out dx, al
    in al, 0x80
    mov al, 0
    mov dx, 0xA1
    out dx, al
	iretq
CurrentLBA: dd 4 + (0x2000 / 512)
CurrentLBAOs: dd 4 + (0x40000 / 512)

[BITS 32]
IDT16:
    dw 0x3FF       ; Limit
    dd 0x0         ; Base
ReadSectorsV8086:
    cli
    lgdt [GDT16Desc]
    jmp 8:.PMode16
[BITS 16]
.PMode16:
    mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

    mov eax, cr0
    and eax, ~1
    mov cr0, eax

    jmp 0:.RealMode
.RealMode:
    mov ax, 0
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax
    xor esp, esp
    mov sp, 0x7E00

    sti

    mov word [DAPACK.cnt], 4
    mov word [DAPACK.dest], 0x8400 + 4096
    mov eax, [CurrentLBA]
    mov [DAPACK.lba], eax
    mov si, DAPACK		; address of "disk address packet"
    mov ah, 0x42		; AL is unused
    mov dl, [0x7E00]
    int 0x13

    mov word [DAPACK.cnt], 4
    mov word [DAPACK.dest], 0x8400 + 4096 + 2048
    mov eax, [CurrentLBAOs]
    mov [DAPACK.lba], eax
    mov si, DAPACK		; address of "disk address packet"
    mov ah, 0x42		; AL is unused
    mov dl, [0x7E00]
    int 0x13
;   Switch back to pmode
    cli
    lgdt [GDTDesc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 8:.PMode
[BITS 32]
.PMode:
; Set up segments and stack pointer
    mov ax, 16
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, 0x7E00
    mov esp, eax

    mov edi, 0x100000
    mov eax, [CurrentLBA]
    sub eax, 4 + (0x2000 / 512)
    imul eax, 512
    add edi, eax
    mov esi, 0x8400 + 4096
    mov ecx, (512 * 4) / 2
    rep movsw
    mov eax, [CurrentLBA]
    add eax, 4
    mov [CurrentLBA], eax

    mov edi, 0xB00000
    mov eax, [CurrentLBAOs]
    sub eax, 4 + (0x40000 / 512)
    imul eax, 512
    add edi, eax
    mov esi, 0x8400 + 4096 + 2048
    mov ecx, (512 * 4) / 2
    rep movsw
    mov eax, [CurrentLBAOs]
    add eax, 4
    mov [CurrentLBAOs], eax

    jmp ReadLoop

align 4
DAPACK:
db	0x10
db	0
.cnt:	dw  16		; int 13 resets this to # of blocks actually read/written
.dest:	dw	0x8400		; memory buffer destination address (0:7c00)
dw 0
.lba:	dd  4		; put the lba to read in this spot
dd	0		; more storage bytes only for big lba's ( > 4 bytes )

GDTStart:
    dq 0 
GDTCode:
    dw 0xFFFF     ; Limit
    dw 0x0000     ; Base
    db 0x00       ; Base
    db 0b10011010 ; Access
    db 0b11001111 ; Flags + Limit
    db 0x00       ; Base
GDTData:
    dw 0xFFFF     ; Limit
    dw 0x0000     ; Base
    db 0x00       ; Base
    db 0b10010010 ; Access
    db 0b11001111 ; Flags + Limit
    db 0x00       ; Base
GDTCode64:
    dw 0xFFFF     ; Limit
    dw 0x0000     ; Base
    db 0x00       ; Base
    db 0b10011010 ; Access
    db 0b10101111 ; Flags + Limit
    db 0x00       ; Base
GDTCode64User:
    dw 0xFFFF     ; Limit
    dw 0x0000     ; Base
    db 0x00       ; Base
    db 0b11111010 ; Access
    db 0b10101111 ; Flags + Limit
    db 0x00       ; Base
GDTDataUser:
    dw 0xFFFF     ; Limit
    dw 0x0000     ; Base
    db 0x00       ; Base
    db 0b11110010 ; Access
    db 0b11001111 ; Flags + Limit
    db 0x00       ; Base
GDTTSS:
    dw 0x67     ; Limit
    dw 0x0     ; Base
    db 0x00       ; Base
    db (0b10001001) ; Access
    db 0b00000000 ; Flags + Limit
    db 0x00       ; Base
    dq 0x0        ; Stuff
GDTEnd:

GDTDesc:
    .GDTSize dw GDTEnd - GDTStart - 1 ; GDT size 
    .GDTAddr dd GDTStart          ; GDT address

GDT16Start:
    dq 0
GDT16Code: ; 16-bit code segment descriptor
    dw 0xFFFF     ; Limit
    dw 0x0000     ; Base
    db 0x00       ; Base
    db 0b10011010 ; Access
    db 0b00001111 ; Flags + Limit
    db 0x00       ; Base

GDT16Data: ; 16-bit data segment descriptor
    dw 0xFFFF     ; Limit
    dw 0x0000     ; Base
    db 0x00       ; Base
    db 0b10010010 ; Access
    db 0b11001111 ; Flags + Limit
    db 0x00       ; Base
GDT16End:

GDT16Desc:
    .GDTSize dw GDT16End - GDT16Start - 1
    .GDTAddr dd GDT16Start

TSS:
    times 0x1A dd 0

times 1024 - ($ - $$) db 0

incbin "paging32.img"

times 4096 - ($ - $$) db 0

times 0x2000 - ($ - $$) db 0

incbin "kernel.img"

times 0x40000 - ($ - $$) db 0

[BITS 64]

section .os

incbin "os.img"

section .kernelend
kernelend: