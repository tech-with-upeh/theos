#include "vga.h"
#include "stdint.h"

uint16_t column = 0;
uint16_t line = 0;
uint16_t* const vga = (uint16_t* const) 0xC00B8000;
const uint16_t defaultColor = (COLOR8_LIGHT_GREY << 8) | (COLOR8_BLACK << 12);
uint16_t currentColor = defaultColor;

void Reset(){
    line = 0;
    column = 0;
    currentColor = defaultColor;

    for (uint16_t y = 0; y < height; y++){
        for (uint16_t x = 0; x < width; x++){
            vga[y * width + x] = ' ' | defaultColor;
        }
    }
}

void newLine(){
    if (line < height - 1){
        line++;
        column = 0;
    }else{
        scrollUp();
        column = 0;
    }
}

void scrollUp(){
    for (uint16_t y = 0; y < height; y++){
        for (uint16_t x = 0; x < width; x++){
            vga[(y-1) * width + x] = vga[y*width+x];
        }
    }

    for (uint16_t x = 0; x < width; x++){
        vga[(height-1) * width + x] = ' ' | currentColor;
    }
}

void print(const char* s){
    while(*s){
        switch(*s){
            case '\n':
                newLine();
                break;
            case '\r':
                column = 0;
                break;
            case '\b':
                if (column == 0 && line != 0){
                    line--;
                    column = width;
                }
                vga[line * width + (--column)] = ' ' | currentColor;
                break;
            case '\t':
                if (column == width){
                    newLine();
                }
                uint16_t tabLen = 4 - (column % 4);
                while (tabLen != 0){
                    vga[line * width + (column++)] = ' ' | currentColor;
                    tabLen--;
                }
                break;
            default:
                if (column == width){
                    newLine();
                }

                vga[line * width + (column++)] = *s | currentColor;
                break;
        }
        s++;
    }
}

// Handles ALL Unsigned Integers (uint8_t, uint16_t, uint32_t, uint64_t)
void print_uint(uint32_t value) {
    if (value == 0) {
        print("0");
        return;
    }

    char buffer[21]; // Fits max uint64_t (18,446,744,073,709,551,615) + null terminator
    int i = 0;

    while (value > 0) {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }

    // Reverse the buffer in place
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }

    buffer[i] = '\0';
    print(buffer); // Route back to your original string function
}

// Handles ALL Signed Integers (int8_t, int16_t, int32_t, int64_t)
void print_int(int32_t value) {
    if (value == 0) {
        print("0");
        return;
    }

    char buffer[21];
    int i = 0;
    bool_t isNegative = false;

    if (value < 0) {
        isNegative = true;
        // Handle absolute minimum overflow edge case for 64-bit signed integers
        if (value == -9223372036854775807LL - 1LL) {
            print("-9223372036854775808");
            return;
        }
        value = -value;
    }

    while (value > 0) {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }

    if (isNegative) {
        buffer[i++] = '-';
    }

    // Reverse the buffer in place
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }
    buffer[i] = '\0';
    print(buffer); // Route back to your original string function
}
