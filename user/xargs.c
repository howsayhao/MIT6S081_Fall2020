#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
    char onebyte;
    char buf[64];
    char *cmd[32];
    int i = 0;
    // char *q;
    // q = buf;

    if(argc < 3) // only a name :: no argument given
    {
        fprintf(2, "What do you mean, No argument!");
        exit(1);
    }
    while(read(0, &onebyte, 1) > 0)
    {
        buf[i++] = onebyte;
        if(onebyte == '\n' || onebyte == '\\'){
            read(0, &onebyte, 1);
            buf[i++] = onebyte;
            if(buf[i-2] == '\n' || (buf[i-2] == '\\' && onebyte == 'n')){
                onebyte = buf[i-2];
                buf[i-2] = '\0';
                if(fork() == 0){
                    int j = 0;
                    for(j=1; j<argc; j++){
                        cmd[j-1] = argv[j];
                    }
                    cmd[j-1] = buf;
                    exec(argv[1], cmd);
                    exit(0);
                }
                wait(0);
                if(onebyte == '\n'){
                    buf[0] = buf[i-1];
                    i = 1;
                }
                else{
                    i = 0;
                }
            }
        }
    }



    exit(0);
}