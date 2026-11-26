#include "keyboard.h"
#include "io.h"
#include "vga.h"

/* Scan code set 1 values, private to the driver (ports are in keyboard.h). */
#define SC_RELEASE_MASK 0x80 // bit 7: 0 = key press, 1 = key release
#define SC_EXTENDED                                                            \
  0xE0 // prefix of 2-byte keys (arrows, Delete, right Ctrl/Alt...)
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_CAPS_LOCK 0x3A
#define SC_F1 0x3B
#define SC_F4 0x3E

// Is the left / right Shift currently pressed ?
// Tracked separately: releasing one must not cancel the other.
static bool g_lshift_pressed = false;
static bool g_rshift_pressed = false;
// Is the caps lock locked ?
static bool g_caps_lock = false;
// Is the Caps Lock key held ? A held key repeats its make code -> toggle only
// once.
static bool g_caps_key_down = false;
// Did the previous call receive the 0xE0 prefix ? Then the following byte is
// ignored.
static bool g_extended = false;

static const char g_scancode_ascii[128] = {
    0,    0,   '1',  '2',  '3', '4', '5',  '6', '7', '8', '9',
    '0',  '-', '=',  '\b', // 0x01 = Esc -> 0: not printable
    '\t', 'q', 'w',  'e',  'r', 't', 'y',  'u', 'i', 'o', 'p',
    '[',  ']', '\n', 0,    'a', 's', 'd',  'f', 'g', 'h', 'j',
    'k',  'l', ';',  '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b',  'n', 'm',  ',',  '.', '/', 0,    '*', 0,   ' ', 0};

static const char g_scancode_ascii_shift[128] = {
    0,   0,   '!',  '@',  '#',  '$', '%', '^', '&', '*', '(', ')',
    '_', '+', '\b', '\t', 'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{',  '}',  '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H',
    'J', 'K', 'L',  ':',  '"',  '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M',  '<',  '>',  '?', 0,   '*', 0,   ' ', 0};

static bool is_alpha(char c) { return (c >= 'a' && c <= 'z'); }

char keyboard_getchar(void) {
  // No byte waiting in the controller -> no key event
  if ((inb(KBD_STATUS_PORT) & KBD_STATUS_OUT_FULL) == 0)
    return 0;

  uint8_t scancode = inb(KBD_DATA_PORT); // Read the new key (8 bits)

  if (scancode == SC_EXTENDED) {
    g_extended = true;
    return 0;
  }
  // Second byte of an extended key: ignored, and the flag is cleared for the
  // next ones
  if (g_extended) {
    g_extended = false;
    return 0;
  }

  // Split the byte: bit 7 = action (press / release), bits 0-6 = which key (0
  // to 127)
  bool released = (scancode & SC_RELEASE_MASK) != 0;
  uint8_t key = scancode & ~SC_RELEASE_MASK;

  if (key == SC_LSHIFT) {
    g_lshift_pressed = !released;
    return 0;
  }
  if (key == SC_RSHIFT) {
    g_rshift_pressed = !released;
    return 0;
  }
  if (key == SC_CAPS_LOCK) {
    // Toggle on the first make code only, ignore auto-repeat
    if (!released && !g_caps_key_down)
      g_caps_lock = !g_caps_lock;
    g_caps_key_down = !released;
    return 0;
  }
  if (key >= SC_F1 && key <= SC_F4 && !released) {
    vga_switch_screen(key - SC_F1);
    return 0;
  }

  if (released)
    return 0;

  bool shift = g_lshift_pressed || g_rshift_pressed;
  char base = g_scancode_ascii[key];
  if (base == 0)
    return 0;

  if (is_alpha(base)) {
    bool upper = shift ^ g_caps_lock; // XOR between Shift and Caps Lock
    return upper ? g_scancode_ascii_shift[key] : base;
  }

  return shift ? g_scancode_ascii_shift[key] : base;
}