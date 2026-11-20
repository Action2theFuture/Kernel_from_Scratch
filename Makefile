# ============================================================================
#  KFS Makefile - builds the same i386 kernel on:
#    - macOS (Apple Silicon / Intel)  -> i686-elf-* cross toolchain (Homebrew)
#    - Linux x86_64 (42 cluster)      -> host gcc -m32 / ld -m elf_i386
#    - anything with Docker           -> `make docker-iso`
# ============================================================================

NAME        := kfs
KERNEL      := $(NAME).bin
ISO         := $(NAME).iso
BUILD_DIR   := build
ISO_DIR     := $(BUILD_DIR)/isodir

UNAME_S     := $(shell uname -s)
have         = $(shell command -v $(1) 2>/dev/null)

# ---------------------------------------------------------------- toolchain --
# Prefer a real i686-elf cross compiler when available (required on macOS).
ifneq ($(call have,i686-elf-gcc),)
  CROSS     ?= i686-elf-
endif

CC          := $(CROSS)gcc
LD          := $(CROSS)ld
AS          := nasm

ifeq ($(CROSS),)
  ifeq ($(UNAME_S),Darwin)
    $(error macOS needs a cross compiler: brew install i686-elf-gcc i686-elf-binutils (or use `make docker-iso`))
  endif
  # Host x86_64 gcc/ld: force 32-bit output and disable distro defaults (PIE).
  ARCH_CFLAGS  := -m32 -fno-pie -fno-pic
  ARCH_LDFLAGS := -m elf_i386 -no-pie
endif

# GRUB tools are prefixed on macOS (i686-elf-grub-mkrescue).
# ~/.local/grub = no-sudo install made by scripts/install_grub_local.sh (cluster).
LOCAL_GRUB    := $(HOME)/.local/grub
# Debian/Ubuntu name the tools grub-*, Fedora/RHEL name them grub2-*.
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

# grub-mkrescue --compress=xz shells out to the `xz` binary: only use it when present.
GRUB_COMPRESS := $(if $(call have,xz),--compress=xz)

QEMU        := qemu-system-i386

# -------------------------------------------------------------------- flags --
# Subject flags adapted to C (-fno-exceptions / -fno-rtti are C++ only).
CFLAGS      := -std=gnu99 -ffreestanding -fno-builtin -fno-stack-protector \
               -nostdlib -nodefaultlibs -fno-asynchronous-unwind-tables \
               -fcf-protection=none -fno-stack-clash-protection \
               -Wall -Wextra -Werror -O2 -g \
               -Iinclude -MMD -MP $(ARCH_CFLAGS)
ASFLAGS     := -f elf32 -g
LDFLAGS     := -T linker.ld -nostdlib -z noexecstack $(ARCH_LDFLAGS)

# ------------------------------------------------------------------ sources --
ASM_SRCS    := boot/boot.asm
C_SRCS      := kernel/kmain.c \
               drivers/vga.c \
			   drivers/keyboard.c \
               lib/string.c

OBJS        := $(ASM_SRCS:%.asm=$(BUILD_DIR)/%.o) $(C_SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS        := $(C_SRCS:%.c=$(BUILD_DIR)/%.d)

# -------------------------------------------------------------------- rules --
all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@if [ -n "$(GRUB_FILE)" ]; then \
		$(GRUB_FILE) --is-x86-multiboot $@ && echo "[ok] $@ is multiboot compliant"; \
	else \
		echo "[warn] grub-file / grub2-file not found: multiboot check skipped"; \
	fi

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

iso: $(ISO)

$(ISO): $(KERNEL) grub/grub.cfg
	@test -n "$(GRUB_MKRESCUE)" || { echo "grub-mkrescue not found: run ./scripts/install_grub_local.sh (no sudo) or see README"; exit 1; }
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/$(KERNEL)
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) $(if $(GRUB_PLATFORM),-d $(GRUB_PLATFORM)) \
		$(GRUB_COMPRESS) --install-modules="multiboot normal" \
		--locales="" --fonts="" --themes="" \
		-o $@ $(ISO_DIR) > $(BUILD_DIR)/grub-mkrescue.log 2>&1 \
		|| { echo "grub-mkrescue failed:"; cat $(BUILD_DIR)/grub-mkrescue.log; exit 1; }
	@echo "[ok] $@ : $$(du -h $@ | cut -f1)"

# Boot the ISO through GRUB (what the evaluator will check).
# Add KVM=1 on a Linux host with /dev/kvm (not available on macOS).
run: $(ISO)
	$(QEMU) -cdrom $(ISO) $(if $(KVM),-enable-kvm)

# Fast dev loop: QEMU can load a multiboot v1 ELF directly (no GRUB).
run-kernel: $(KERNEL)
	$(QEMU) -kernel $(KERNEL)

# Start QEMU frozen with a gdb server on :1234 (then: gdb kfs.bin -ex 'target remote :1234').
debug: $(ISO)
	$(QEMU) -cdrom $(ISO) -s -S

# Headless boot test: checks that "42" is written in VGA memory.
test: $(ISO)
	./scripts/smoke_test.sh $(ISO)

# Print the toolchain actually used (paste this when comparing team setups).
info:
	@echo "host      : $(UNAME_S) $$(uname -m)"
	@echo "CC        : $$(command -v $(CC)) -> $$($(CC) --version | head -1)"
	@echo "LD        : $$(command -v $(LD)) -> $$($(LD) --version | head -1)"
	@echo "AS        : $$($(AS) -v)"
	@echo "mkrescue  : $(or $(GRUB_MKRESCUE),MISSING)"
	@echo "grub dir  : $(or $(GRUB_PLATFORM),default)"
	@echo "qemu      : $$($(QEMU) --version 2>/dev/null | head -1 || echo MISSING)"

# Build inside an x86_64 Linux container (works on Apple Silicon via emulation).
DOCKER_IMG := kfs-build
docker-image:
	docker build --platform linux/amd64 -t $(DOCKER_IMG) .

# Always rebuild from scratch inside the container so the container toolchain is
# really exercised (host artifacts would otherwise be reused). Runs as the host
# user so generated files are not owned by root on Linux.
docker-%: docker-image
	docker run --rm --platform linux/amd64 --user "$$(id -u):$$(id -g)" \
		-v "$(CURDIR)":/kfs -w /kfs $(DOCKER_IMG) \
		sh -c "make fclean && make $*"

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -f $(KERNEL) $(ISO)

re: fclean all

-include $(DEPS)

.PHONY: all iso run run-kernel debug test info docker-image clean fclean re
