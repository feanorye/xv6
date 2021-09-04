#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int main(int acount, char **av)
{
        int left[2], right[2], pid;
    if( pipe(left) != 0 ){
        printf("error: pipe failed\n");
        exit(1);
    }

    pid = fork();
    if( pid == 0 ){
        close(left[1]);
    }
    else
    {
        close(left[0]);
    }

    exit(0);
}