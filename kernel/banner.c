#include "banner.h"
#include "types.h"
#include "vga.h"

/*
 * "KFS" logo drawn with the VGA text-mode font (code page 437):
 *   0xDB = full block   -> letter
 *   0xB1 = medium shade -> 3D shadow going down-right (SHADOW_DEPTH steps)
 * One bitmap pixel = one character cell (VGA cells are 9x16: tall letters).
 */
#define CP437_FULL_BLOCK    '\xDB'
#define CP437_MEDIUM_SHADE  '\xB1'

#define GLYPH_ROWS      5
#define GLYPH_COLS      5
#define PIXEL_WIDTH     1       /* characters per bitmap pixel */
#define GLYPH_GAP       2       /* empty pixels between letters (>= SHADOW_DEPTH) */
#define SHADOW_DEPTH    2       /* shadow = letter shifted by (d, d) for d = 1..DEPTH */
#define LEFT_MARGIN     2       /* characters */

static const char *const g_glyphs[][GLYPH_ROWS] = {
    {   /* K */
        "X...X",
        "X..X.",
        "XXX..",
        "X..X.",
        "X...X",
    },
    {   /* F */
        "XXXXX",
        "X....",
        "XXXX.",
        "X....",
        "X....",
    },
    {   /* S */
        "XXXXX",
        "X....",
        "XXXXX",
        "....X",
        "XXXXX",
    },
};

#define GLYPH_COUNT     (sizeof(g_glyphs) / sizeof(g_glyphs[0]))
#define LOGO_COLS       (GLYPH_COUNT * (GLYPH_COLS + GLYPH_GAP) - GLYPH_GAP)
#define LOGO_ROWS       GLYPH_ROWS

/* Is logo pixel (row, col) part of a letter ? Out-of-range pixels are empty. */
static bool logo_pixel(int row, int col)
{
    int glyph = col / (GLYPH_COLS + GLYPH_GAP);
    int x = col % (GLYPH_COLS + GLYPH_GAP);

    if (row < 0 || row >= LOGO_ROWS || col < 0 || col >= (int)LOGO_COLS)
        return false;
    if (x >= GLYPH_COLS)        /* gap between two letters */
        return false;
    return g_glyphs[glyph][row][x] == 'X';
}

/* Shadow: an empty cell behind a letter pixel seen from the top-left. */
static bool in_shadow(int row, int col)
{
    for (int d = 1; d <= SHADOW_DEPTH; d++)
        if (logo_pixel(row - d, col - d))
            return true;
    return false;
}

static void put_pixel(char c)
{
    for (int i = 0; i < PIXEL_WIDTH; i++)
        vga_putchar(c);
}

void banner_print(void)
{
    vga_putchar('\n');
    for (int row = 0; row < LOGO_ROWS + SHADOW_DEPTH; row++)
    {
        for (int i = 0; i < LEFT_MARGIN; i++)
            vga_putchar(' ');
        for (int col = 0; col < (int)LOGO_COLS + SHADOW_DEPTH; col++)
        {
            if (logo_pixel(row, col))
            {
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                put_pixel(CP437_FULL_BLOCK);
            }
            else if (in_shadow(row, col))
            {
                vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
                put_pixel(CP437_MEDIUM_SHADE);
            }
            else
                put_pixel(' ');
        }
        vga_putchar('\n');
    }
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_putchar('\n');
}
