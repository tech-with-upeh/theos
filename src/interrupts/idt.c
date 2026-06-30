#include "stdint.h"
#include "../util.h"
#include "../vga.h"
#include "idt.h"
#include "../gdt/gdt.h"
#include "../task.h"
#include "../stdlib/stdio.h"
#include "../syscall.h"
struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct idt_ptr;

extern struct tss_entry_struct tss_entry;
extern void idt_flush(uint32_t);
extern Task* current_task;
extern uint64_t ticks;

void initIdt(){
    idt_ptr.limit = sizeof(struct idt_entry_struct) * 256 - 1;
    idt_ptr.base = (uint32_t) &idt_entries;

    memset(&idt_entries, 0, sizeof(struct idt_entry_struct) * 256);

    outPortB(0x20, 0x11);
    outPortB(0xA0, 0x11);

    outPortB(0x21, 0x20);
    outPortB(0xA1, 0x28);

    outPortB(0x21, 0x04);
    outPortB(0xA1, 0x02);

    outPortB(0x21, 0x01);
    outPortB(0xA1, 0x01);

    outPortB(0x21, 0xFC);
    outPortB(0xA1, 0xFF);

    setIdtGate(0, (uint32_t)isr0, 0x08, 0x8E);
    setIdtGate(1, (uint32_t)isr1, 0x08, 0x8E);
    setIdtGate(2, (uint32_t)isr2, 0x08, 0x8E);
    setIdtGate(3, (uint32_t)isr3, 0x08, 0x8E);
    setIdtGate(4, (uint32_t)isr4, 0x08, 0x8E);
    setIdtGate(5, (uint32_t)isr5, 0x08, 0x8E);
    setIdtGate(6, (uint32_t)isr6, 0x08, 0x8E);
    setIdtGate(7, (uint32_t)isr7, 0x08, 0x8E);
    setIdtGate(8, (uint32_t)isr8, 0x08, 0x8E);
    setIdtGate(9, (uint32_t)isr9, 0x08, 0x8E);
    setIdtGate(10, (uint32_t)isr10, 0x08, 0x8E);
    setIdtGate(11, (uint32_t)isr11, 0x08, 0x8E);
    setIdtGate(12, (uint32_t)isr12, 0x08, 0x8E);
    setIdtGate(13, (uint32_t)isr13, 0x08, 0x8E);
    setIdtGate(14, (uint32_t)isr14, 0x08, 0x8E);
    setIdtGate(15, (uint32_t)isr15, 0x08, 0x8E);
    setIdtGate(16, (uint32_t)isr16, 0x08, 0x8E);
    setIdtGate(17, (uint32_t)isr17, 0x08, 0x8E);
    setIdtGate(18, (uint32_t)isr18, 0x08, 0x8E);
    setIdtGate(19, (uint32_t)isr19, 0x08, 0x8E);
    setIdtGate(20, (uint32_t)isr20, 0x08, 0x8E);
    setIdtGate(21, (uint32_t)isr21, 0x08, 0x8E);
    setIdtGate(22, (uint32_t)isr22, 0x08, 0x8E);
    setIdtGate(23, (uint32_t)isr23, 0x08, 0x8E);
    setIdtGate(24, (uint32_t)isr24, 0x08, 0x8E);
    setIdtGate(25, (uint32_t)isr25, 0x08, 0x8E);
    setIdtGate(26, (uint32_t)isr26, 0x08, 0x8E);
    setIdtGate(27, (uint32_t)isr27, 0x08, 0x8E);
    setIdtGate(28, (uint32_t)isr28, 0x08, 0x8E);
    setIdtGate(29, (uint32_t)isr29, 0x08, 0x8E);
    setIdtGate(30, (uint32_t)isr30, 0x08, 0x8E);
    setIdtGate(31, (uint32_t)isr31, 0x08, 0x8E);

    setIdtGate(32, (uint32_t)irq0, 0x08, 0x8E);
    setIdtGate(33, (uint32_t)irq1, 0x08, 0x8E);
    setIdtGate(34, (uint32_t)irq2, 0x08, 0x8E);
    setIdtGate(35, (uint32_t)irq3, 0x08, 0x8E);
    setIdtGate(36, (uint32_t)irq4, 0x08, 0x8E);
    setIdtGate(37, (uint32_t)irq5, 0x08, 0x8E);
    setIdtGate(38, (uint32_t)irq6, 0x08, 0x8E);
    setIdtGate(39, (uint32_t)irq7, 0x08, 0x8E);
    setIdtGate(40, (uint32_t)irq8, 0x08, 0x8E);
    setIdtGate(41, (uint32_t)irq9, 0x08, 0x8E);
    setIdtGate(42, (uint32_t)irq10, 0x08, 0x8E);
    setIdtGate(43, (uint32_t)irq11, 0x08, 0x8E);
    setIdtGate(44, (uint32_t)irq12, 0x08, 0x8E);
    setIdtGate(45, (uint32_t)irq13, 0x08, 0x8E);
    setIdtGate(46, (uint32_t)irq14, 0x08, 0x8E);
    setIdtGate(47, (uint32_t)irq15, 0x08, 0x8E);

    setIdtGate(128, (uint32_t)isr128, 0x08, 0xEE);
    setIdtGate(177, (uint32_t)isr177, 0x08, 0xEE);

    idt_flush((uint32_t)&idt_ptr);
}

void setIdtGate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags){

    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags | 0x60;

}

char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment not present",
    "Stack fault",
    "General protection fault",
    "Page fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Fault",
    "Machine Check", 
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

// void handle_syscall(struct InterruptRegisters* regs) {
//     // Check if the user is asking to print a character (EAX = 'U' or other chars)
//     char user_char = (char)(regs->eax);

//     // Create a tiny 2-byte string to feed your standard kernel string printer safely
//     char buf[2];
//     buf[0] = user_char;
//     buf[1] = '\0';

//     print(buf); // Safe execution inside Ring 0!
// }

// Individual implementation for System Call 1: Print Character
void syscall_print_char(char c) {
    char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    print(buf);
}

// Individual implementation for System Call 2: Print String
void syscall_print_string(const char* str) {
    print((char*)str);
}

// Force a manual CPU software interrupt to trip a context switch instantly
static inline void force_scheduler_yield() {
    __asm__ volatile("int $0x20"); // Tripping the PIT Timer interrupt gate manually
}

void syscall_exit(int exit_code) {
    // 1. Complete ALL print streams first while the task is officially alive
    print("\n[Process Exited with code: ");
    print_int(exit_code);
    print("]\n");

    // 2. Add a tiny delay spin loop so the hardware/emulator video states can flush 
    // and draw the characters onto the screen layout safely
    for (volatile int i = 0; i < 2000000; i++);

    // 3. NOW it is safe to flag the running task card as dead
    current_task->is_alive = 0;

    // 4. Yield control away to the scheduler
    force_scheduler_yield();

    while(1); 
}



// The core router that captures int 0x80
void handle_syscall(struct InterruptRegisters* regs) {
    // Look at EAX to determine which service the user wants
    uint32_t syscall_number = regs->eax;

    switch (syscall_number) {
        case SYS_EXIT:
            // Pass the first argument from EBX
            syscall_exit((int)regs->ebx);
            break;

        case SYS_PRINT_CHAR:
            // Pass the first argument from EBX
            syscall_print_char((char)regs->ebx);
            break;

        case SYS_PRINT_STR:
            // Pass a pointer string argument from EBX
            syscall_print_string((const char*)regs->ebx);
            break;

        default:
            print("UNKNOWN SYSCALL TRIGGERED: ");
            print_int(syscall_number);
            print("\n");
            break;
    }
}


uint32_t isr_handler(struct InterruptRegisters* regs){
    if (regs->int_no == 128) {
        handle_syscall(regs);
        return (uint32_t)regs; // Resume user program execution cleanly
    }

    if (regs->int_no < 32){
        print(exception_messages[regs->int_no]);
        print("\n");
        print("Exception! System Halted\n");
        for (;;);
    }
    return (uint32_t)regs;
}

void *irq_routines[16] = {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

void irq_install_handler (int irq, void (*handler)(struct InterruptRegisters *r)){
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq){
    irq_routines[irq] = 0;
}

uint32_t irq_handler(struct InterruptRegisters* regs) {
    if (current_task == 0) {
        if (regs->int_no == 32) ticks += 1;
        if (regs->int_no >= 40) outPortB(0xA0, 0x20);
        outPortB(0x20, 0x20);
        return (uint32_t)regs;
    }

        if (regs->int_no == 32) {
        ticks += 1;
        outPortB(0x20, 0x20); // Send EOI

        if (current_task->is_alive) {
            current_task->regs = regs;
        }

        Task* previous_task = current_task;
        current_task = current_task->next;

        if (current_task->is_alive == 0) {
            if (current_task->id != 0) {
                print(" [Reaping Dead Task ID: "); print_int(current_task->id); print("] \n");
                previous_task->next = current_task->next;
                current_task = current_task->next;
            }
        }

        // --- CRITICAL VIRTUAL MEMORY ISOLATION SWAP ---
        // Force the physical CPU to switch to the new task's private page directory universe!
        __asm__ volatile("mov %0, %%cr3" : : "r"(current_task->page_directory));

        // Update the hardware TSS stack anchor reference pointer
        //tss_entry.esp0 = (uint32_t)(current_task->regs) + sizeof(struct InterruptRegisters);
        tss_entry.esp0 = (uint32_t) current_task->kernel_stack_ptr + 8192;
        
        return (uint32_t)(current_task->regs);
    }


    if (regs->int_no >= 40) outPortB(0xA0, 0x20);
    outPortB(0x20, 0x20);
    return (uint32_t)regs;
}
