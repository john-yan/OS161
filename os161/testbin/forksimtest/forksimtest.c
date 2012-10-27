/*
 * implement to test the readchar system call
 */

#include <unistd.h>
#include <stdio.h>

int main(void)
{
    int pid = fork();
    if(!pid) {
        printf("hello from child %d.\n", getpid());
        return 0;
    }
    
    printf("hello from parent %d.\n", getpid());
    return 0;
}

