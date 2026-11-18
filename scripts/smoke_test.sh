#!/usr/bin/env bash
# Boot the ISO headless in QEMU and check that "42" is the first thing on screen.
# Works on macOS and Linux (only needs qemu-system-i386).
set -euo pipefail

ISO="${1:-kfs.iso}"
WAIT="${WAIT:-4}"   # seconds to let GRUB + kernel boot (TCG on Apple Silicon is slower)

out=$( { sleep "$WAIT"; echo "xp /2xh 0xb8000"; echo "quit"; } \
       | qemu-system-i386 -cdrom "$ISO" -display none -monitor stdio 2>/dev/null \
       | tr -d '\r' | grep -E '^0*b8000:' || true )

# VGA cells = (color << 8) | ascii -> we only compare the low byte.
c0=$(echo "$out" | awk '{print $2}')
c1=$(echo "$out" | awk '{print $3}')

if [[ "${c0: -2}" == "34" && "${c1: -2}" == "32" ]]; then
    echo "[PASS] screen starts with \"42\" ($out)"
else
    echo "[FAIL] unexpected VGA content: '${out}'"
    exit 1
fi
