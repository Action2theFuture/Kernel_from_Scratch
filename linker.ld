/* linker.ld - Our own linker script (the host's default .ld must NOT be used). */

ENTRY(_start)

SECTIONS
{
    /* Load the kernel at 2 MiB: stays clear of BIOS/VGA areas in low memory
       and keeps 0x800 free for the GDT required by KFS-2. */
    . = 2M;

    kernel_start = .;

    .text BLOCK(4K) : ALIGN(4K)
    {
        KEEP(*(.multiboot))     /* must be in the first 8 KiB of the file */
        *(.text .text.*)
        kernel_text_end = .;
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data .data.*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    kernel_end = .;

    /* Host-toolchain leftovers we never want in the kernel image. */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.property)
        *(.note.GNU-stack)
    }
}
