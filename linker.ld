ENTRY(start)

SECTIONS
{
    . = 1M;

    .rodata : {
        KEEP(*(.multiboot))
        *(.rodata*)
    }

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
}