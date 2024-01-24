OUTPUT_FORMAT(binary)
SECTIONS {
    . = 0xFFFFFFFFC0000000;
    start_of_sections = .;

    .main64 :
    {
        *(.main64)
        *(.text)
        *(.handlers)
    }
    .rodata : 
    {
        *(.rodata)
    }
    .data : 
    {
        *(.data)
    }

    end_of_sections = .;

    ASSERT((end_of_sections - start_of_sections) <= 0x200000, "Size of all sections exceeds the limit")
    . = 0xFFFFFFFFC1200000;
    .bss : 
    {
        *(.bss)
    }
}
