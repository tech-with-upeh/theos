#include "stdint.h"
#include "../util.h"
#include "../vga.h"
#include "idt.h"
#include "../gdt/gdt.h"
#include "../task.h"
#include "../stdlib/stdio.h"
struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct idt_ptr;

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

    setIdtGate(128, (uint32_t)isr128, 0x08, 0x8E);
    setIdtGate(177, (uint32_t)isr177, 0x08, 0x8E);

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

void isr_handler(struct InterruptRegisters* regs){
    if (regs->int_no < 32){
        print(exception_messages[regs->int_no]);
        print("\n");
        print("Exception! System Halted\n");
        for (;;);
    }
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

// uint32_t irq_handler(struct InterruptRegisters* regs) {
//     // Safety guard for early ticks before multitasking initialization
//     if (current_task == 0) {
//         if (regs->int_no == 32) {
//             ticks += 1;
//         }
//         if (regs->int_no >= 40) outPortB(0xA0, 0x20);
//         outPortB(0x20, 0x20);
//         return (uint32_t)regs;
//     }

//     // Handle PIT Timer Ticks (IRQ 0 / Interrupt 32)
//     if (regs->int_no == 32) {
//         ticks += 1;
//         outPortB(0x20, 0x20); // Signal EOI to PIC immediately
//         printf("switching to task id=%d esp=%x eip=%x\n", current_task->id, (uint32_t)&current_task->regs, current_task->regs.eip);
//         current_task->regs = *regs;             // Save current register layout
        
//         current_task = current_task->next;      // Move forward to the next task card
        
//         //switchTSS((uint32_t)&(current_task->regs), sizeof(struct InterruptRegisters));
//         return (uint32_t)&(current_task->regs); // Send new task stack pointer to eax
//     }

//     // Handle other hardware interrupts (Keyboard, etc.)
//     if (regs->int_no >= 40) {
//         outPortB(0xA0, 0x20);
//     }
//     outPortB(0x20, 0x20);

//     return (uint32_t)regs; // Keep current task execution if not a timer tick
// }

uint32_t irq_handler(struct InterruptRegisters* regs) {
    if (current_task == 0) {
        if (regs->int_no == 32) ticks += 1;
        if (regs->int_no >= 40) outPortB(0xA0, 0x20);
        outPortB(0x20, 0x20);
        return (uint32_t)regs;
    }

    if (regs->int_no == 32) {
        ticks += 1;
        outPortB(0x20, 0x20);

        // Fix: Save the pointer address directly! No massive memory copying.
        current_task->regs = regs;             
        
        current_task = current_task->next;      
        
        // Return the address stored inside the pointer
        return (uint32_t)(current_task->regs); 
    }

    if (regs->int_no >= 40) outPortB(0xA0, 0x20);
    outPortB(0x20, 0x20);
    return (uint32_t)regs;
}
