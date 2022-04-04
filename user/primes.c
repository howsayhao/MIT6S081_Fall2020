#include "kernel/types.h"
#include "user/user.h"

void primes(int pread)
{
  char num[4];
  int fprime;
  int p[2];
  if(read(pread, num, 4) == 0){
    close(pread);
    return;
  }
  printf("prime %d\n", *((int*)num));
  fprime = *((int*)num);
  pipe(p);

  if(fork() == 0)
  {
    close(pread);
    close(p[1]);
    primes(p[0]);
    close(p[0]);
    exit(0);
  }

  while(read(pread, num, 4) > 0)
  {
    if((*((int*)num))%fprime != 0){
      write(p[1], num, 4);
    }
  }
  close(pread);
  close(p[0]);
  close(p[1]);
  wait(0);

}

int main()
{
  int p[2];
  pipe(p);
  for (int i = 2; i < 36; i++)
  {
    write(p[1], (char *)&i, 4);
  }
  //关闭管道写入端  
  close(p[1]);
  primes(p[0]);
  exit(0);
}