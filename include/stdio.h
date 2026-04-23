#ifndef STDIO_H
#define STDIO_H

#define EOF -1

#if defined (__cplusplus) && !(restrict)
#define restrict __restrict
#endif

#ifdef __cplusplus
extern "C" {
#endif
int putchar(int c);
int puts(const char *str);
int printf(const char *restrict fmt, ...);
#ifdef __cplusplus
}
#endif

#endif