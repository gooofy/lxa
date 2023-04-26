/*
 * special linker script that generates LXA ROMs
 *
 */

MEMORY
{
    ram (rwx) : ORIGIN = 0x00000400, LENGTH = 0xc00   /*  63 kB RAM at 0x400 (right behind the exception vectors) - 0x0400-0x0ffff */
    rom (rx)  : ORIGIN = 0x00FC0000, LENGTH = 0x40000 /* 256 kB system ROM at 0x00fc0000-0x00ffffff */
    /*invalid (rwx) : org = 0xdead0000 , l = 0x100*/
}

OUTPUT_FORMAT("amiga")
/*OUTPUT_FORMAT(binary)*/
OUTPUT_ARCH(m68k)

/*SEARCH_DIR("/opt/amiga/m68k-amigaos/lib");*/

SECTIONS
{
    .text   :
        {
            *(.text*)
            *(.rodata*)
            __data_start__ = .;
            *(.data*)
            __data_end__ = .;
        } > rom
    .bss    :
        {
            __bss_start__ = .;
            *(.bss*)
            __bss_end__ = .;
        } > ram
}

/*
SECTIONS
{
    .text   :
        {
            *(.text*)
        } > rom
    .rodata :
        {
            *(.rodata*)
        } > rom
    __data_rom_start__ = .;
    .data   :
        {
            __data_start__ = .;
            *(.data*)
            __data_end__ = .;
        } > rom
    .bss    :
        {
            __bss_start__ = .;
            *(.bss*)
            __bss_end__ = .;
        } > ram
}
*/

