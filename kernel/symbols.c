#include "symbols.h"

extern uint8_t kernel_text_end[];   /* linker.ld: end of .text */

const char *symbol_lookup(uint32_t addr, uint32_t *offset)
{
    const struct kernel_symbol *best = NULL;

    if (addr >= (uint32_t)(uintptr_t)kernel_text_end)
        return NULL;
    /* table is sorted by address: keep the last symbol starting at or before addr */
    for (size_t i = 0; i < g_kernel_symbol_count; i++)
    {
        if (g_kernel_symbols[i].addr > addr)
            break;
        best = &g_kernel_symbols[i];
    }
    if (!best)
        return NULL;
    *offset = addr - best->addr;
    return best->name;
}
