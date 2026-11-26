#include "printk.h"
#include "types.h"
#include "vga.h"

/* Single output point: change this function to redirect printk (e.g. serial). */
static void out_char(char c, int *count)
{
    vga_putchar(c);
    (*count)++;
}

static void out_repeat(char c, int n, int *count)
{
    while (n-- > 0)
        out_char(c, count);
}

static size_t str_len(const char *s)
{
    size_t len = 0;

    while (s[len])
        len++;
    return len;
}

/* Format specification parsed from "%[flags][width][.precision]conv". */
struct spec
{
    bool    left;        /* '-' */
    bool    zero;        /* '0' */
    int     width;
    int     precision;   /* -1 = not given */
};

/* Print `len` characters of `s` padded to spec->width. */
static void out_padded(const char *s, int len, const struct spec *sp, char pad, int *count)
{
    int padding = sp->width > len ? sp->width - len : 0;

    if (!sp->left)
        out_repeat(pad, padding, count);
    for (int i = 0; i < len; i++)
        out_char(s[i], count);
    if (sp->left)
        out_repeat(' ', padding, count);
}

/*
 * Print an unsigned value in `base`, with optional sign / prefix.
 * Zero padding goes between the sign/prefix and the digits ("-0042", "0x0000002a").
 */
static void out_number(uint32_t value, unsigned base, bool upper, bool negative,
                       const char *prefix, const struct spec *sp, int *count)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char        buf[32];
    int         len = 0;
    int         prefix_len = (int)str_len(prefix) + (negative ? 1 : 0);
    int         padding;

    do
    {
        buf[len++] = digits[value % base];
        value /= base;
    } while (value);

    padding = sp->width - len - prefix_len;
    if (padding < 0)
        padding = 0;

    if (!sp->left && !sp->zero)
        out_repeat(' ', padding, count);
    if (negative)
        out_char('-', count);
    for (const char *p = prefix; *p; p++)
        out_char(*p, count);
    if (!sp->left && sp->zero)
        out_repeat('0', padding, count);
    while (len)
        out_char(buf[--len], count);
    if (sp->left)
        out_repeat(' ', padding, count);
}

int vprintk(const char *fmt, va_list ap)
{
    int count = 0;

    for (; *fmt; fmt++)
    {
        if (*fmt != '%')
        {
            out_char(*fmt, &count);
            continue;
        }

        struct spec sp = { false, false, 0, -1 };

        /* flags */
        for (fmt++; *fmt == '-' || *fmt == '0'; fmt++)
        {
            if (*fmt == '-')
                sp.left = true;
            else
                sp.zero = true;
        }
        /* width */
        if (*fmt == '*')
        {
            sp.width = va_arg(ap, int);
            if (sp.width < 0)
            {
                sp.left = true;
                sp.width = -sp.width;
            }
            fmt++;
        }
        else
            for (; *fmt >= '0' && *fmt <= '9'; fmt++)
                sp.width = sp.width * 10 + (*fmt - '0');
        /* precision (used by %s) */
        if (*fmt == '.')
        {
            fmt++;
            sp.precision = 0;
            if (*fmt == '*')
            {
                sp.precision = va_arg(ap, int);
                fmt++;
            }
            else
                for (; *fmt >= '0' && *fmt <= '9'; fmt++)
                    sp.precision = sp.precision * 10 + (*fmt - '0');
        }
        if (sp.left)
            sp.zero = false;    /* '-' overrides '0', like printf */

        switch (*fmt)
        {
            case 'c':
            {
                char c = (char)va_arg(ap, int);
                out_padded(&c, 1, &sp, ' ', &count);
                break;
            }
            case 's':
            {
                const char *s = va_arg(ap, const char *);
                int         len;

                if (!s)
                    s = "(null)";
                len = (int)str_len(s);
                if (sp.precision >= 0 && sp.precision < len)
                    len = sp.precision;
                out_padded(s, len, &sp, ' ', &count);
                break;
            }
            case 'd':
            case 'i':
            {
                int32_t  v = va_arg(ap, int32_t);
                /* negate in unsigned arithmetic: also correct for INT32_MIN */
                uint32_t magnitude = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
                out_number(magnitude, 10, false, v < 0, "", &sp, &count);
                break;
            }
            case 'u':
                out_number(va_arg(ap, uint32_t), 10, false, false, "", &sp, &count);
                break;
            case 'x':
            case 'X':
                out_number(va_arg(ap, uint32_t), 16, *fmt == 'X', false, "", &sp, &count);
                break;
            case 'p':
            {
                /* Always 8 hex digits on i386: easier to read in stack dumps. */
                struct spec ps = sp;
                if (ps.width < 10)
                {
                    ps.width = 10;
                    ps.zero = true;
                    ps.left = false;
                }
                out_number((uint32_t)(uintptr_t)va_arg(ap, void *), 16, false, false,
                           "0x", &ps, &count);
                break;
            }
            case '%':
                out_char('%', &count);
                break;
            case '\0':
                /* format ends with a lone '%': print it and stop */
                out_char('%', &count);
                return count;
            default:
                /* unknown conversion: print it verbatim so the bug is visible */
                out_char('%', &count);
                out_char(*fmt, &count);
                break;
        }
    }
    return count;
}

int printk(const char *fmt, ...)
{
    va_list ap;
    int     count;

    va_start(ap, fmt);
    count = vprintk(fmt, ap);
    va_end(ap);
    return count;
}
