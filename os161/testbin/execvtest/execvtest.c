#include <unistd.h>
#include <stdio.h>

int main(void)
{
    char *argv[] = {
        "b",
        "c",
        NULL
    };
    
    printf("ret = %d\n", execv("/testbin/printchar", argv));
    return 0;
}
