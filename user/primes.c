#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int childWork(int p, int pid);
int main()
{
    int p[2], pid, i;
    if (pipe(p) != 0)
    {
        printf("error: main pipe failed\n");
        exit(1);
    }

    pid = fork();
    if (pid == 0) //child
    {
        close(p[1]);
        childWork(p[0], pid);
    }
    else //parent
    {
        close(p[0]);
        for (i = 2; i < 35; i++)
        {
            write(p[1], &i, sizeof(i));
            //printf("write %d\n", i);
        }
        close(p[1]);
        wait(0);
    }

    exit(0);
}
int childWork(int left, int pid)
{
    int right[2], firstN = 2, nextN;
    right[0] = left;

    while ((read(left, &nextN, sizeof(nextN))) != 0)
    {
        //printf("read:%d\n", nextN);
        if (pid == 0) //init
        {
            pid = 1;
            firstN = nextN;
            printf("prime %d\n", firstN);
        }
        if ((nextN % firstN) != 0)
        {
            if (pid == 1)//first time
            {
                if (pipe(right) != 0)
                {
                    printf("error: pipe failed\n");
                    exit(0);
                }
                pid = fork();
                if(pid == 0){
                    left = right[0];
                    close(right[1]);
                    continue;
                }
                else{
                    close(right[0]);
                }
            }
            write(right[1], &nextN, sizeof(nextN));
            //printf("write to left: %d\n", nextN);
        }
    }
    close(right[1]);
    close(left);
    wait(0);
    //printf("child %d exit\n", getpid());
    exit(0);
}
