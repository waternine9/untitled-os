[BITS 64]

section .text
; rcx = LBA
; rdi = Destination
; rdx = IO Port Start
global _ReadAta
_ReadAta:

    push rax
    push rdx
    push rcx
    push rbx
    push rdi
    push rsi
	and rcx, 1
    shl rcx, 4
    or rcx, 0b11100000

    mov rdi, rsi
    mov rsi, rcx
    mov rcx, rdi
	mov rbx, rdx
 
    mov rax, rsi
    and rcx, 0x0FFFFFFF
    shr rcx, 24
    or rax, rcx

    mov rdx, rbx
    add rdx, 6
    out dx, al

    mov rdx, rbx
    add rdx, 2
    mov al, 1
    out dx, al
    mov rax, rcx
    mov rdx, rbx
    add rdx, 3
    out dx, al
    mov rax, rcx
    shr rax, 8
    mov rdx, rbx
    add rdx, 4
    out dx, al
    mov rax, rcx
    shr rax, 16
    mov rdx, rbx
    add rdx, 5
    out dx, al
    mov rdx, rbx
    add rdx, 7
    mov al, 0x20
    out dx, al

    push rbx
    mov rcx, 0xFFFFFF

.wait_drq_set:
    dec rcx
    test rcx, rcx
    jz .fail

    in al, dx

    test al, 1
    jnz .fail

    test al, 1 << 5
    jnz .fail
.ready:
    pop rbx
    mov rcx, 256
    mov rdx, rbx
    rep insw

    pop rsi
    pop rdi
    pop rbx
    pop rcx
    pop rdx
    pop rax

	mov rax, 1

    ret
.fail:
    pop rbx
    mov rax, rsi
    mov rdx, rbx
    add rdx, 6
    out dx, al

    mov rdx, rbx
    mov rax, 1 << 2
    out dx, al
    in al, 0x80
    mov rax, 0
    out dx, al


    pop rsi
    pop rdi
    pop rbx
    pop rcx
    pop rdx
    pop rax

    mov rax, 0

    ret