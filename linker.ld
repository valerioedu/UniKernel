ENTRY(_start);

SECTIONS
{
    . = 0x40100000;

    . = ALIGN(4096);
    _text_start = .;
    .text : {
        KEEP(*(.text.boot))
        *(.text)
    }
    . = ALIGN(4096);
    _text_end = .;

    . = ALIGN(4096);
    _rodata_start = .;
    .rodata : {
        *(.rodata)
    }
    . = ALIGN(4096);
    _rodata_end = .;

    . = ALIGN(4096);
    _data_start = .;
    .data : {
        *(.data)
        *(.data*)
    }

    .bss : {
        . = ALIGN(16);
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(.COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }
    . = ALIGN(4096);
    _data_end = .;

    . = ALIGN(16);
    . += 0x4000;
    _stack_top = .;
    _kernel_end = .;
}