
VbeCurrentMode: dw 0
;; Set up VESA VBE with maximum Resolution.
VesaVbeSetup:

; Get controller information
push es
mov ax, 0x4F00
mov di, VbeControllerInfo
int 0x10
pop es

mov si, [VbeControllerInfo.VideoModePtr]


.loop:

mov ax, [VbeControllerInfo.VideoModePtr + 2]
mov es, ax

mov cx, [es:si] ; Our current mode number

xor ax, ax
mov es, ax

cmp cx, 0xFFFF ; Check if at end of the list
je .set_mode

mov [VbeCurrentMode], cx

add si, 2  ; Each element in the list is 2 bytes

push si

push es
; Get current mode information. 
mov ax, 0x4F01
mov di, VbeModeInfo
int 0x10
pop es

pop si

; Check if supports linear frame buffer
mov ax, [VbeModeInfo.ModeAttributes]
and ax, 0x90
cmp ax, 0x90
jne .loop

; Check if is direct color or packed pixel
mov al, [VbeModeInfo.MemoryModel]

cmp al, 0x04

je .RGB

mov al, [VbeModeInfo.MemoryModel]

cmp al, 0x06

jne .loop

.RGB:

; Check if 32 bit
mov al, [VbeModeInfo.BitsPerPixel]
cmp al, 32

jne .loop

.loop_done:

xor ebx, ebx
add bx, [VbeModeInfo.XResolution]
cmp word [VbeModeInfo.XResolution], 1920
jg .loop
add bx, [VbeModeInfo.YResolution]
cmp word [VbeModeInfo.YResolution], 1080

cmp ebx, [.BestScore]
jl .loop

mov [.BestScore], ebx
mov bx, [VbeCurrentMode]
mov [.BestMode], bx

jmp .loop

.set_mode:

cmp word [.BestMode], 0xFFFF
je .done_fail

push es
mov ax, 0x4F01
mov cx, [.BestMode]
mov di, VbeModeInfo
int 0x10
pop es

push es
mov ax, 0x4F02
mov bx, [.BestMode]
and bx, 0b111111111
or bx, 0x4000    
mov di, 0
int 0x10
pop es

.done:
ret

.BestMode: dw 0xFFFF
.BestScore: dd 0x0

.done_fail:
mov ax, 0xb800
mov es, ax
mov word [es:0], 0x0F00 | 'F'
cli
hlt