#include <stdio.h>
int main(void)
{
  int i=8192;
  void *m1 = (void *)malloc(i);
  void *m2 = (void *)malloc(i);
  void *m3 = (void *)malloc(i);
  printf("%d: %x,%x,%x\n",i,m1,m2,m3);
  free(m1);
  free(m1);
  free(m1);
  return 0;
}
