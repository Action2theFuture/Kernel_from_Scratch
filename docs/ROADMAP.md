# ROADMAP — KFS-1 → KFS-2 (2026-11-17 → 2026-12-17)

Team: 2 people (**A**, **B**).

## Team rules

- **No direct push to `main`.** Work on a branch, open a PR, the other teammate reviews.
- **Branch names** must match `^(feature|hotfix|bugfix|refac|wip|build|style|chore)/.+`
  (enforced by `.github/workflows/branch-name-validation.yml`), e.g. `feature/printk`, `bugfix/vga-scroll`.
- **Merge condition:** CI is green (`build-and-boot` + `Branch Naming Validation`).
- **Merge method:** *Squash and merge* → one commit per PR on `main`.
- **Before pushing:** `make re && make test`.

---

## Week 1 (11/17 – 11/23) — Environment + KFS-1 mandatory part

| Owner | Task |
|---|---|
| Both | `make test` passes on each machine (macOS: Homebrew, cluster: `./scripts/install_grub_local.sh`) |
| A | Understand and document the boot flow: Multiboot header, `_start`, stack, linker script, ISO generation |
| B | Review and test the VGA driver and the kernel library (`lib/string.c`), understand the type definitions |
| Both | **Build and boot from scratch on the 42 cluster** (remove the evaluation-environment risk in week 1) |

**Done when:** each of us can explain every item of the mandatory part to the other (defense questions).

## Week 2 (11/24 – 11/30) — KFS-1 bonus

| Owner | Task |
|---|---|
| A | `printk` (`%c %s %d %u %x %p %%`), COM1 serial output (debug logs, headless tests) |
| B | PS/2 keyboard polling (ports `0x60` / `0x64`, scancode set 1 → ASCII, Shift / Caps Lock) |
| Both | Multiple screens (one buffer per screen + shortcut to switch). Scroll / cursor / colors are already done |

> The bonus is only evaluated if the mandatory part is **perfect** → keep `make test` green; never break week 1.

## Week 3 (12/1 – 12/7) — KFS-2 (GDT & stack)

| Owner | Task |
|---|---|
| A | GDT at `0x00000800`; descriptors for kernel code/data/stack and user code/data/stack; `lgdt` + far jump to reload `CS`, then reload `DS/ES/FS/GS/SS` |
| B | Kernel stack printing tool (hexdump from `esp`, call trace following the `ebp` chain — `boot.asm` sets `ebp = 0` as the end marker) |
| Both | Bonus: minimal shell (`reboot`, `halt`, `stack`, `gdt` commands) |

> Check this table against the official KFS-2 subject once we have it, and update the segment list / requirements.

## Week 4 (12/8 – 12/17) — Stabilisation & defense prep

- On the cluster: `make re && make test`, ISO size < 10 MB, remove `kfs.iso` from `.gitignore` and commit the image
- Prepare defense questions: Multiboot magic values, why the kernel is loaded at 2 MiB, VGA cell format,
  GDT descriptor fields, why segment registers must be reloaded after `lgdt`
- Buffer for unexpected issues