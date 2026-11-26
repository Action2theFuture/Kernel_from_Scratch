#include "stack.h"
#include "printk.h"
#include "vga.h"
#include "symbols.h"

#define MAX_FRAMES  16

/*
 * Stack frame layout (cdecl, with frame pointer), stack grows downwards:
 *
 *   higher addresses
 *   | argument n ... 1   |
 *   | return address     |  <- ebp + 4
 *   | saved ebp (caller) |  <- ebp      = start of the frame chain link
 *   | local variables    |
 *   | ...                |  <- esp
 *   lower addresses
 */
struct stack_frame
{
    struct stack_frame  *ebp;   /* caller's frame */
    uint32_t            ret;    /* return address into the caller */
};

static bool in_stack(uintptr_t addr)
{
    return addr >= (uintptr_t)stack_bottom && addr + sizeof(uint32_t) <= (uintptr_t)stack_top;
}

static void print_header(uintptr_t esp, uintptr_t ebp)
{
    uintptr_t top = (uintptr_t)stack_top;
    uintptr_t bottom = (uintptr_t)stack_bottom;

    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    printk("=== kernel stack ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    printk("top    %p  (highest address, stack grows down)\n", (void *)top);
    printk("bottom %p  size %u bytes\n", (void *)bottom, (uint32_t)(top - bottom));
    printk("esp    %p  used %u bytes\n", (void *)esp, (uint32_t)(top - esp));
    printk("ebp    %p\n", (void *)ebp);
}

/* Walk the saved-EBP chain; store frames in `frames`, return how many. */
static size_t collect_frames(uintptr_t ebp, struct stack_frame **frames)
{
    size_t n = 0;

    while (ebp != 0 && n < MAX_FRAMES && in_stack(ebp))
    {
        struct stack_frame *frame = (struct stack_frame *)ebp;

        frames[n++] = frame;
        /* the chain must go up the stack, otherwise it is corrupted */
        if ((uintptr_t)frame->ebp != 0 && (uintptr_t)frame->ebp <= ebp)
            break;
        ebp = (uintptr_t)frame->ebp;
    }
    return n;
}

static void print_trace(struct stack_frame **frames, size_t n)
{
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    printk("--- call trace (most recent first) ---\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (size_t i = 0; i < n; i++)
    {
        uint32_t    offset = 0;
        const char  *name = symbol_lookup(frames[i]->ret, &offset);

        printk("#%u  frame %p  return to %p  %s+0x%x\n", (uint32_t)i, (void *)frames[i],
               (void *)frames[i]->ret, name ? name : "??", offset);
    }
    if (n == 0)
        printk("(no frame: built without frame pointer?)\n");
}

/*
 * Dump starts at the current frame (saved ebp of print_kernel_stack) and goes up:
 * that is where the frame links and return addresses are. Below it there are only
 * this function's own locals.
 */
static void print_dump(uintptr_t esp, size_t max_words, struct stack_frame **frames, size_t n)
{
    uint32_t    *word = n > 0 ? (uint32_t *)frames[0] : (uint32_t *)esp;
    size_t      count = 0;

    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    printk("--- raw dump from %s (address: value) ---\n", n > 0 ? "current frame" : "esp");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    while (count < max_words && in_stack((uintptr_t)word))
    {
        const char *note = "";
        bool        is_return = false;

        for (size_t i = 0; i < n; i++)
        {
            if (word == (uint32_t *)frames[i])
                note = "<- saved ebp";
            else if (word == &frames[i]->ret)
            {
                note = "<- return address";
                is_return = true;
            }
        }
        if (is_return)  /* add the function the address points into */
        {
            uint32_t    offset = 0;
            const char  *name = symbol_lookup(*word, &offset);

            printk("%p: %08x  %s (%s)\n", (void *)word, *word, note, name ? name : "??");
        }
        else
            printk("%p: %08x  %s\n", (void *)word, *word, note);
        word++;
        count++;
    }
}

/* noinline: keep a real frame for this function so the trace starts from here. */
__attribute__((noinline)) void print_kernel_stack(size_t max_words)
{
    uintptr_t           esp;
    uintptr_t           ebp;
    struct stack_frame  *frames[MAX_FRAMES];
    size_t              n;

    __asm__ volatile ("mov %%esp, %0" : "=r"(esp));
    __asm__ volatile ("mov %%ebp, %0" : "=r"(ebp));

    n = collect_frames(ebp, frames);
    print_header(esp, ebp);
    print_trace(frames, n);
    print_dump(esp, max_words, frames, n);
}
