#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--) 
    ;
  p++;

  return p;
}

void
find(char *path, char *name)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: can not open %s!\n", path);
        return;
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: stat not get at %s!\n", path);
        close(fd);
        return;
    }

    switch(st.type)
    {
    case T_FILE:
        if(strcmp(fmtname(path), name) == 0){
            printf("%s\n",path);
        }
        break;
    
    case T_DIR:
        // printf("it is a dir: %s\n", path);
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
            printf("ls: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf+strlen(buf);
        *p++ = '/';
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0)
                continue;
            // printf("denode is: %s\n",de.name);
            if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0){   
                continue;
            }
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            //printf("denode path is: %s\n", buf);
            find(buf, name);
        }
        break;
    }
    close(fd);
    return;
}

int
main(int argc, char *argv[])
{
    if(argc <= 2) // only a name :: no argument given
    {
        fprintf(2, "find: arguments not enough!\n");
        exit(1);
    }
    find(argv[1], argv[2]);

    exit(0);
}