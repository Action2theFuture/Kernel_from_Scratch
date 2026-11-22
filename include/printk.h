#ifndef PRINTK_H
#define PRINTK_H

#include <stdarg.h>     /* freestanding header provided by the compiler, not libc */

/*
 * Kernel formatted output (subset of printf).
 *
 * Conversions : %c %s %d %i %u %x %X %p %%
 * Flags       : '-' (left-justify), '0' (zero padding, numbers only)
 * Width       : decimal number or '*' (taken from the arguments)
 * Precision   : '.N' or '.*' on %s (max characters printed)
 *
 * Returns the number of characters written.
 * The format attribute lets GCC check arguments at compile time (-Wformat).
 */
int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int vprintk(const char *fmt, va_list ap) __attribute__((format(printf, 1, 0)));

#endif
