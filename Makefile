# ============================================================================
#  KFS - Kernel From Scratch (i386)
#
#  Builds the same i386 kernel on:
#    - macOS (Apple Silicon / Intel)  -> i686-elf-* cross toolchain (Homebrew)
#    - Linux x86_64 (42 cluster)      -> host gcc -m32 / ld -m elf_i386
#    - anything with Docker           -> make docker-<target>
#
#  Quick start:   make help     list every target
#                 make run      build and boot in QEMU
#                 make test     automated boot tests (KFS-1 + KFS-2)
#  Options:       V=1           show the full commands
#                 NO_COLOR=1    plain output (also automatic when TERM=dumb)
#                 KVM=1         use KVM with `make run` (Linux only)
#                 WAIT=<sec>    boot delay of the tests (default 4)
# ============================================================================

NAME        := kfs
KERNEL      := $(NAME).bin
ISO         := $(NAME).iso
BUILD_DIR   := build
ISO_DIR     := $(BUILD_DIR)/isodir

UNAME_S     := $(shell uname -s)
have         = $(shell command -v $(1) 2>/dev/null)

# ------------------------------------------------------------------ colors --
# ANSI colors, disabled with NO_COLOR=1 (https://no-color.org) or TERM=dumb.
ifeq ($(NO_COLOR)$(filter dumb,$(TERM)),)
  ESC       := $(shell printf '\033')
  RESET     := $(ESC)[0m
  BOLD      := $(ESC)[1m
  RED       := $(ESC)[31m
  GREEN     := $(ESC)[32m
  YELLOW    := $(ESC)[33m
  BLUE      := $(ESC)[34m
  MAGENTA   := $(ESC)[35m
  CYAN      := $(ESC)[36m
endif
# color of each build step tag
COLOR_AS    := $(YELLOW)
COLOR_CC    := $(GREEN)
COLOR_LD    := $(CYAN)
COLOR_SYM   := $(BLUE)
COLOR_ISO   := $(MAGENTA)
COLOR_RM    := $(RED)
OK          := $(GREEN)[ok]$(RESET)
WARN        := $(YELLOW)[warn]$(RESET)
FAIL        := $(RED)[FAIL]$(RESET)

# --------------------------------------------------------------- verbosity --
# Short output by default ("  CC   kernel/gdt.c"); `make V=1` prints full commands.
ifeq ($(V),1)
  Q         :=
  msg        = @true
else
  Q         := @
  msg        = @printf '  %s%-4s%s %s\n' '$(COLOR_$(1))' '$(1)' '$(RESET)' '$(2)'
endif

# --------------------------------------------------------------- toolchain --
# Prefer a real i686-elf cross compiler when available (required on macOS).
ifneq ($(call have,i686-elf-gcc),)
  CROSS     ?= i686-elf-
endif

CC          := $(CROSS)gcc
LD          := $(CROSS)ld
NM          := $(CROSS)nm
AS          := nasm
QEMU        := qemu-system-i386

ifeq ($(CROSS),)
  ifeq ($(UNAME_S),Darwin)
    $(error macOS needs a cross compiler: brew install i686-elf-gcc i686-elf-binutils (or use `make docker-iso`))
  endif
  # Host x86_64 gcc/ld: force 32-bit output and disable distro defaults (PIE).
  ARCH_CFLAGS  := -m32 -fno-pie -fno-pic
  ARCH_LDFLAGS := -m elf_i386 -no-pie
endif

# GRUB: prefixed on macOS (i686-elf-grub-*), grub-* on Debian/Ubuntu, grub2-* on
# Fedora/RHEL, or a no-sudo install in ~/.local/grub (scripts/install_grub_local.sh).
LOCAL_GRUB    := $(HOME)/.local/grub
GRUB_MKRESCUE := $(firstword $(call have,i686-elf-grub-mkrescue) $(call have,grub-mkrescue) \
                   $(call have,grub2-mkrescue) \
                   $(wildcard $(LOCAL_GRUB)/usr/bin/grub-mkrescue $(LOCAL_GRUB)/usr/bin/grub2-mkrescue))
GRUB_FILE     := $(firstword $(call have,i686-elf-grub-file) $(call have,grub-file) \
                   $(call have,grub2-file) \
                   $(wildcard $(LOCAL_GRUB)/usr/bin/grub-file $(LOCAL_GRUB)/usr/bin/grub2-file))
# Only embed the BIOS (i386-pc) platform -> small ISO (< 10 MB requirement).
GRUB_PLATFORM := $(firstword $(wildcard /usr/lib/grub/i386-pc /usr/share/grub2/i386-pc \
                   $(dir $(call have,i686-elf-grub-mkrescue))../lib/i686-elf/grub/i386-pc \
                   $(LOCAL_GRUB)/usr/lib/grub/i386-pc))
# --compress=xz shells out to the `xz` binary: only use it when present.
GRUB_COMPRESS := $(if $(call have,xz),--compress=xz)

# ------------------------------------------------------------------- flags --
# Subject flags adapted to C (-fno-exceptions / -fno-rtti are C++ only).
#   -fno-omit-frame-pointer / -fno-optimize-sibling-calls: keep the EBP chain
#   complete so the kernel stack tool (KFS-2) can walk every frame.
CFLAGS      := -std=gnu99 -ffreestanding -fno-builtin -fno-stack-protector \
               -nostdlib -nodefaultlibs -fno-asynchronous-unwind-tables \
               -fcf-protection=none -fno-stack-clash-protection \
               -fno-omit-frame-pointer -fno-optimize-sibling-calls \
               -Wall -Wextra -Werror -O2 -g \
               -Iinclude -MMD -MP $(ARCH_CFLAGS)
ASFLAGS     := -f elf32 -g
LDFLAGS     := -T linker.ld -nostdlib -z noexecstack $(ARCH_LDFLAGS)

# ----------------------------------------------------------------- sources --
ASM_SRCS    := boot/boot.asm \
               boot/gdt_flush.asm
C_SRCS      := kernel/kmain.c \
               kernel/gdt.c \
               kernel/stack.c \
               kernel/symbols.c \
               kernel/shell.c \
               kernel/banner.c \
               drivers/vga.c \
               drivers/keyboard.c \
               lib/string.c \
               lib/printk.c

OBJS        := $(ASM_SRCS:%.asm=$(BUILD_DIR)/%.o) $(C_SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS        := $(C_SRCS:%.c=$(BUILD_DIR)/%.d)

# Symbol table (function names for stack traces), see the $(KERNEL) rule.
SYMTAB_SRC  := $(BUILD_DIR)/symtab.c
SYMTAB_OBJ  := $(BUILD_DIR)/symtab.o
SYMTAB_NULL := $(BUILD_DIR)/symtab_empty.o

# ================================================================== build ==
.DEFAULT_GOAL := all

all: $(KERNEL)                                  ## Build the kernel (kfs.bin)

iso: $(ISO)                                     ## Build the bootable GRUB image (kfs.iso)

# Two-pass link so the kernel contains its own function names (stack traces):
#   1. link with an empty symbol table   -> $(BUILD_DIR)/kernel.pass1
#   2. nm pass1 -> $(SYMTAB_SRC) (address -> name), compile it
#   3. link again with the real table    -> $(KERNEL)
# The tables only live in .rodata, placed after .text: code addresses do not move.
$(KERNEL): $(OBJS) $(SYMTAB_NULL) linker.ld scripts/gen_symbols.sh
	$(call msg,LD,$(BUILD_DIR)/kernel.pass1)
	$(Q)$(LD) $(LDFLAGS) -o $(BUILD_DIR)/kernel.pass1 $(OBJS) $(SYMTAB_NULL)
	$(call msg,SYM,$(SYMTAB_SRC))
	$(Q)./scripts/gen_symbols.sh $(NM) $(BUILD_DIR)/kernel.pass1 > $(SYMTAB_SRC)
	$(Q)$(CC) $(CFLAGS) -c $(SYMTAB_SRC) -o $(SYMTAB_OBJ)
	$(call msg,LD,$@)
	$(Q)$(LD) $(LDFLAGS) -o $@ $(OBJS) $(SYMTAB_OBJ)
	@if [ -n "$(GRUB_FILE)" ]; then \
		$(GRUB_FILE) --is-x86-multiboot $@ && echo "$(OK) $@ is multiboot compliant" \
			|| { echo "$(FAIL) $@ is not multiboot compliant"; exit 1; }; \
	else \
		echo "$(WARN) grub-file / grub2-file not found: multiboot check skipped"; \
	fi

$(SYMTAB_NULL): scripts/gen_symbols.sh include/symbols.h
	@mkdir -p $(dir $@)
	@printf '#include "symbols.h"\nconst struct kernel_symbol g_kernel_symbols[] = { { 0, "" } };\nconst size_t g_kernel_symbol_count = 0;\n' > $(BUILD_DIR)/symtab_empty.c
	$(Q)$(CC) $(CFLAGS) -c $(BUILD_DIR)/symtab_empty.c -o $@

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(call msg,AS,$<)
	$(Q)$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(call msg,CC,$<)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(ISO): $(KERNEL) grub/grub.cfg
	@test -n "$(GRUB_MKRESCUE)" || { echo "$(FAIL) grub-mkrescue not found: run ./scripts/install_grub_local.sh (no sudo) or see README"; exit 1; }
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL) $(ISO_DIR)/boot/$(KERNEL)
	@cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	$(call msg,ISO,$@)
	$(Q)$(GRUB_MKRESCUE) $(if $(GRUB_PLATFORM),-d $(GRUB_PLATFORM)) \
		$(GRUB_COMPRESS) --install-modules="multiboot normal" \
		--locales="" --fonts="" --themes="" \
		-o $@ $(ISO_DIR) > $(BUILD_DIR)/grub-mkrescue.log 2>&1 \
		|| { echo "$(FAIL) grub-mkrescue failed:"; cat $(BUILD_DIR)/grub-mkrescue.log; exit 1; }
	@echo "$(OK) $@ : $$(du -h $@ | cut -f1)"

# ==================================================================== run ==
run: $(ISO)                                     ## Boot the ISO through GRUB in QEMU (KVM=1 on Linux)
	$(QEMU) -cdrom $(ISO) $(if $(KVM),-enable-kvm)

run-kernel: $(KERNEL)                           ## Boot kfs.bin directly, no GRUB (fast dev loop)
	$(QEMU) -kernel $(KERNEL)

debug: $(ISO)                                   ## Start QEMU frozen with a gdb server on :1234
	@echo "gdb: gdb $(KERNEL) -ex 'target remote :1234' -ex 'break kmain' -ex 'continue'"
	$(QEMU) -cdrom $(ISO) -s -S

# =================================================================== test ==
test: $(ISO)                                    ## Headless boot tests: "42" (KFS-1) + GDT at 0x800 (KFS-2)
	@./scripts/smoke_test.sh $(ISO)
	@./scripts/gdt_test.sh $(ISO)

info:                                           ## Show the toolchain in use (paste it when comparing setups)
	@echo "$(BOLD)toolchain$(RESET)"
	@echo "host      : $(UNAME_S) $$(uname -m)"
	@echo "CC        : $$(command -v $(CC)) -> $$($(CC) --version | head -1)"
	@echo "LD        : $$(command -v $(LD)) -> $$($(LD) --version | head -1)"
	@echo "NM        : $$(command -v $(NM) || echo MISSING)"
	@echo "AS        : $$($(AS) -v)"
	@echo "mkrescue  : $(or $(GRUB_MKRESCUE),MISSING)"
	@echo "grub-file : $(or $(GRUB_FILE),MISSING)"
	@echo "grub dir  : $(or $(GRUB_PLATFORM),default)"
	@echo "qemu      : $$($(QEMU) --version 2>/dev/null | head -1 || echo MISSING)"

# ================================================================= docker ==
# x86_64 Linux container (Fedora 44, like the cluster and CI). Works on Apple
# Silicon via emulation. Always rebuilds from scratch so the container toolchain
# is really used; runs as the host user so files are not owned by root on Linux.
DOCKER_IMG := kfs-build

docker-image:                                   ## Build the Docker image (needed after a Dockerfile change)
	docker build --platform linux/amd64 -t $(DOCKER_IMG) .

docker-%: docker-image                          ## Run a target in Docker, e.g. make docker-test
	docker run --rm --platform linux/amd64 --user "$$(id -u):$$(id -g)" \
		-v "$(CURDIR)":/kfs -w /kfs $(DOCKER_IMG) \
		sh -c "make fclean && make $*"

# ================================================================== clean ==
clean:                                          ## Remove the build directory
	$(call msg,RM,$(BUILD_DIR))
	$(Q)rm -rf $(BUILD_DIR)

fclean: clean                                   ## Remove build directory, kfs.bin and kfs.iso
	$(call msg,RM,$(KERNEL) $(ISO))
	$(Q)rm -f $(KERNEL) $(ISO)

# Sequential on purpose: with `make -j re` a plain `re: fclean all` could build
# and delete at the same time.
re:                                             ## Rebuild everything from scratch
	@$(MAKE) --no-print-directory fclean
	@$(MAKE) --no-print-directory all

# =================================================================== help ==
help:                                           ## Show this help
	@echo "$(BOLD)Usage:$(RESET) make [target] [V=1] [KVM=1] [WAIT=<sec>] [NO_COLOR=1]"
	@echo ""
	@grep -hE '^[a-zA-Z_%-]+:.*## ' $(firstword $(MAKEFILE_LIST)) \
		| sed -E 's/^([a-zA-Z_%-]+):.*## (.*)$$/\1|\2/' \
		| awk -F'|' '{ printf "  $(CYAN)%-14s$(RESET) %s\n", $$1, $$2 }'

-include $(DEPS)

.PHONY: all iso run run-kernel debug test info docker-image clean fclean re help
