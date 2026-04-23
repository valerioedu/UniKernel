#ifndef STDLIB_H
#define STDLIB_H

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef __SIZE_T
#define __SIZE_T
typedef typeof(sizeof(0)) size_t;
#endif

#endif