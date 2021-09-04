#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    int p[2], pid,pid_currnt;
    char buf[80] = "init";

    if( pipe(p) != 0 )
    {
        printf("error: pipe failed\n");
        exit(1);
    }

    pid = fork();
    pid_currnt = getpid();
    if (pid == 0)
    {
        read(p[0], buf, sizeof(buf));
        printf("%d: received %s\n", pid_currnt, buf);
        write(p[1], "pong", 5);
    }else{
        write(p[1], "ping", 5);
        wait(0);
        read(p[0], buf, sizeof(buf));
        printf("%d: received %s\n", pid_currnt, buf);
        //printf("parent work\n");
    }

        close(p[0]);
        close(p[1]);
    exit(0);
}