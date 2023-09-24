; Directives:
[BITS 16]
[ORG 0x7C00]

section .boot

global bootentry
bootentry: jmp entry0

db "UNTITLED_OS"

entry0:
    jmp 0:entry
; Entry point:
entry:

; Setup segments and stack pointer:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov ax, 0x7C00
    mov sp, ax
    
    
; First things first, store the drive number
    or dl, 0x80
    mov [DriveNumber], dl

    mov si, DAPACK		; address of "disk address packet"
    mov ah, 0x42		; AL is unused
    mov dl, [DriveNumber]
    int 0x13


    ; Interrogate BIOS for MemoryMap:
; ...prepare the first call:
    xor ebx, ebx
    mov es, bx
    sub sp, 20 ; Make space for the temporary structure on the stack.
    mov bp, sp
    mov di, MemoryMap
; ...repeatedly torment the BIOS until all information has been extracted:
    .interrogation_loop:
    ; ...intimidatingly ask the BIOS for the location of memory:
        push di
        mov di, bp
        mov eax, 0x0000E820 ; Magic 0.
        mov ecx, 20 ; Buffer size.
        mov edx, 0x534D4150 ; Magic 1.
        int 0x15
        pop di
    ; ...if unable to answer, conclude the interrogation:
        jc .interrogation_done
        cmp eax, 0x534D4150 ; Make sure EAX has the magic number given to EDX.
        jne .interrogation_done
    ; ...if the BIOS gave a valid answer, write it down before continuing extraction of information:
        mov si, bp
        mov eax, [si + 16] ; Make sure the answer is valid.
        cmp eax, 1
        jne .invalid_answer
        mov cx, 8 ; Now write it down.
        rep movsw
        inc dword [MemoryMapSize]
        cmp di, MemoryMap.end ; Make sure we have space in the buffer.
        jae .interrogation_done
    .invalid_answer:
    ; ...check if this was the last answer the BIOS could provide:
        test ebx, ebx
        jnz .interrogation_loop ; If the BIOS shows signs of still knowing something, continue the suffering.
.interrogation_done:
; ...clean up afterwards:
    add sp, 20

;   SSE support
    mov eax, cr4
    or ax, 3 << 9		;set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    mov cr4, eax

    jmp VbeSetup
%include "boot/vbe_setup.s"
VbeSetup:
    call VesaVbeSetup

; Load a global descriptor table for flat addressing:
    cli
    lgdt [GDTDesc]
    mov   eax, cr0
    or    eax, 1
    mov   cr0, eax
    jmp 8:kstart

align 4
DAPACK:
db	0x10
db	0
.cnt:	dw  16		; int 13 resets this to # of blocks actually read/written
.dest:	dw	0x8400		; memory buffer destination address (0:7c00)
dw	0		; in memory page zero
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
GDTEnd:

GDTDesc:
    .GDTSize dw GDTEnd - GDTStart - 1 ; GDT size 
    .GDTAddr dd GDTStart          ; GDT address

times 0x1BE-($-$$) db 0

db 0x80, 0, 0, 0, 0, 0, 0, 0
dd 0
dd 0xFFFFFFFF

; Boot signature:
times 510 - ($ - $$) db 0x0
dw 0xAA55
DriveNumber: db 0

MemoryMapSize: dd 0
MemoryMap:
times 1024 - ($ - $$) db 0
.end:

%include "boot/vbe_strucs.s"

times 2048 - ($ - $$) db 0x0
kstart: