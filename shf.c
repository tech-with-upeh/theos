// FORCE the very first bytes of the binary file to be a jump to main!
void __attribute__((naked)) _start() {
    __asm__ volatile (
        "call main\n\t"
        "mov $0, %ebx\n\t" // Exit code 0
        "mov $0, %eax\n\t" // SYS_EXIT
        "int $0x80\n\t"
    );
}

// Inline User Syscall Wrapper
void print_str(const char* str) {
    __asm__ volatile("mov $2, %%eax\n\t"
                     "mov %0, %%ebx\n\t"
                     "int $0x80" : : "r"(str) : "eax", "ebx");
}

// Secondary function test
void helper_function() {
    print_str("SUCCESS: Sub-function called smoothly!\n");
}

void main() {
    print_str("Hello from Main!\n");
    
    // Test the call
    helper_function();
    
    print_str("Back in Main safely!\n");
}
