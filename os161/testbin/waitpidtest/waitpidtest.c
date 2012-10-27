/*
 * implement to test the readchar system call
 */

#include <unistd.h>
#include <stdio.h>

int main(void)
{
    int i, pid[10];
    
    for (i = 0; i < 10; i ++) {
        pid[i] = fork();
        if (pid[i] == 0) {
            printf("childID = %d\n", getpid());
            _exit(i);
        }
    }
    
    for (i = 0; i < 10; i ++) {
        int status, ret;
        ret = waitpid(pid[i], &status, 0);
        if (ret) {
            printf("waitpid fail.\n");
        }
    }
    return 0;
}

