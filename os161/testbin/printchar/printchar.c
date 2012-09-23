/*
 * implement to test the printchar system call
 */

#include <unistd.h>

#define HW "Hello World!\n"
static char* str = HW;

int main(void)
{
    int i;
    for(i = 0; i < sizeof(HW); i++){
        printchar(str[i]);
    }

    return 0;
}

