#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

/* 8042 PS/2 controller ports (shared: keyboard driver, reboot) */
#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64     /* read: status, write: command */
#define KBD_STATUS_OUT_FULL 0x01 /* bit 0: a byte is waiting in 0x60 */
#define KBD_CMD_RESET_CPU 0xFE   /* pulse reset line (used by reboot) */

/* Returns the next ASCII character typed, or 0 if no printable key was pressed.
 */
char keyboard_getchar(void);

#endif