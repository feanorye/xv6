#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
void find(char *path, char *target)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
    {
        fprintf(2, "find: path too long\n");
        return;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    if (*p != '/')
        *p++ = '/';

    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        if (de.inum == 0 || !strcmp(".", de.name) || !strcmp("..", de.name))
            continue;
            
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        if (stat(buf, &st) < 0)
        {
            printf("find: cannot stat %s\n", buf);
            continue;
        }

        switch (st.type)
        {
        case T_FILE:
            if (strcmp(target, de.name) == 0)
                printf("%s\n", buf);
            break;
        case T_DIR:
            find(buf, target);
        }
    }
    close(fd);
    return;
}

int main(int ac, char *av[])
{
    if (ac < 3)
    {
        printf("Usage: find path target\n");
        exit(0);
    }
    find(".", av[2]);

    exit(0);
}