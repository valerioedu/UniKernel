#ifndef STRING_H
#define STRING_H

#include <stddef.h>

#ifndef NULL
    #ifdef __cplusplus
        #define NULL 0
    #else
        #define NULL ((void*)0)
    #endif
#endif

#if defined (__cplusplus) && !(restrict)
#define restrict __restrict
#endif

#ifdef __cplusplus
extern "C" {
#endif
void    *memccpy(void *restrict, const void *restrict, int, size_t);
void    *memchr(const void *, int, size_t);
int      memcmp(const void *, const void *, size_t);
void    *memcpy(void *restrict, const void *restrict, size_t);
void    *memmem(const void *, size_t, const void *, size_t);
void    *memmove(void *, const void *, size_t);
void    *memset(void *, int, size_t);
char    *stpcpy(char *restrict, const char *restrict);
char    *stpncpy(char *restrict, const char *restrict, size_t);
char    *strcat(char *restrict, const char *restrict);
char    *strchr(const char *, int);
int      strcmp(const char *, const char *);
char    *strcpy(char *restrict, const char *restrict);
size_t   strcspn(const char *, const char *);
size_t   strlen(const char *);
char    *strncat(char *restrict, const char *restrict, size_t);
int      strncmp(const char *, const char *, size_t);
char    *strncpy(char *restrict, const char *restrict, size_t);
size_t   strnlen(const char *, size_t);
char    *strrchr(const char *, int);
char*    strtok_r(char *restrict, const char *restrict, char **restrict);

size_t   strlcat(char *, char *, size_t);
#ifdef __cplusplus
}
#endif

#endif