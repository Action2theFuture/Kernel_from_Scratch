#include "vga.h"
#include "io.h"
#include "string.h"

/* VGA text mode: each cell is 16 bits -> low byte = ASCII, high byte = color
   (low nibble = foreground, high nibble = background). */

#define VGA_CRTC_INDEX      0x3D4
#define VGA_CRTC_DATA       0x3D5
#define TAB_SIZE            4
#define VGA_TEXT_HEIGHT     (VGA_HEIGHT - 1)

struct vga_screen
{
    uint16_t    buffer[VGA_WIDTH * VGA_HEIGHT];
    size_t      row;
    size_t      column;
    uint8_t     color;
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

static void draw_paging(void)
{
    uint8_t paging_color = make_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    uint8_t active_color = make_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_CYAN);
    size_t  row_offset = VGA_TEXT_HEIGHT * VGA_WIDTH;

    for (size_t x = 0; x < VGA_WIDTH; x++)
        g_buffer_mem[row_offset + x] = make_entry(' ', paging_color);

    const char *title = " Pages";
    for (size_t i = 0; title[i]; i++)
        g_buffer_mem[row_offset + i] = make_entry(title[i], paging_color);

    /* Print [1] [2] [3] [4] */
    size_t start_x = VGA_WIDTH - (VGA_NUM_SCREENS * 4) - 1;
    for (size_t screen = 0; screen < VGA_NUM_SCREENS; screen++)
    {
        uint8_t col = (screen == g_active_screen) ? active_color : paging_color;
        size_t pos = row_offset + start_x + screen * 4;

        g_buffer_mem[pos] = make_entry(screen == g_active_screen ? '[' : ' ', col);
        g_buffer_mem[pos + 1] = make_entry((char)('1' + screen), col);
        g_buffer_mem[pos + 2] = make_entry(screen == g_active_screen ? ']' : ' ', col);
    }
}

static void refresh_screen(void)
{
    struct vga_screen   *screen = &g_screens[g_active_screen];

    draw_paging();
    for (size_t i = 0; i < VGA_WIDTH * VGA_TEXT_HEIGHT; i++)
        g_buffer_mem[i] = screen->buffer[i];
}

static void update_cursor(void)
{
    struct vga_screen   *screen = &g_screens[g_active_screen];
    uint16_t pos = (uint16_t)(screen->row * VGA_WIDTH + screen->column);

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
    struct vga_screen   *screen = &g_screens[g_active_screen];

    /* Move rows 1..24 up by one, then blank the last row. */
    for (size_t i = 0; i < (VGA_TEXT_HEIGHT - 1) * VGA_WIDTH; i++)
        screen->buffer[i] = screen->buffer[i + VGA_WIDTH];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        screen->buffer[(VGA_TEXT_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', screen->color);
    screen->row = VGA_TEXT_HEIGHT - 1;
    refresh_screen();
}

static void newline(void)
{
    struct vga_screen   *screen = &g_screens[g_active_screen];

    screen->column = 0;
    if (++screen->row == VGA_TEXT_HEIGHT)
        scroll();
}

void vga_clear(void)
{
    struct vga_screen   *screen = &g_screens[g_active_screen];

    for (size_t i = 0; i < VGA_WIDTH * VGA_TEXT_HEIGHT; i++)
        screen->buffer[i] = make_entry(' ', screen->color);
    screen->row = 0;
    screen->column = 0;
    refresh_screen();
    update_cursor();
}

void vga_init(void)
{
    uint8_t default_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (size_t screen = 0; screen < VGA_NUM_SCREENS; screen++)
    {
        g_screens[screen].row = 0;
        g_screens[screen].column = 0;
        g_screens[screen].color = default_color;
        for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
            g_screens[screen].buffer[i] = make_entry(' ', default_color);
    }
    g_active_screen = 0;
    enable_cursor(14, 15);
    refresh_screen();
    update_cursor();
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    g_screens[g_active_screen].color = make_color(fg, bg);
}

static void put_raw(char c)
{
    struct      vga_screen   *screen = &g_screens[g_active_screen];
    size_t      i;
    uint16_t    entry;

    switch (c)
    {
        case '\n':
            newline();
            break;
        case '\r':
            screen->column = 0;
            break;
        case '\t':
            do
                put_raw(' ');
            while (screen->column % TAB_SIZE);
            break;
        case '\b':
            if (screen->column > 0)
            {
                i = screen->row * VGA_WIDTH + --screen->column;
                entry = make_entry(' ', screen->color);
                screen->buffer[i] = entry;
                g_buffer_mem[i] = entry;
            }
            break;
        default:
            i = screen->row * VGA_WIDTH + screen->column;
            entry = make_entry(c, screen->color);
            screen->buffer[i] = entry;
            g_buffer_mem[i] = entry;
            if (++screen->column == VGA_WIDTH)
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
    refresh_screen();
    update_cursor();
}
