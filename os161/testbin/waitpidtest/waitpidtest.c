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
            _exit(getpid());
        }
    }
    int status, ret;
    
    ret = waitpid(pid[0], NULL, 0);
    if (ret != -1) {
        printf("False test failed.");
    } else {
        printf("False test succeed.");
    }
    
    for (i = 0; i < 10; i ++) {
        
        ret = waitpid(pid[i], &status, 0);
        if (ret) {
            printf("waitpid fail.\n");
        } else {
            printf("waited child %d\n", status);
        }
    }
    return 0;
}

