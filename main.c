#include <stdlib.h>
#include <stdio.h>

int main( void )
{
  int i;
  for(i=1;i!=100;i++) {
     size_t sz = rand()%(i*100);
     void *x = malloc( sz );
     printf( "%ld,%x\n",sz,x);
     free(x);
  }
}
