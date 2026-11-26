#!/usr/bin/env bash
# Boot the ISO headless and check the KFS-2 GDT requirements from the CPU state:
#   - GDTR base = 0x00000800, limit = 7 * 8 - 1 = 0x37
#   - CS = 0x08 (kernel code), DS = 0x10 (kernel data), SS = 0x18 (kernel stack)
#   - the 7 descriptors in memory at 0x800 have the expected access bytes
set -euo pipefail

# Colors only on a terminal, and not with NO_COLOR=1 / TERM=dumb.
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ] && [ "${TERM:-}" != "dumb" ]; then
    C_OK=$'\e[32m'; C_FAIL=$'\e[31m'; C_RESET=$'\e[0m'
else
    C_OK=""; C_FAIL=""; C_RESET=""
fi

ISO="${1:-kfs.iso}"
WAIT="${WAIT:-4}"

out=$( { sleep "$WAIT"; echo "info registers"; echo "xp /14xw 0x800"; echo "quit"; } \
       | qemu-system-i386 -cdrom "$ISO" -display none -monitor stdio 2>/dev/null | tr -d '\r' )

fail=0
check() {   # check <description> <extended regex expected in the output>
    if echo "$out" | grep -Eq "$2"; then
        echo "${C_OK}[ok]${C_RESET}   $1"
    else
        echo "${C_FAIL}[FAIL]${C_RESET} $1"
        fail=1
    fi
}

check "GDTR base 0x800, limit 0x37"  '^GDT= +00000800 00000037'
check "CS = 0x08 kernel code ring 0" '^CS =0008 .*DPL=0 CS32'
check "DS = 0x10 kernel data"        '^DS =0010 '
check "SS = 0x18 kernel stack"       '^SS =0018 '
# descriptors as little-endian dwords: low = limit/base, high = base/access/flags
check "null descriptor"              '800: 0x00000000 0x00000000 0x0000ffff 0x00cf9a00'
check "kernel data + kernel stack"   '810: 0x0000ffff 0x00cf9[23]00 0x0000ffff 0x00cf9[23]00'
check "user code + user data (DPL3)" '820: 0x0000ffff 0x00cffa00 0x0000ffff 0x00cff[23]00'
check "user stack (DPL3)"            '830: 0x0000ffff 0x00cff[23]00'

if [ "$fail" -ne 0 ]; then
    echo "$out" | grep -E '^(GDT|CS|DS|SS) *=|^0*8[0-3]0:' || true
    exit 1
fi
echo "${C_OK}[PASS]${C_RESET} GDT at 0x800 is loaded and used by the CPU"
