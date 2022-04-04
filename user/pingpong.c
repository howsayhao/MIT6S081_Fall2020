#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
    int p[2];
    int p2[2];
    char message[8];
    pipe(p);
    pipe(p2);
    if(fork() == 0){
        close(p[1]);
        while(read(p[0], message, 8) > 0);
        printf("%d: received %s\n",getpid(), message);
        write(p2[1], "pong", 8);
        close(p2[1]);
        exit(0);
    }
    write(p[1], "ping", 8);
    close(p[1]);
    wait(0);
    close(p2[1]);
    while(read(p2[0], message, 8)>0);
    printf("%d: received %s\n",getpid(), message);
    close(p[0]);

    exit(0);
}