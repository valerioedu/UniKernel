#include <stdlib.h>
#include <stdbool.h>

int atoi(const char *str) {
    if (!str) {
        return 0;
    }

    while (*str == ' '  || *str == '\t') {
        str++;
    }

    bool is_negative = false;
    if (*str == '-') {
        is_negative = true;
        str++;
    }

    if (*str < '0' || *str > '9') {
        return 0;
    }

    int ret = 0;
    while (*str >= '0' && *str <= '9') {
        ret *= 10;
        ret += *str - '0';
        str++;
    }

    if (is_negative) {
        return ret * -1;
    } else {
        return ret;
    }
}