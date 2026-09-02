#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

extern bool mmu_enabled;
static pthread_spinlock_t print_lock = 0;
static pthread_spinlock_t read_lock = 0;

volatile char *uart = (volatile char*)0x09000000;

int putchar(int c) {
    if (c > 127) {
        return EOF;
    }

    volatile unsigned int *uart_fr = (volatile unsigned int*)(uart + 0x18);
    while (*uart_fr & (1 << 5)) {
        asm volatile("yield" ::: "memory");
    }

    *uart = (char)c;
    return c;
}

int getchar(void) {
    volatile unsigned int *uart_fr = (volatile unsigned int*)(uart + 0x18);

    while (*uart_fr & (1 << 4)) {
        asm volatile("yield" ::: "memory");
    }

    int c = *uart & 0xFF;
    putchar(c);
    return c;
}

char *gets(char *str) {
    char c;
    unsigned i = 0;
    while ((c = (char)getchar()) != '\n' && c != '\r') {
        str[i++] = c;
    }

    if (i == 0) {
        return NULL;
    } else {
        str[i++] = '\0';
    }

    return str;
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
    if (num == 0) {
        putchar('0');
        return 1;
    }

    int ret = 0;
    char buffer[12];
    int i = 0;

    if (num < 0) {
        putchar('-');
        ret++;
        num = -num;
    }

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
        ret++;
    }

    while (i > 0) {
        putchar(buffer[--i]);
    }

    return ret;
}

int printld(long num) {
    if (num == 0) {
        putchar('0');
        return 1;
    }

    int ret = 0;
    char buffer[20];
    int i = 0;

    if (num < 0) {
        putchar('-');
        ret++;
        num = -num;
    }

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
        ret++;
    }

    while (i > 0) {
        putchar(buffer[--i]);
    }

    return ret;
}

int printlu(unsigned long num) {
    if (num == 0) {
        putchar('0');
        return 1;
    }

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

int printlx(long num, bool uppercase) {
    char buf[20];
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

int printx(int num, bool uppercase) {
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

    if (mmu_enabled) pthread_spin_lock(&print_lock);
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
                        long num = va_arg(ap, long);
                        ret += printld(num);
                        p += 3;
                        break;
                    }

                    case 'u': {
                        unsigned long num = va_arg(ap, unsigned long);
                        ret += printlu(num);
                        p += 3;
                        break;
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
                        break;
                    }

                    case 'X': {
                        int num = va_arg(ap, int);
                        ret += printx(num, true);
                        p += 2;
                        break;
                    }

                    default:
                        p++;
                        break;
                }
            }
        } else {
            putchar(*p++);
            ret++;
        }
    }

    if (mmu_enabled) pthread_spin_unlock(&print_lock);
    va_end(ap);
    return ret;
}

int scand() {
    char buf[64];
    gets(buf);
    return atoi(buf);
}

int scanf(const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = 0;
    const char *p = format;
    if (mmu_enabled) {
        pthread_spin_lock(&read_lock);
    }

    while (*p != '\0') {
        if (*p == '%') {
            if (*(p + 1) == 'l') {

            } else {
                switch (*(p + 1)) {
                case 'd':
                    int *num = va_arg(ap, int*);
                    *num = scand();
                    p += 2;
                    ret++;
                    break;
                }
            }
        } else {
            p++;
        }
    }

    if (mmu_enabled) {
        pthread_spin_unlock(&read_lock);
    }

    va_end(ap);
    return ret;
}