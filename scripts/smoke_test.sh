#!/usr/bin/env bash
# Boot the ISO headless in QEMU and check that "42" is displayed on screen
# (a row of the VGA text buffer that starts with "42").
# Works on macOS and Linux (only needs qemu-system-i386 and awk).
set -euo pipefail

ISO="${1:-kfs.iso}"
WAIT="${WAIT:-4}"   # seconds to let GRUB + kernel boot (TCG on Apple Silicon is slower)

# Colors only on a terminal, and not with NO_COLOR=1 / TERM=dumb.
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ] && [ "${TERM:-}" != "dumb" ]; then
    C_OK=$'\e[32m'; C_FAIL=$'\e[31m'; C_RESET=$'\e[0m'
else
    C_OK=""; C_FAIL=""; C_RESET=""
fi

# Whole text buffer: 80 x 25 cells of 16 bits = 2000 halfwords at 0xB8000.
dump=$( { sleep "$WAIT"; echo "xp /2000xh 0xb8000"; echo "quit"; } \
        | qemu-system-i386 -cdrom "$ISO" -display none -monitor stdio 2>/dev/null \
        | tr -d '\r' | grep -E '^0*b8[0-9a-f]*:' || true )

# Cell = (color << 8) | ascii: keep the low byte, find the first row that
# starts with '4' (0x34), '2' (0x32) followed by a space or the end of the row.
row=$(echo "$dump" | awk '
    { for (i = 2; i <= NF; i++) cell[n++] = substr($i, length($i) - 1) }
    END {
        for (r = 0; r < 25; r++) {
            b = r * 80
            if (cell[b] == "34" && cell[b + 1] == "32" && (cell[b + 2] == "20" || cell[b + 2] == "00")) {
                print r; exit
            }
        }
        print -1
    }')

if [ "$row" -ge 0 ]; then
    echo "${C_OK}[PASS]${C_RESET} \"42\" is displayed (screen row $row)"
else
    echo "${C_FAIL}[FAIL]${C_RESET} no screen row starts with \"42\""
    exit 1
fi
