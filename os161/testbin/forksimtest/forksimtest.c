/*
 * implement to test the readchar system call
 */

#include <unistd.h>
#include <stdio.h>

static int buf;

int main(void)
{
    int pid = fork();
    if(!pid) {
        printf("hello from child.\n");
        return 0;
    }
    
    printf("hello from parent.\n");
    return 0;
}

