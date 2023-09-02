build:
	mkdir -p bin
	gcc -m32 kernel/32/*.c -nostdlib -ffreestanding -mno-red-zone -fno-exceptions -nodefaultlibs -fno-builtin -fno-pic -fno-pie -O2 -T link32.ld -o paging32.img
	gcc -c -m64 kernel/64/*.c kernel/64/drivers/*/*.c -nostdlib -ffreestanding -mno-red-zone -fno-exceptions -nodefaultlibs -fno-builtin -fno-pic -fno-pie -mcmodel=large -fcf-protection=none -O2
	nasm -f elf64 kernel/64/idt.s -o idt_s.o
	ld -m elf_x86_64 -T link64.ld *.o -o kernel.img
	nasm -f elf64 os/helper.s -o helper.o 
	gcc -m64 os/*.c os/modules/*.c helper.o -nostdlib -ffreestanding -mno-red-zone -fno-exceptions -nodefaultlibs -fno-builtin -fno-PIC -fno-PIE -mcmodel=large -fcf-protection=none -O0 -T linkos.ld -o os.img
	nasm -f bin boot/boot32.s -o boot.img
	nasm -f bin boot/os_trampoline32.s -o boot-x86_64.img
	cat boot.img > bin/boot.img
	cat boot-x86_64.img >> bin/boot.img
	rm *.img
	rm *.o
	qemu-img resize bin/boot.img 32M
run:
	qemu-system-x86_64 -hda bin/boot.img -no-reboot -d int -m 4G