#include "shell.h"
#include "types.h"
#include "io.h"
#include "string.h"
#include "vga.h"
#include "printk.h"
#include "keyboard.h"
#include "gdt.h"
#include "stack.h"

#define PROMPT          "kfs> "
#define PROMPT_LEN      5
/* prompt + line must fit on one row: Backspace cannot go back to the previous row */
#define LINE_MAX        (VGA_WIDTH - PROMPT_LEN - 1)

#define QEMU_SHUTDOWN_PORT  0x604   /* QEMU (PIIX4 ACPI) power-off port */
#define QEMU_SHUTDOWN_VALUE 0x2000

struct shell_line
{
    char    buf[LINE_MAX + 1];
    size_t  len;
};

struct shell_command
{
    const char  *name;
    const char  *help;
    void        (*run)(const char *args);
};

static struct shell_line g_lines[VGA_NUM_SCREENS];

static void print_prompt(void)
{
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    printk(PROMPT);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/* ------------------------------------------------------------- commands -- */

static void cmd_help(const char *args);

static void cmd_clear(const char *args)
{
    (void)args;
    vga_clear();
}

static void cmd_echo(const char *args)
{
    printk("%s\n", args);
}

static void cmd_gdt(const char *args)
{
    (void)args;
    gdt_print();
}

static void cmd_stack(const char *args)
{
    (void)args;
    print_kernel_stack(8);
}

static void cmd_reboot(const char *args)
{
    (void)args;
    printk("Rebooting...\n");
    /* wait until the 8042 input buffer is empty (status bit 1), then pulse the CPU reset line */
    while (inb(KBD_STATUS_PORT) & 0x02)
        ;
    outb(KBD_STATUS_PORT, KBD_CMD_RESET_CPU);
    halt_forever();     /* not reached if the reset worked */
}

static void cmd_halt(const char *args)
{
    (void)args;
    printk("System halted. You can close the window.\n");
    halt_forever();
}

static void cmd_shutdown(const char *args)
{
    (void)args;
    printk("Shutting down...\n");
    outw(QEMU_SHUTDOWN_PORT, QEMU_SHUTDOWN_VALUE);
    /* only reached on a machine without this port (real hardware needs ACPI) */
    printk("shutdown: not supported here, halting instead\n");
    halt_forever();
}

static const struct shell_command g_commands[] = {
    { "help",     "list the commands",                   cmd_help },
    { "clear",    "clear the current screen",            cmd_clear },
    { "echo",     "echo <text>: print the text",          cmd_echo },
    { "gdt",      "print the GDT and segment registers", cmd_gdt },
    { "stack",    "print the kernel stack",              cmd_stack },
    { "reboot",   "restart the machine",                 cmd_reboot },
    { "halt",     "stop the CPU",                        cmd_halt },
    { "shutdown", "power off (QEMU)",                    cmd_shutdown },
};

#define COMMAND_COUNT (sizeof(g_commands) / sizeof(g_commands[0]))

static void cmd_help(const char *args)
{
    (void)args;
    printk("Commands:\n");
    for (size_t i = 0; i < COMMAND_COUNT; i++)
        printk("  %-10s %s\n", g_commands[i].name, g_commands[i].help);
    printk("F1-F4: switch screen\n");
}

/* ------------------------------------------------------------ execution -- */

static void execute(char *line)
{
    char    *name;
    char    *args;

    while (*line == ' ')
        line++;
    if (*line == '\0')
        return;

    /* split "name args": name ends at the first space, args start after the spaces */
    name = line;
    args = line;
    while (*args && *args != ' ')
        args++;
    if (*args)
    {
        *args++ = '\0';
        while (*args == ' ')
            args++;
    }

    for (size_t i = 0; i < COMMAND_COUNT; i++)
    {
        if (strcmp(name, g_commands[i].name) == 0)
        {
            g_commands[i].run(args);
            return;
        }
    }
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    printk("%s: command not found (type 'help')\n", name);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void shell_init(void)
{
    size_t current = vga_active_screen();

    memset(g_lines, 0, sizeof(g_lines));
    /* every screen gets its own prompt */
    for (size_t screen = 0; screen < VGA_NUM_SCREENS; screen++)
    {
        vga_switch_screen(screen);
        print_prompt();
    }
    vga_switch_screen(current);
}

void shell_handle_char(char c)
{
    struct shell_line *line = &g_lines[vga_active_screen()];

    if (c == '\n')
    {
        vga_putchar('\n');
        line->buf[line->len] = '\0';
        execute(line->buf);
        line->len = 0;
        print_prompt();
    }
    else if (c == '\b')
    {
        if (line->len > 0)      /* never erase the prompt */
        {
            line->len--;
            vga_putchar('\b');
        }
    }
    else if (c >= ' ' && c <= '~' && line->len < LINE_MAX)
    {
        line->buf[line->len++] = c;
        vga_putchar(c);
    }
    /* other characters (Tab, ...) and characters past LINE_MAX are ignored */
}
