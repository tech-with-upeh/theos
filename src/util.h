
void memset(void *dest, char val, uint32_t count);
void outPortB(uint16_t Port, uint8_t Value);
char inPortB(uint16_t port);

void outPortL(uint16_t port, uint32_t value);
uint32_t inPortL(uint16_t port);


#define CEIL_DIV(a,b) (((a + b) - 1)/b)

struct InterruptRegisters{
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, csm, eflags, useresp, ss;
};

void* memcpy(void* dest, const void* src, uint32_t count);

