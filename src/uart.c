#include "uart.h"
#include <stdarg.h>

volatile char *uart = (char*)0x09000000;

void putc(const char c) {
    *uart = c;
}

int puts(const char *str) {
    int ret = 0;
    while (*str != '\0') {
        putc(*str++);
        ret++;
    }

    return ret;
}

int printnum(int num) {
    int ret = 0;
    char buffer[12];
    int i = 0;
    
    while (num != 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
        ret++;
    }
    
    while (i > 0) {
        putc(buffer[--i]);
    }

    return ret;
}

int printlx(unsigned long num) {
    char buf[32];
    int i = 0;
    
    do {
        int digit = num % 16;
        buf[i++] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        num /= 16;
    } while (num > 0 && i < 31);
    
    buf[i] = '\0';
    
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = buf[j];
        buf[j] = buf[k];
        buf[k] = temp;
    }
    
    puts(buf);
    return i - 1;
}

int printx(unsigned long num) {
    char buf[16];
    int i = 0;
    
    do {
        int digit = num % 16;
        buf[i++] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        num /= 16;
    } while (num > 0 && i < 15);
    
    buf[i] = '\0';
    
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = buf[j];
        buf[j] = buf[k];
        buf[k] = temp;
    }
    
    puts(buf);
    return i - 1;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int ret = 0;
    const char *p = fmt;
    while (*p != '\0') {
        if (*p == '%') {
            if (*(p + 1) == 'l') {
                switch (*(p + 2)) {
                    case 'x': {
                        unsigned long num = va_arg(ap, unsigned long);
                        ret += printlx(num);
                        p += 3;
                        break;
                    }
                }
            } else {
                switch (*(p + 1)) {
                    case 'd': {
                        int num = va_arg(ap, int);
                        ret += printnum(num);
                        p += 2;
                        break;
                    }

                    case 's': {
                        ret += puts(va_arg(ap, char*));
                        p += 2;
                        break;
                    }

                    case 'x': {
                        int num = va_arg(ap, int);
                        ret += printx(num);
                        p += 2;
                    }
                }
            }
        } else {
            putc(*p++);
            ret++;
        }
    }

    va_end(ap);
    return ret;
}