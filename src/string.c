#include <string.h>
#include <stdlib.h>
#include <arm_neon.h>

void *memccpy(void *restrict s1, const void *restrict s2, int c, size_t n) {
    unsigned char *d = (unsigned char *)s1;
    const unsigned char *s = (const unsigned char *)s2;
    unsigned char uc = (unsigned char)c;

    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
        
        if (d[i] == uc)
            return (void*)(d + i + 1);
    }

    return NULL;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *a = (const unsigned char*)s;

    for (size_t i = 0; i < n; i++) {
        if (a[i] == (unsigned char)c)
            return (void*)(a + i);
    }

    return NULL;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a1 = (const unsigned char*)s1;
    const unsigned char *a2 = (const unsigned char*)s2;

    for (; n > 0; --n, ++a1, ++a2) {
        if (*a1 != *a2)
            return (int)(*a1 - *a2);
    }

    return 0;
}

void *memcpy(void *restrict s1, const void *restrict s2, size_t n) {
    unsigned char *d = (unsigned char*)s1;
    const unsigned char *s = (const unsigned char*)s2;
    
    while (n >= 16) {
        uint8x16_t chunk = vld1q_u8(s);
        vst1q_u8(d, chunk);
        s += 16;
        d += 16;
        n -= 16;
    }

    while (n--)
        *d++ = *s++;

    return s1;
}

void* memmove(void* s1, const void* s2, size_t n) {
    unsigned char *d = (unsigned char*)s1;
    const unsigned char *s = (const unsigned char*)s2;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *(--d) = *(--s);
    }

    return s1;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *a = (unsigned char*)s;

    uint8x16_t val = vdupq_n_u8(c);
    while (n >= 16) {
        vst1q_u8(a, val);
        a += 16;
        n -= 16;
    }

    while (n--)
        *a++ = (unsigned char)c;

    return s;
}

char *stpcpy(char *restrict s1, const char *restrict s2) {
    char *d = s1;
    const char *s = s2;
    
    while ((*d++ = *s++) != '\0');

    return d - 1;
}

char *stpncpy(char *restrict s1, const char *restrict s2, size_t n) {
    char *d = s1;
    const char *s = s2;
    size_t i = 0;

    for (; i < n && s[i] != '\0'; ++i) 
        d[i] = s[i];

    if (i < n) {
        d[i] = '\0';

        for (size_t j =  i + 1; j < n; ++j)
            d[j] = '\0';
    }

    return &s1[n];
}

char *strcat(char *restrict s1, const char *restrict s2) {
    char *d = s1;

    while (*d) d++;
    while ((*d++ = *s2++) != '\0');

    return s1;
}

size_t strlcat(char *destination, char *source, size_t n) {
    char *d = destination;
    int i = 0;

    while (*d && i++ < n) d++;
    while ((*d++ = *source++) != '\0' && i++ < n);
}

char *strchr(const char *s, int c) {
    for (size_t i = 0;; ++i) {
        if (s[i] == (unsigned char)c)
            return (char*)&s[i];
        
        if (s[i] == '\0')
            return NULL;
    }
}

int strcmp(const char *s1, const char *s2) {
    const unsigned char *a = (const unsigned char*)(s1 ? s1 : "");
    const unsigned char *b = (const unsigned char*)(s2 ? s2 : "");

    while (*a && (*a == *b)) {
        ++a;
        ++b;
    }

    return (int)(*a - *b);
}

size_t strcspn(const char *str, const char *reject) {
    const char *s = str;
    while (*s) {
        const char *r = reject;
        while (*r) {
            if (*s == *r)
                return s - str;

            r++;
        }

        s++;
    }

    return s - str;
}

char* strcpy(char* restrict dest, const char* restrict src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

size_t strlen(const char *s) {
    size_t ret = 0;

    for (; s[ret] != '\0'; ret++);

    return ret;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t ret = 0;
    
    for (; s[ret] != '\0' && ret <= maxlen; ret++);

    return ret;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest + strlen(dest);
    while (n && (*d++ = *src++))
        n--;

    if (n)
        *d = '\0';
    
    return dest;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    const unsigned char *a = (const unsigned char*)(s1 ? s1 : "");
    const unsigned char *b = (const unsigned char*)(s2 ? s2 : "");

    for (; n > 0; --n, ++a, ++b) {
        if (*a != *b) return (int)(*a - *b);
        if (*a == '\0') break;
    }

    return 0;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n && (*d++ = *src++))
        n--;

    if (n) {
        while (--n)
            *d++ = '\0';
    }

    return dest;
}

char* strtok_r(char *restrict str, const char *restrict delim, char **restrict saveptr) {
    if (str == NULL) str = *saveptr;
    if (str == NULL) return NULL;
    
    while (*str && strchr(delim, *str))
        str++;

    if (*str == '\0') return NULL;
    
    char *start = str;
    while (*str && !strchr(delim, *str))
        str++;

    if (*str)
        *str++ = '\0';

    *saveptr = str;
    
    return start;
}

char* strrchr(const char* str, int c) {
    const unsigned char *p = (const unsigned char*)str;
    const unsigned char *last = NULL;

    for (;;) {
        if (*p == (unsigned char)c)
            last = p;
        
        if (*p == '\0')
            break;
        
        p++;
    }

    return (char*)last;
}