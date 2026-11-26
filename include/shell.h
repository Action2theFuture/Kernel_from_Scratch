#ifndef SHELL_H
#define SHELL_H

/*
 * Minimal debug shell (KFS-2 bonus). One input line per VGA screen, so
 * switching screens (F1-F4) in the middle of a command does not mix them.
 */
void    shell_init(void);
void    shell_handle_char(char c);

#endif
