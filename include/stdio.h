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
int getchar(void);
char *gets(char *str);
int puts(const char *str);
int printf(const char *restrict fmt, ...);
int scanf(const char *restrict format, ...);
#ifdef __cplusplus
}
#endif

#endif