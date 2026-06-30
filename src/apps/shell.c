#include "libc/ulib.h"

void main() {
    puts("Testing our new standard library output streams...\n");

    putc('[');
    puts("SUCCESS");
    putc(']');
    putc('\n');

    int process_id = 1;
    uint32_t sample_pointer = 0x00401234;

    // Direct multi-parameter printf verification call!
    printf("Hello from application. PID: %d, Pointer Location: %p, Hex representation: %x\n", 
             process_id, sample_pointer, 255);

    printf("String formatting verification: %s\n", "The ulib framework is fully operational!");

    int fd = open("test.txt");
    char buff[5];
    int rdf = read(fd, buff, 5);

    printf("buffer: %s: \n", buff);
    // Exit system trap
    exit(0);
}

