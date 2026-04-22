#include "uart.h"

int main() {
    puts("Press Ctrl + A + X to exit\n");
    int ret = printf("First print!\n");
    printf("Returned: %d", ret);
}