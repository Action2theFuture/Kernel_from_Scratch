#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

/* Returns the next ASCII character typed, or 0 if no printable key was pressed. */
char keyboard_getchar(void);

#endif