; boot/boot.asm - Multiboot (v1) entry point for the kernel.
;
; GRUB looks for the multiboot header in the first 8 KiB of the kernel file,
; loads the ELF segments, switches the CPU to 32-bit protected mode
; (paging off, interrupts off) and jumps to _start with:
;   eax = 0x2BADB002 (multiboot magic)
;   ebx = physical address of the multiboot information structure

bits 32

MB_ALIGN     equ 1 << 0                  ; align loaded modules on page boundaries
MB_MEMINFO   equ 1 << 1                  ; ask GRUB for a memory map
MB_FLAGS     equ MB_ALIGN | MB_MEMINFO
MB_MAGIC     equ 0x1BADB002
MB_CHECKSUM  equ -(MB_MAGIC + MB_FLAGS)  ; magic + flags + checksum must be 0

KERNEL_STACK_SIZE equ 16384              ; 16 KiB

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
align 16
global stack_bottom
global stack_top
stack_bottom:
    resb KERNEL_STACK_SIZE
stack_top:

section .text
global _start
extern kmain

_start:
    cli                         ; no IDT yet: keep interrupts disabled
    mov  esp, stack_top         ; the stack grows downwards
    xor  ebp, ebp               ; ebp = 0 marks the end of the call chain (for stack traces in KFS-2)

    push ebx                    ; arg 2: multiboot info pointer
    push eax                    ; arg 1: multiboot magic
    call kmain

    ; kmain should never return; if it does, halt forever.
.hang:
    cli
    hlt
    jmp  .hang

; Tell the linker this object does not need an executable stack.
section .note.GNU-stack noalloc noexec nowrite progbits
