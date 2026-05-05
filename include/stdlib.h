#ifndef STDLIB_H
#define STDLIB_H

#ifndef NULL
    #ifdef __cplusplus
        #define NULL 0
    #else
        #define NULL ((void*)0)
    #endif
#endif

#ifndef __SIZE_T
#define __SIZE_T
typedef typeof(sizeof(0)) size_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif
void*   malloc(size_t size);
void    free(void* ptr);
void*   calloc(size_t num, size_t size);
void*   realloc(void* ptr, size_t new_size);
#ifdef __cplusplus
}
#endif

#endif