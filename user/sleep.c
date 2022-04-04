#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
    int time_sec = 0;

    if(argc <= 1) // only a name :: no argument given
    {
        fprintf(1, "forget to pass an argument!");
        exit(1);
    }
    time_sec = atoi(argv[1]);
    sleep(time_sec);

    exit(0);
}