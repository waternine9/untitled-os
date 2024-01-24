OUTPUT_FORMAT(binary)
SECTIONS {
    . = 0x8400;
    .boot : 
    {
        *(.boot)
    }
    . = 0x100000;
    .os :
    {
        *(.os)
    }
}
