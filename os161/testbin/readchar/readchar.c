/*
 * implement to test the readchar system call
 */

#include <unistd.h>
#include <stdio.h>

static int buf;

int main(void)
{
    do {
        buf = readchar();
        printf("\nreceived: %d\n", buf);
    } while (buf != 'q');
    
    printf("read char finish.\n");
    return 0;
}

