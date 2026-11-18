#include "types.h"
#include "io.h"
#include "vga.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kmain(uint32_t magic, uint32_t mbi_addr)
{
    (void)mbi_addr; /* multiboot info: memory map etc. (needed from KFS-3) */

    vga_init();

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_putstr("42\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_putstr("warning: not booted by a multiboot loader\n");
    }

    halt_forever();
}
