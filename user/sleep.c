#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if(argc <= 1){
        printf("sleep: missing operand\n");
        exit(1);
    }
    int num;
    num = atoi(argv[1]);
    sleep(num);
    exit (0);
}