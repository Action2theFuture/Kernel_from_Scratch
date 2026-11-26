#include "types.h"
#include "io.h"
#include "vga.h"
#include "keyboard.h"
#include "printk.h"
#include "gdt.h"
#include "shell.h"
#include "banner.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Banner + help hint, drawn on every screen (F1-F4). "42" only on screen 1. */
static void draw_welcome(size_t screen, uint32_t magic)
{
    vga_switch_screen(screen);
    banner_print();
    if (screen == 0)
    {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        printk("42\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            printk("warning: not booted by a multiboot loader\n");
            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        }
    }
    printk("Type 'help' for the list of commands. F1-F4: switch screen.\n");
}

void kmain(uint32_t magic, uint32_t mbi_addr)
{
    (void)mbi_addr; /* multiboot info: memory map etc. (needed from KFS-3) */

    vga_init();
    gdt_init();     /* KFS-2: GDT at 0x800, segment registers reloaded */

    for (size_t screen = 0; screen < VGA_NUM_SCREENS; screen++)
        draw_welcome(screen, magic);
    vga_switch_screen(0);

    shell_init();   /* one prompt per screen */
    for (;;)
    {
        char c = keyboard_getchar();
        if (c)
            shell_handle_char(c);
    }
}
