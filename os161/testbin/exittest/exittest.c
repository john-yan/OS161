/*
 * implement to test the readchar system call
 */

#include <unistd.h>
#include <stdio.h>

int main(void)
{
    int i;
    for (i = 0; i < 10; i++) {
        int pid = fork();
        if(!pid) {
            printf("childID=%d.\n", getpid());
            _exit(0);
        }
    }
    printf("parentID=%d\n", getpid());
    return 0;
}

