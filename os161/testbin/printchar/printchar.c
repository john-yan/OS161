/*
 * implement to test the printchar system call
 */

#include <unistd.h>
#include <stdio.h>

#define HW "Hello world!\n"
static char* str = HW;

int main(void)
{
    int i;
    for(i = 0; i < sizeof(HW); i++){
        printchar(str[i]);
    }

    printf("Hello printf!\n");
    return 0;
}

