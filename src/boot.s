MBOOT_PAGE_ALIGN EQU 1 << 0
MBOOT_MEM_INFO EQU 1 << 1
MBOOT_USE_GFX EQU 0

MBOOT_MAGIC EQU 0x1BADB002
MBOOT_FLAGS EQU MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_USE_GFX
MBOOT_CHECKSUM EQU -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
ALIGN 4
    DD MBOOT_MAGIC
    DD MBOOT_FLAGS
    DD MBOOT_CHECKSUM
    DD 0, 0, 0, 0, 0

    DD 0
    DD 800
    DD 600
    DD 32

SECTION .bss
ALIGN 16
stack_bottom:
    RESB 16384 * 8
stack_top:

section .boot

global _start
_start:
    MOV ecx, (initial_page_dir - 0xC0000000)
    MOV cr3, ecx

    MOV ecx, cr4
    OR ecx, 0x10
    MOV cr4, ecx

    MOV ecx, cr0
    OR ecx, 0x80000000
    MOV cr0, ecx

    JMP higher_half

section .text
higher_half:
    MOV esp, stack_top
    PUSH ebx
    PUSH eax
    XOR ebp, ebp
    extern kmain
    CALL kmain

halt:
    hlt
    JMP halt

section .data
align 4096
global initial_page_dir
initial_page_dir:
    ; 1. Identity map the first 4MB (so the CPU doesn't crash when paging turns on)
    DD 10000011b
    TIMES 768-1 DD 0      ; Fill up to index 768 (0xC0000000)

    ; 2. Map 0xC0000000 to 0xC0FFFFFF (Indices 768 to 771)
    DD (0 << 22) | 10000011b
    DD (1 << 22) | 10000011b
    DD (2 << 22) | 10000011b
    DD (3 << 22) | 10000011b

    ; 3. Fill the gap between 0xC1000000 and 0xD0000000 (60 empty entries)
    TIMES 60 DD 0

    ; 4. Map 0xD0000000 to 0xD0FFFFFF for your Heap (Indices 832 to 835)
    ; This maps the heap space directly to physical memory blocks 4, 5, 6, and 7
    DD (4 << 22) | 10000011b
    DD (5 << 22) | 10000011b
    DD (6 << 22) | 10000011b
    DD (7 << 22) | 10000011b

    ; 5. Clear out the rest of the page directory (192 entries left)
    TIMES 192 DD 0
