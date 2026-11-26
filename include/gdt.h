#ifndef GDT_H
#define GDT_H

#include "types.h"

/* The subject requires the GDT at this physical address (paging is off: physical = linear). */
#define GDT_ADDRESS         0x00000800
#define GDT_ENTRIES         7

/* Segment selectors = index * 8 | RPL (requested privilege level, bits 0-1). */
#define GDT_KERNEL_CODE     0x08    /* index 1, ring 0 */
#define GDT_KERNEL_DATA     0x10    /* index 2, ring 0 */
#define GDT_KERNEL_STACK    0x18    /* index 3, ring 0 */
#define GDT_USER_CODE       0x23    /* index 4, ring 3 (0x20 | 3) */
#define GDT_USER_DATA       0x2B    /* index 5, ring 3 (0x28 | 3) */
#define GDT_USER_STACK      0x33    /* index 6, ring 3 (0x30 | 3) */

/* One 8-byte segment descriptor (Intel SDM Vol. 3, 3.4.5). */
struct gdt_entry
{
    uint16_t    limit_low;      /* limit bits 0-15 */
    uint16_t    base_low;       /* base bits 0-15 */
    uint8_t     base_middle;    /* base bits 16-23 */
    uint8_t     access;         /* P | DPL | S | type */
    uint8_t     granularity;    /* G | D/B | L | AVL (high nibble) + limit bits 16-19 */
    uint8_t     base_high;      /* base bits 24-31 */
} __attribute__((packed));

/* Operand of lgdt: 16-bit size - 1, then 32-bit linear address. */
struct gdt_ptr
{
    uint16_t    limit;
    uint32_t    base;
} __attribute__((packed));

_Static_assert(sizeof(struct gdt_entry) == 8, "a GDT descriptor is 8 bytes");
_Static_assert(sizeof(struct gdt_ptr) == 6, "the lgdt operand is 6 bytes");

void    gdt_init(void);
void    gdt_print(void);

/* boot/gdt_flush.asm: lgdt + reload CS (far jump) and the data/stack segment registers. */
void    gdt_flush(const struct gdt_ptr *ptr);

#endif
