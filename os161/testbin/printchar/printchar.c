/*
 * implement to test the printchar system call
 */

#include <unistd.h>
#include <stdio.h>

#define HW "Hello world!\n"
static char* str = HW;

int main(int argc, char *argv[])
{
    unsigned int i;
    for(i = 0; i < sizeof(HW); i++){
        printchar(str[i]);
    }

    printf("Hello printf!\n");
    
    printf("argc = %d, argv = %x\n", argc, argv);
    for(i = 0; i < argc; i++){
        printf("argv[%d] = (%x): %s\n", i, argv[i], argv[i]);
    }
    return 0;
}

