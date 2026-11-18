#include "vga.h"
#include "io.h"
#include "string.h"

/* VGA text mode: each cell is 16 bits -> low byte = ASCII, high byte = color
   (low nibble = foreground, high nibble = background). */

#define VGA_CRTC_INDEX  0x3D4
#define VGA_CRTC_DATA   0x3D5
#define TAB_SIZE        4

static volatile uint16_t *const g_buffer = (volatile uint16_t *)VGA_MEMORY;
static size_t  g_row;
static size_t  g_col;
static uint8_t g_color;

static inline uint8_t make_color(enum vga_color fg, enum vga_color bg)
{
    return (uint8_t)(fg | (bg << 4));
}

static inline uint16_t make_entry(char c, uint8_t color)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void update_cursor(void)
{
    uint16_t pos = (uint16_t)(g_row * VGA_WIDTH + g_col);

    outb(VGA_CRTC_INDEX, 0x0F);                 /* cursor location low */
    outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(VGA_CRTC_INDEX, 0x0E);                 /* cursor location high */
    outb(VGA_CRTC_DATA, (uint8_t)(pos >> 8));
}

static void enable_cursor(uint8_t start_line, uint8_t end_line)
{
    outb(VGA_CRTC_INDEX, 0x0A);
    outb(VGA_CRTC_DATA, (inb(VGA_CRTC_DATA) & 0xC0) | start_line);
    outb(VGA_CRTC_INDEX, 0x0B);
    outb(VGA_CRTC_DATA, (inb(VGA_CRTC_DATA) & 0xE0) | end_line);
}

static void scroll(void)
{
    /* Move rows 1..24 up by one, then blank the last row. */
    for (size_t i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
        g_buffer[i] = g_buffer[i + VGA_WIDTH];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        g_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', g_color);
    g_row = VGA_HEIGHT - 1;
}

static void newline(void)
{
    g_col = 0;
    if (++g_row == VGA_HEIGHT)
        scroll();
}

void vga_clear(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        g_buffer[i] = make_entry(' ', g_color);
    g_row = 0;
    g_col = 0;
    update_cursor();
}

void vga_init(void)
{
    g_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    enable_cursor(14, 15);
    vga_clear();
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    g_color = make_color(fg, bg);
}

static void put_raw(char c)
{
    switch (c)
    {
        case '\n':
            newline();
            break;
        case '\r':
            g_col = 0;
            break;
        case '\t':
            do
                put_raw(' ');
            while (g_col % TAB_SIZE);
            break;
        case '\b':
            if (g_col > 0)
                g_buffer[g_row * VGA_WIDTH + --g_col] = make_entry(' ', g_color);
            break;
        default:
            g_buffer[g_row * VGA_WIDTH + g_col] = make_entry(c, g_color);
            if (++g_col == VGA_WIDTH)
                newline();
    }
}

void vga_putchar(char c)
{
    put_raw(c);
    update_cursor();
}

void vga_write(const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        put_raw(data[i]);
    update_cursor();
}

void vga_putstr(const char *s)
{
    vga_write(s, strlen(s));
}
