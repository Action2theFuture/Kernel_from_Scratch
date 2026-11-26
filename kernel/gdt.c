#include "gdt.h"
#include "printk.h"

/*
 * Access byte (bits 7..0):  P | DPL(2) | S | E | DC | RW | A
 *   P   = present
 *   DPL = privilege level (0 = kernel, 3 = user)
 *   S   = 1 for code/data segments
 *   E   = 1 code (executable), 0 data
 *   RW  = readable (code) / writable (data)
 */
#define ACCESS_KERNEL_CODE  0x9A    /* 1001 1010: present, ring 0, code, readable */
#define ACCESS_KERNEL_DATA  0x92    /* 1001 0010: present, ring 0, data, writable */
#define ACCESS_USER_CODE    0xFA    /* 1111 1010: present, ring 3, code, readable */
#define ACCESS_USER_DATA    0xF2    /* 1111 0010: present, ring 3, data, writable */

/*
 * Flags (high nibble of the granularity byte):  G | D/B | L | AVL
 *   G   = 1: limit counted in 4 KiB pages -> limit 0xFFFFF = 4 GiB
 *   D/B = 1: 32-bit segment
 */
#define FLAGS_32BIT_4K      0xC0

/*
 * Flat model: every segment covers 0 - 4 GiB. Paging (KFS-3) will do the real
 * protection; the GDT only separates privilege levels (ring 0 / ring 3) and
 * code / data / stack roles.
 */
#define SEGMENT_BASE        0x00000000
#define SEGMENT_LIMIT       0xFFFFF

static const char *const g_segment_names[GDT_ENTRIES] = {
    "null", "kernel code", "kernel data", "kernel stack",
    "user code", "user data", "user stack"
};

static struct gdt_entry *gdt_table(void)
{
    struct gdt_entry *table = (struct gdt_entry *)GDT_ADDRESS;

    /* Hide the constant address from the optimizer: GCC >= 12 treats pointers
       into the first 4 KiB page as invalid and warns (-Warray-bounds). */
    __asm__("" : "+r"(table));
    return table;
}

static void gdt_set_entry(struct gdt_entry *entry, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags)
{
    entry->limit_low = limit & 0xFFFF;
    entry->base_low = base & 0xFFFF;
    entry->base_middle = (base >> 16) & 0xFF;
    entry->access = access;
    entry->granularity = (uint8_t)((flags & 0xF0) | ((limit >> 16) & 0x0F));
    entry->base_high = (base >> 24) & 0xFF;
}

void gdt_init(void)
{
    struct gdt_entry    *gdt = gdt_table();
    struct gdt_ptr      ptr;

    gdt_set_entry(&gdt[0], 0, 0, 0, 0);     /* mandatory null descriptor */
    gdt_set_entry(&gdt[1], SEGMENT_BASE, SEGMENT_LIMIT, ACCESS_KERNEL_CODE, FLAGS_32BIT_4K);
    gdt_set_entry(&gdt[2], SEGMENT_BASE, SEGMENT_LIMIT, ACCESS_KERNEL_DATA, FLAGS_32BIT_4K);
    gdt_set_entry(&gdt[3], SEGMENT_BASE, SEGMENT_LIMIT, ACCESS_KERNEL_DATA, FLAGS_32BIT_4K);
    gdt_set_entry(&gdt[4], SEGMENT_BASE, SEGMENT_LIMIT, ACCESS_USER_CODE, FLAGS_32BIT_4K);
    gdt_set_entry(&gdt[5], SEGMENT_BASE, SEGMENT_LIMIT, ACCESS_USER_DATA, FLAGS_32BIT_4K);
    gdt_set_entry(&gdt[6], SEGMENT_BASE, SEGMENT_LIMIT, ACCESS_USER_DATA, FLAGS_32BIT_4K);

    ptr.limit = (uint16_t)(sizeof(struct gdt_entry) * GDT_ENTRIES - 1);
    ptr.base = GDT_ADDRESS;
    gdt_flush(&ptr);
}

/* Human-friendly dump of the live GDT (read back through GDTR with sgdt). */
void gdt_print(void)
{
    struct gdt_ptr      ptr;
    struct gdt_entry    *gdt = gdt_table();
    uint16_t            cs, ds, ss;

    __asm__ volatile ("sgdt %0" : "=m"(ptr));
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));

    printk("GDTR base=%p limit=%u (%u entries)  CS=%02x DS=%02x SS=%02x\n",
           (void *)ptr.base, ptr.limit, (ptr.limit + 1u) / 8u, cs, ds, ss);
    printk("sel  name          base       limit  access  flags  ring\n");
    for (uint32_t i = 0; i < GDT_ENTRIES; i++)
    {
        struct gdt_entry *e = &gdt[i];
        uint32_t base = e->base_low | ((uint32_t)e->base_middle << 16)
                        | ((uint32_t)e->base_high << 24);
        uint32_t limit = e->limit_low | ((uint32_t)(e->granularity & 0x0F) << 16);

        printk("%02x   %-12s  %08x   %05x  %02x      %x      %u\n",
               i * 8, g_segment_names[i], base, limit, e->access,
               e->granularity >> 4, (e->access >> 5) & 3u);
    }
}
