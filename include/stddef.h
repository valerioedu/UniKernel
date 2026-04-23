#ifndef STDDEF_H
#define STDDEF_H

#ifndef __SIZE_T
#define __SIZE_T
typedef typeof(sizeof(0)) size_t;
#endif

#endif