#!/usr/bin/env bash
# Install GRUB (grub-mkrescue + i386-pc modules) into ~/.local/grub WITHOUT sudo.
# Downloads the distro packages and extracts them; nothing is installed system-wide.
#   - Fedora / RHEL : dnf download + rpm2cpio | cpio   (tool is named grub2-mkrescue)
#   - Debian/Ubuntu : apt-get download + dpkg -x       (tool is named grub-mkrescue)
set -euo pipefail

DEST="${1:-$HOME/.local/grub}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$DEST"

# Already available system-wide? Then there is nothing to do.
for t in grub-mkrescue grub2-mkrescue; do
    if command -v "$t" >/dev/null 2>&1 && \
       { [ -d /usr/lib/grub/i386-pc ] || [ -d /usr/share/grub2/i386-pc ]; }; then
        echo "[ok] $t and i386-pc modules are already installed system-wide: nothing to do"
        exit 0
    fi
done

cd "$TMP"

if command -v dnf >/dev/null 2>&1; then
    echo "[*] Fedora/RHEL: downloading GRUB packages (no root needed)"
    dnf download --destdir "$TMP" \
        grub2-tools-extra grub2-tools-minimal grub2-tools grub2-pc-modules grub2-common
    for rpm in "$TMP"/*.rpm; do
        if command -v rpm2cpio >/dev/null 2>&1 && command -v cpio >/dev/null 2>&1; then
            (cd "$DEST" && rpm2cpio "$rpm" | cpio -idm --quiet)
        elif command -v bsdtar >/dev/null 2>&1; then
            bsdtar -xf "$rpm" -C "$DEST"
        else
            echo "error: need rpm2cpio+cpio or bsdtar to extract $rpm" >&2
            exit 1
        fi
    done
    MKRESCUE="$DEST/usr/bin/grub2-mkrescue"
elif command -v apt-get >/dev/null 2>&1; then
    echo "[*] Debian/Ubuntu: downloading GRUB packages (no root needed)"
    apt-get download grub-common grub-pc-bin
    for deb in *.deb; do
        dpkg -x "$deb" "$DEST"
    done
    MKRESCUE="$DEST/usr/bin/grub-mkrescue"
else
    echo "error: unsupported distribution (no dnf or apt-get)" >&2
    exit 1
fi

"$MKRESCUE" --version
test -f "$DEST/usr/lib/grub/i386-pc/multiboot.mod" || {
    echo "error: i386-pc modules not found in $DEST/usr/lib/grub/i386-pc" >&2
    exit 1
}
echo "[ok] GRUB installed in $DEST (the Makefile detects it automatically)"
