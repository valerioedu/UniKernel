#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

volatile char *uart = (volatile char*)0x09000000;

int putchar(int c) {
    if (c > 127) {
        return EOF;
    }

    *uart = c;
    return c;
}

int __puts(const char *str) {
    int ret = 0;
    while (*str != '\0') {
        putchar(*str++);
        ret++;
    }

    return ret;
}

int puts(const char *str) {
    int ret = 0;
    while (*str != '\0') {
        putchar(*str++);
        ret++;
    }

    putchar('\n');
    ret++;

    return ret;
}

int printd(int num) {
    int ret = 0;
    char buffer[12];
    int i = 0;
    
    while (num != 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
        ret++;
    }
    
    while (i > 0) {
        putchar(buffer[--i]);
    }

    return ret;
}

int printld(int num) {
    int ret = 0;
    char buffer[20];
    int i = 0;
    
    while (num != 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
        ret++;
    }
    
    while (i > 0) {
        putchar(buffer[--i]);
    }

    return ret;
}

int printlx(unsigned long num, bool uppercase) {
    char buf[32];
    int i = 0;

    char a = uppercase ? 'A' : 'a';
    
    do {
        int digit = num % 16;
        buf[i++] = digit < 10 ? '0' + digit : a + digit - 10;
        num /= 16;
    } while (num > 0 && i < 31);
    
    buf[i] = '\0';
    
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = buf[j];
        buf[j] = buf[k];
        buf[k] = temp;
    }
    
    __puts(buf);
    return i - 1;
}

int printx(unsigned long num, bool uppercase) {
    char buf[16];
    int i = 0;

    char a = uppercase ? 'A' : 'a';
    
    do {
        int digit = num % 16;
        buf[i++] = digit < 10 ? '0' + digit : a + digit - 10;
        num /= 16;
    } while (num > 0 && i < 15);
    
    buf[i] = '\0';
    
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = buf[j];
        buf[j] = buf[k];
        buf[k] = temp;
    }
    
    __puts(buf);
    return i - 1;
}

int printf(const char *restrict fmt, ...) {
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
                        ret += printlx(num, false);
                        p += 3;
                        break;
                    }

                    case 'X': {
                        unsigned long num = va_arg(ap, unsigned long);
                        ret += printlx(num, true);
                        p += 3;
                        break;
                    }

                    case 'd': {
                        unsigned long num = va_arg(ap, unsigned long);
                        ret += printld(num);
                        p += 3;
                    }
                }
            } else {
                switch (*(p + 1)) {
                    case 'd': {
                        int num = va_arg(ap, int);
                        ret += printd(num);
                        p += 2;
                        break;
                    }

                    case 's': {
                        ret += __puts(va_arg(ap, char*));
                        p += 2;
                        break;
                    }

                    case 'x': {
                        int num = va_arg(ap, int);
                        ret += printx(num, false);
                        p += 2;
                    }

                    case 'X': {
                        int num = va_arg(ap, int);
                        ret += printx(num, true);
                        p += 2;
                    }
                }
            }
        } else {
            putchar(*p++);
            ret++;
        }
    }

    va_end(ap);
    return ret;
}