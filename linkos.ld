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
    start_of_sections = .;
    .bss : 
    {
        *(.bss)
    }
    end_of_sections = .;

    ASSERT((end_of_sections - start_of_sections) <= 0x50000, "Size of bss exceeds the limit")
}
