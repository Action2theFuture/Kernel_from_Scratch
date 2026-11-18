#include "../include/keyboard.h"
#include "io.h"

#define KBD_DATA_PORT       0x60 // byte receive by the keyboard
#define KBD_STATUS_PORT     0x64 // State
#define KBD_STATUS_OUT_FULL 0x01 // When it's equal to 1, the controller is free -> ready to read a new byte

#define SC_RELEASE_MASK     0x80 // 0 = key press, 1 = key released 
#define SC_EXTENDED         0xE0 // Some keys (arrows, Delete...) = 2 bytes.
#define SC_LSHIFT           0x2A
#define SC_RSHIFT           0x36
#define SC_CAPS_LOCK        0x3A

static bool g_shift_pressed = false; // Is the Shift currently press ?
static bool g_caps_lock = false; // Is the caps lock lock ?
static bool g_extended = false; // Did the previous call receive the OxE0 prefix ? Used to indicate that the following byte should be ignored

static const char g_scancode_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0
};

static const char g_scancode_ascii_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0
};

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z');
}

char keyboard_getchar(void)
{
    if ((inb(KBD_STATUS_PORT) & KBD_STATUS_OUT_FULL) == 0) // If none key has been press
        return 0;

    uint8_t scancode = inb(KBD_DATA_PORT); // Read the new key (8 bits)

    if (scancode == SC_EXTENDED)
    {
        g_extended = true;
        return 0;
    }
    if (g_extended) // If this is true, we ignore the next key because we don't press it and set it false for the next ones 
    {
        g_extended = false;
        return 0;
    }

    // These two lines serve to split the byte in two, in order to separate the key action
    bool released = (scancode & SC_RELEASE_MASK) != 0; // The leftmost bit (bit 7) indicates the action: 0 = key pressed, 1 = key released
    uint8_t key = scancode & ~SC_RELEASE_MASK; // The other seven bits indicates which key has been manipulated (between 0 and 127)

    if (key == SC_LSHIFT || key == SC_RSHIFT)
    {
        g_shift_pressed = !released;
        return 0;
    }
    if (key == SC_CAPS_LOCK && !released)
    {
        g_caps_lock = !g_caps_lock;
        return 0;
    }

    if (released || key >= 128)
        return 0;

    char base = g_scancode_ascii[key];
    if (base == 0)
        return 0;

    if (is_alpha(base))
    {
        bool upper = g_shift_pressed ^ g_caps_lock; // OR bewteen Shift and Caps lock
        return upper ? g_scancode_ascii_shift[key] : base;
    }

    return g_shift_pressed ? g_scancode_ascii_shift[key] : base;
}