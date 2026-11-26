#ifndef STACK_H
#define STACK_H

#include "types.h"

/* Kernel stack bounds, defined in boot/boot.asm (.bss, 16 KiB). */
extern uint8_t stack_bottom[];
extern uint8_t stack_top[];

/*
 * Print the kernel stack in a human-friendly way:
 *   1. bounds and usage (top, bottom, current ESP / EBP)
 *   2. call trace, following the saved-EBP chain until EBP = 0 (set in boot.asm)
 *   3. raw dump of the first `max_words` 32-bit words from ESP, each word
 *      annotated when it is a saved EBP or a return address of a frame
 *
 * Requires -fno-omit-frame-pointer, otherwise the EBP chain does not exist.
 */
void    print_kernel_stack(size_t max_words);

#endif
