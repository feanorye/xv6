#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    int p[2], pid;
    if( pipe(p) != 0 ){
        printf("error: pipe failed\n");
        exit(1);
    }

    pid = fork();
    if( pid == 0 ){
        close(p[1]);
    }
    else
    {
        close(p[0]);
        for (int i = 2; i < 35;i++)
        {
            write(p[1],)
        }
    }

    exit(0);
}