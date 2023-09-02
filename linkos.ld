OUTPUT_FORMAT(binary)
SECTIONS {
    . = 0xB00000;
    .text :
    {
        *(.startos)
        *(.text)
        *(.helper)
    }
    .rodata : 
    {
        *(.rodata)
    }
    .data : 
    {
        *(.data)
    }
    . = 0xB50000;
    .bss : 
    {
        *(.bss)
    }
}
