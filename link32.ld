OUTPUT_FORMAT(binary)
SECTIONS {
    . = 0x8600;
    .main32 : 
    {
        *(.main32)
        *(.text)
    }
    .rodata : 
    {
        *(.rodata)
    }
    .data : 
    {
        *(.data)
    }
    .bss : 
    {
        *(.bss)
    }
}
