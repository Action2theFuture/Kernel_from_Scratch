#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "types.h"

/* Kernel function table, generated at build time from `nm` (scripts/gen_symbols.sh). */
struct kernel_symbol
{
    uint32_t    addr;
    const char  *name;
};

extern const struct kernel_symbol   g_kernel_symbols[];
extern const size_t                 g_kernel_symbol_count;

/*
 * Find the function containing `addr`.
 * Returns its name and stores addr - function start in *offset,
 * or returns NULL if the address is not inside the kernel code.
 */
const char  *symbol_lookup(uint32_t addr, uint32_t *offset);

#endif
