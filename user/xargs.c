#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int ac, char *av[])
{
    char buf[100];
    char *p = buf;
    char *str[10];
    int count = 0;
    int mode = 9999;
    int i = 1;
    int pid;
    int cl = 0;
/*    int pl[2];
    int tmp = 0;

    // test workaround
    if (pipe(pl) != 0)
    {
        printf("error: pipe failed\n");
        exit(1);
    }

    char *tmpS[] = {"\"1\n2\"",
                    "hello too"};
    if (tmp == 1)
    {
        write(pl[1], tmpS[1], strlen(tmpS[1]));
    }
    else
    {
        write(pl[1], tmpS[0], strlen(tmpS[0]));
    }
    close(pl[1]); */

    if (!strcmp(av[1], "-n"))
    {
        mode = *(av[2]) - '0';
        i = 3;
    }

    for (; i < ac; i++)
    {
        //printf("%d: %s\n", i, av[i]);
        str[count++] = av[i];
    }
    while (read(0, buf, 1) > 0)
    {
        pid = fork();
        if (pid < 0)
        {
            printf("error: fork failed\n");
            exit(1);
        }
        if (pid > 0)
        {
            wait(0);
            continue;
        }
        if (*p != '"')
            p = buf + 1;
        str[count++] = buf;

        for (int num = 0; num < mode;)
        {
            if (read(0, p, 1) < 1)
                break;
            if (*p == '"')
            {
                //printf("\" come\n");
                continue;
            }
            if (*p == '\\')
            {
                p++;
                if (read(0, p, 1) < 1)
                    break;
                if (*p == 'n')
                {
                    *p = 0;
                    cl = 1;
                    p--;
                }
            }

            // printf("num%d:%c\n", num, *p);
            if (*p == ' ' || cl == 1 || *p == '\n')
            {
                //printf("end:%d\n", (int)(*p));
                *(p++) = '\0';
                num++;
                //printf("str:%s, p:%s\n", str[count], p);
                str[count++] = p;
                if (cl == 1)
                    cl = 0;
                continue;
            }
            p++;
        }
        str[count] = 0;
        //for (i = 0; i < count; i++)
        //    printf("%d:%s\n", i, str[i]);
        //close(pl[0]);
        exec(str[0], str);
        printf("Error: exec failed\n");
    }
    exit(0);
}