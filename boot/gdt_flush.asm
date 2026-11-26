; boot/gdt_flush.asm - load the GDT and reload every segment register.
;
; void gdt_flush(const struct gdt_ptr *ptr);
;
; lgdt only changes GDTR: the CPU keeps using the old (hidden) descriptor
; cached in each segment register until that register is reloaded.
;   - CS cannot be written with mov: a far jump loads it.
;   - DS/ES/FS/GS/SS are reloaded with mov.

bits 32

KERNEL_CODE_SEL  equ 0x08
KERNEL_DATA_SEL  equ 0x10
KERNEL_STACK_SEL equ 0x18

section .text
global gdt_flush

gdt_flush:
    mov  eax, [esp + 4]             ; arg: pointer to struct gdt_ptr
    lgdt [eax]                      ; declare the GDT to the CPU (GDTR = base + limit)

    jmp  KERNEL_CODE_SEL:.reload_cs ; far jump: CS = 0x08 (kernel code)
.reload_cs:
    mov  ax, KERNEL_DATA_SEL
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ax, KERNEL_STACK_SEL       ; SS = kernel stack segment
    mov  ss, ax                     ; flat segment: ESP stays valid
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
