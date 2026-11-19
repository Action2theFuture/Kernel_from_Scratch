#include "vga.h"
#include "io.h"
#include "string.h"

/* VGA text mode: each cell is 16 bits -> low byte = ASCII, high byte = color
   (low nibble = foreground, high nibble = background). */

#define VGA_CRTC_INDEX  0x3D4
#define VGA_CRTC_DATA   0x3D5
#define TAB_SIZE        4

struct vga_screen
{
    uint16_t    buffer[VGA_WIDTH * VGA_HEIGHT];
    size_t      g_row;
    size_t      g_col;
    uint8_t     g_color;
};

static volatile uint16_t *const g_buffer_mem = (volatile uint16_t *)VGA_MEMORY;
static struct vga_screen        g_screens[VGA_NUM_SCREENS];
static size_t                   g_active_screen;

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
    uint16_t pos = (uint16_t)(g_screens->g_row * VGA_WIDTH + g_screens->g_col);

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
        g_buffer_mem[i] = g_buffer_mem[i + VGA_WIDTH];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        g_buffer_mem[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', g_screens->g_color);
    g_screens->g_row = VGA_HEIGHT - 1;
}

static void newline(void)
{
    g_screens->g_col = 0;
    if (++g_screens->g_row == VGA_HEIGHT)
        scroll();
}

void vga_clear(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        g_buffer_mem[i] = make_entry(' ', g_screens->g_color);
    g_screens->g_row = 0;
    g_screens->g_col = 0;
    update_cursor();
}

void vga_init(void)
{
    uint8_t default_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (size_t screen = 0; screen < VGA_NUM_SCREENS; screen++)
    {
        g_screens[screen].g_row = 0;
        g_screens[screen].g_col = 0;
        g_screens[screen].g_color = default_color;
        for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
            g_screens[screen].buffer[i] = make_entry(' ', default_color);
    }
    g_active_screen = 0;
    enable_cursor(14, 15);
    update_cursor();
    vga_clear();
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    g_screens->g_color = make_color(fg, bg);
}

static void put_raw(char c)
{
    switch (c)
    {
        case '\n':
            newline();
            break;
        case '\r':
            g_screens->g_col = 0;
            break;
        case '\t':
            do
                put_raw(' ');
            while (g_screens->g_col % TAB_SIZE);
            break;
        case '\b':
            if (g_screens->g_col > 0)
                g_buffer_mem[g_screens->g_row * VGA_WIDTH + --g_screens->g_col] = make_entry(' ', g_screens->g_color);
            break;
        default:
            g_buffer_mem[g_screens->g_row * VGA_WIDTH + g_screens->g_col] = make_entry(c, g_screens->g_color);
            if (++g_screens->g_col == VGA_WIDTH)
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

void vga_switch_screen(size_t i)
{
    if (i >= VGA_NUM_SCREENS || i == g_active_screen)
        return ;
    g_active_screen = i;
    update_cursor();
}
