# KFS — Kernel From Scratch (i386, C)

A minimal x86 (i386) kernel booted by GRUB through the Multiboot (v1) protocol.
It currently prints `42` on the screen through its own VGA text-mode driver.

Goal of the team: **KFS-1 → KFS-2**, deadline **2026-12-17**. See [`docs/ROADMAP.md`](docs/ROADMAP.md).

---

## Table of contents

1. [Key idea: host ≠ target](#1-key-idea-host--target)
2. [Setup (pick your machine)](#2-setup-pick-your-machine)
3. [Quick start](#3-quick-start)
4. [Make targets](#4-make-targets)
5. [Project layout](#5-project-layout)
6. [How the kernel boots](#6-how-the-kernel-boots)
7. [Team workflow](#7-team-workflow)
8. [Submission checklist](#8-submission-checklist)
9. [References](#9-references)

---

## 1. Key idea: host ≠ target

The kernel is **always built as an i386 ELF** and **always runs inside QEMU**, which emulates a whole
x86 PC. Only the *toolchain* that produces the i386 code changes with the machine you work on —
the `Makefile` picks it automatically.

| Host | Compiler / linker | GRUB tool | Notes |
|---|---|---|---|
| macOS (Apple Silicon or Intel) | `i686-elf-gcc` / `i686-elf-ld` | `i686-elf-grub-mkrescue` | QEMU emulates x86 in software (no KVM) |
| Linux x86_64 (42 cluster, PC, WSL2) | `gcc -m32` / `ld -m elf_i386` | `grub2-mkrescue` (Fedora) / `grub-mkrescue` (Debian, Ubuntu) | `gcc-multilib` is **not** needed |
| Docker (any OS) | `gcc -m32` inside Fedora 44 | `grub2-mkrescue` | Same distribution as the 42 cluster and CI |

> Why can't we just run `./kfs.bin`? It is a kernel, not a program: it must own the whole machine
> (writes to VGA memory at `0xB8000`, uses I/O ports and privileged instructions like `cli`/`hlt`).
> A normal OS forbids that, so we boot it in a virtual PC (QEMU) instead.

---

## 2. Setup (pick your machine)

Run `make info` at the end of any setup: every line must show a path, none may say `MISSING`.

### macOS (Homebrew)

```sh
brew install i686-elf-gcc i686-elf-binutils i686-elf-grub nasm xorriso qemu
```

### 42 cluster (Fedora 44)

Nothing to install: `gcc`, `ld`, `nasm`, `xorriso`, `qemu-system-i386`, `grub2-mkrescue`,
`grub2-file` and the i386-pc GRUB modules are already there. Just run `make info && make test`.

> Fedora names the GRUB tools `grub2-*` (not `grub-*`); the Makefile looks for both.

### Linux x86_64 with sudo

```sh
# Fedora
sudo dnf install make gcc binutils nasm grub2-tools grub2-tools-extra grub2-pc-modules xorriso xz qemu-system-x86-core
# Debian / Ubuntu
sudo apt install build-essential nasm grub-pc-bin grub-common xorriso xz-utils qemu-system-x86
```

### Linux without sudo and without GRUB

Install GRUB into your home directory (downloads the distro packages and extracts them, done once):

```sh
./scripts/install_grub_local.sh     # installs into ~/.local/grub, detected by the Makefile
```

### Windows

Install WSL2 with Ubuntu (`wsl --install` in an admin PowerShell), then follow
*Linux x86_64 with sudo* inside the Ubuntu terminal.

### Docker (optional, any OS)

Only needs Docker running (`docker info` must succeed; on macOS start Docker Desktop with
`open -a Docker`). Nothing else to install. Use it to reproduce the cluster / CI environment:

```sh
make docker-image    # (re)build the Fedora 44 image, needed after every Dockerfile change
make docker-test     # rebuild from scratch in Fedora 44 + boot test
```

On Apple Silicon, enable *Settings → General → Use Rosetta for x86_64/amd64 emulation* in
Docker Desktop for faster builds.

---

## 3. Quick start

```sh
git clone <repo-url> kfs && cd kfs
make info      # check the toolchain
make           # build kfs.bin (checks it is multiboot compliant)
make test      # build kfs.iso, boot it headless, check that "42" is on screen
make run       # open a QEMU window and boot through GRUB
```

Expected output of `make test`:

```
[ok] kfs.bin is multiboot compliant
[ok] kfs.iso : 604K          # size depends on the GRUB version (624K on Fedora 44)
[PASS] screen starts with "42" (...b8000: 0x0a34 0x0a32)
```

---

## 4. Make targets

| Target | Description |
|---|---|
| `make` / `make all` | Build `kfs.bin` (kernel ELF) |
| `make iso` | Build `kfs.iso` (GRUB bootable image, BIOS only, xz compressed when `xz` exists, ~0.6 MB) |
| `make run` | Boot the ISO in QEMU. Add `KVM=1` on Linux hosts with `/dev/kvm` |
| `make run-kernel` | Boot `kfs.bin` directly with `qemu -kernel` (no GRUB, fastest dev loop) |
| `make test` | Headless boot + check VGA memory. Use `WAIT=10 make test` if the boot is slow |
| `make debug` | Start QEMU frozen with a gdb server on port 1234 |
| `make info` | Print the toolchain actually used (share this when debugging setups) |
| `make docker-<target>` | Run `make fclean && make <target>` inside the Docker image (e.g. `docker-test`) |
| `make clean` / `fclean` / `re` | Remove objects / everything / rebuild |

Debugging with gdb:

```sh
make debug                                   # terminal 1
gdb kfs.bin -ex 'target remote :1234' \
            -ex 'break kmain' -ex 'continue' # terminal 2 (use i686-elf-gdb or lldb on macOS)
```

---

## 5. Project layout

```
boot/boot.asm          Multiboot header, 16 KiB stack, calls kmain(magic, mbi)
linker.ld              Our own linker script: kernel loaded at 2 MiB (0x800 kept free for the KFS-2 GDT)
grub/grub.cfg          GRUB menu entry
include/types.h        Fixed-width types (static asserts fail the build if not 32-bit)
include/io.h           inb / outb / halt_forever
include/string.h       Kernel string/memory helpers
include/vga.h          VGA driver API and colors
kernel/kmain.c         Kernel entry point (prints "42")
drivers/vga.c          VGA text mode: colors, newline, tab, backspace, scrolling, hardware cursor
lib/string.c           strlen, strcmp, strncmp, strcpy, memset, memcpy, memmove, memcmp
scripts/smoke_test.sh  Headless boot test used by `make test`
scripts/install_grub_local.sh  No-sudo GRUB install for machines without GRUB (Fedora or Debian/Ubuntu)
Dockerfile             Reference build environment: Fedora 44 (= 42 cluster = CI)
.github/workflows/ci.yml  Build + boot test + size check on every push / PR (Fedora 44 container)
.github/workflows/branch-name-validation.yml  Rejects branches not named <type>/<name>
docs/ROADMAP.md        Weekly plan and task split
```

### Compiler flags

The subject lists C++ flags; adapted to C we use:

| Flag | Why |
|---|---|
| `-ffreestanding -nostdlib -nodefaultlibs` | No host libc / startup files: nothing from the host may be linked |
| `-fno-builtin` | Do not replace our functions with compiler built-ins |
| `-fno-stack-protector` | No `__stack_chk_fail` (does not exist in our kernel) |
| `-fcf-protection=none -fno-stack-clash-protection` | Some distro gcc builds enable these by default; disabled so every host produces the same code |
| `-m32 -fno-pie -fno-pic`, `ld -m elf_i386 -no-pie` | Only with the host gcc (Linux): force 32-bit, non-PIE output |

`-fno-exceptions` and `-fno-rtti` are C++-only and therefore not used.

---

## 6. How the kernel boots

```
BIOS ──> GRUB (from kfs.iso)
           │  reads grub.cfg: `multiboot /boot/kfs.bin`
           │  finds the Multiboot header (magic 0x1BADB002) in the first 8 KiB of kfs.bin
           │  loads the ELF segments at 2 MiB, switches to 32-bit protected mode (paging off)
           ▼
_start (boot/boot.asm)
           │  cli, set up the stack, ebp = 0
           │  push ebx (multiboot info), push eax (0x2BADB002)
           ▼
kmain(magic, mbi) (kernel/kmain.c)
           │  vga_init() → print "42" → halt
```

The VGA text buffer is at `0xB8000`: 80×25 cells of 16 bits
(`low byte = ASCII`, `high byte = color: background << 4 | foreground`).
`0x0A34` = light green (`0x0A`) `'4'` (`0x34`).

---

## 7. Team workflow

- **Never push to `main` directly.** Work on a branch, open a PR, the other teammate reviews.
- **Branch names:** `<type>/<short-name>` with type in `feature`, `hotfix`, `bugfix`, `refac`, `wip`,
  `build`, `style`, `chore` (e.g. `feature/printk`). Other names fail the `Branch Naming Validation` check.
- **CI is the referee.** A PR is merged only when the GitHub Actions jobs are green
  (`build-and-boot` in Fedora 44: build, multiboot check, boot test, ISO < 10 MB; `Branch Naming Validation`).
  "It works on my machine" is not enough: our compilers differ (Homebrew cross-gcc on macOS vs Fedora's gcc)
  and `-Werror` is on.
- **Merge with *Squash and merge*:** one commit per PR on `main`.
- **Before pushing:** `make re && make test`.
- **Setup problem?** Paste the output of `make info` in the discussion.
- **Something only fails in CI?** Reproduce it locally with `make docker-test`.

---

## 8. Submission checklist

- [ ] `make re && make test` passes on the 42 cluster
- [ ] CI is green on the submitted commit
- [ ] Remove the `kfs.iso` line from `.gitignore` and commit the image (the subject requires it, max 10 MB)
- [ ] Both teammates can explain: Multiboot header and magic values, `_start`, the linker script,
      why the kernel is loaded at 2 MiB, the VGA cell format, the Makefile rules

---

## 9. References

- OSDev Wiki — [Bare Bones](https://wiki.osdev.org/Bare_Bones), [GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler), [Text Mode Cursor](https://wiki.osdev.org/Text_Mode_Cursor), [Printing To Screen](https://wiki.osdev.org/Printing_To_Screen)
- GNU — [Multiboot Specification v0.6.96](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html), [GNU ld: Linker Scripts](https://sourceware.org/binutils/docs/ld/Scripts.html)
- QEMU — [Direct Linux / Multiboot boot (`-kernel`)](https://www.qemu.org/docs/master/system/linuxboot.html), [GDB usage](https://www.qemu.org/docs/master/system/gdb.html)
- Homebrew — [i686-elf-gcc](https://formulae.brew.sh/formula/i686-elf-gcc), [i686-elf-grub](https://formulae.brew.sh/formula/i686-elf-grub)
- Fedora — [grub2-tools-extra (grub2-mkrescue)](https://packages.fedoraproject.org/pkgs/grub2/grub2-tools-extra/), [grub2-pc-modules](https://packages.fedoraproject.org/pkgs/grub2/grub2-pc-modules/)
- GitHub — [Running jobs in a container](https://docs.github.com/en/actions/writing-workflows/choosing-where-your-workflow-runs/running-jobs-in-a-container), [Squash merging](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/about-merge-methods-on-github)
