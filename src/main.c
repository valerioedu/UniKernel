#include <stdio.h>
#include <stdlib.h>

int main() {
    puts("Press Ctrl + A + X to exit\n");
    int ret = printf("First print!\n");
    printf("Returned: %d\n", ret);
    printf("String and hex test: %s %X\n", "Passed!", 0xFFFF);
}