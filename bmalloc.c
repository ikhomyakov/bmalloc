#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>

#define getpagesize() sysconf(_SC_PAGESIZE)
#define	MAGIC 0xef
#define	NBUCKETS 30
#define MAXTD 1024

union overhead {
  union	overhead *nextf;
  struct {
    unsigned char magic;
    unsigned char bucket;
  } ov;
};

struct heap {
  union overhead *nextf[NBUCKETS];
  unsigned int nmalloc[NBUCKETS];
  unsigned int nfree[NBUCKETS];
};

static void dstats( struct heap *hp );
static void mstats( struct heap *hp );
static void morecore(struct heap *hp,int bucket);
static int  init( void );
static struct heap *gethp( void );
static void heap_dtor(void *value);

static int init_flag = 0;

static size_t pagesz;
static int    pagebucket;

static pthread_key_t   key;

static struct heap global_heap;
static struct heap *tdp[MAXTD];

static int init( void )
{
  union overhead *op;
  int bucket;
  size_t n, amt;

  pagesz = getpagesize();
  n = pagesz;
  op = (union overhead *)sbrk(0);
  n = n - sizeof(*op) - ((int)op & (n - 1));
  if (n < 0 ) {
    n += pagesz;
  }
  if( n ) {
      if( (void *)sbrk(n) == (void *)-1 ) {
perror("sbrk error");
exit(1);
          return( -1 );
      }
  }
  bucket = 0;
  amt = 8;
  while( pagesz > amt ) {
      amt <<= 1;
      bucket++;
  }
  pagebucket = bucket;
  /*atexit(dumpstats);*/

  return( 0 );
}

static void heap_dtor(void *value)
{
  struct heap *hp;
  hp = (struct heap *)value;
  dstats(hp);
  mstats(hp);
}

static struct heap *gethp( void )
{
  pthread_t tid;
  struct heap *hp;
  tid = pthread_self();
  if( tid >= MAXTD ) {
perror("tid >=MAXTD error");
exit(1);
      return( NULL );
  }
  hp = tdp[tid];
  if( hp == NULL ) {
      hp = (struct heap *)sbrk(pagesz);
      if( (void *)hp == (void *)-1 ) {
perror("sbrk error");
exit(1);
          return( NULL );
      }
      bzero((void *)hp, pagesz);
      tdp[tid] = hp;
  }
  return( hp );
}

void *malloc( size_t nbytes)
{
  struct heap *hp;
  union overhead *op;
  int bucket;
  size_t n, amt;

  if( !init_flag ) {
      if( init() < 0 ) {
perror("init error");
exit(1);
          return( NULL );
      }
  }
  hp = gethp();
  if( hp == NULL ) {
perror("gethp error");
exit(1);
       return( NULL );
  }
  
  n = pagesz - sizeof(*op);
  if( nbytes <= n ) {
      amt = 8;
      bucket = 0;
      n = -sizeof(*op);
  } else {
      amt = pagesz;
      bucket = pagebucket;
  }

  while( nbytes > amt+n ) {
      amt <<= 1;
      if( amt == 0 ) {
perror("amt<< == 0 error");
exit(1);
          return (NULL);
      }
      bucket++;
  }

  if( (op=hp->nextf[bucket]) == NULL ) {
      morecore(hp,bucket);
      if( (op=hp->nextf[bucket]) == NULL ) {
perror("morecore error");
exit(1);
          return (NULL);
      }
  }

  hp->nextf[bucket] = op->nextf;
  op->ov.magic  = MAGIC;
  op->ov.bucket  = bucket;
  hp->nmalloc[bucket]++;
  return ((void *)(op + 1));
}

static void morecore(struct heap *hp, int bucket)
{
  union overhead *op;
  int sz;
  int amt;
  int nblks;

  sz = 1 << (bucket + 3);
  if (sz <= 0)
    return;
  if (sz < pagesz) {
    amt = pagesz;
    nblks = amt / sz;
  } else {
    amt = sz + pagesz;
    nblks = 1;
  }
  op = (union overhead *)sbrk(amt);
  if ((void *)op == (void *)-1) {
perror("sbrk in morecore error");
exit(1);
    return;
  }

  hp->nextf[bucket] = op;
  while (--nblks > 0) {
    op->nextf = (union overhead *)((char *)op + sz);
    op = (union overhead *)((char *)op + sz);
  }
  op->nextf = NULL;
}

void free(void *cp)
{   
  struct heap *hp;
  union overhead *op;
  int bucket;

  if (cp == NULL)
    return;
  op = (union overhead *)((char *)cp - sizeof (union overhead));
  if (op->ov.magic != MAGIC){
perror("free magic error");
exit(1);
    return;
  }

  hp = gethp();
  if( hp == NULL ) {
perror("gethp error");
exit(1);
      return;
  }

  bucket = op->ov.bucket;
  op->nextf = hp->nextf[bucket];
  hp->nextf[bucket] = op;
  hp->nfree[bucket]++;
}
 
void *realloc(void *cp, size_t nbytes)
{   
  union overhead *op;
  size_t onb, sz;
  int bucket;
  void *res;
  int was_alloced = 0;

  if (cp == NULL) {
      return( malloc(nbytes) );
  }

  op = (union overhead *)( (char *)cp - sizeof(union overhead) );

  if( op->ov.magic == MAGIC ) {
      was_alloced++;
      bucket = op->ov.bucket;
  } else {
perror("realloc did not find magic");
exit(1);
      return( NULL );
  }

  onb = 1 << (bucket + 3);
  if( onb < pagesz ) {
      onb -= sizeof(*op);
  } else {
      onb += pagesz - sizeof(*op);
  }

  if( was_alloced ) {
    if( bucket ) {
        sz = 1 << (bucket + 2);
        if( sz < pagesz ) {
            sz -= sizeof(*op);
        } else {
	    sz += pagesz - sizeof (*op);
        }
    }
    if (nbytes <= onb && nbytes > sz) {
        return( cp );
    } else {
      free( cp );
    }
  }

  if( (res=malloc(nbytes)) == NULL ) {
    return( NULL );
  }

  if( cp != res ) {
    memcpy( res, cp, (nbytes<onb)?nbytes:onb );
  }

  return( res );
}

static void dstats( struct heap *hp )
{
  int i, count;
  union overhead *rv;
  fprintf( stderr, "dstats %u\t", (unsigned int)pthread_self() );
  for( i=0; i!=NBUCKETS; i++ ) {
      count=0;
      for( rv=hp->nextf[i]; rv; rv=rv->nextf ) {
          count++;
      }
      if( count )
          fprintf( stderr, "t%u b%2d c%12d ~s%12d\n", 
                   (unsigned int)pthread_self(), i, count, (1<<(i+3))*count
          );
  }
}

static void mstats( struct heap *hp )
{
  int i, j;
  union overhead *p;
  int totfree = 0,
  totused = 0;

  fprintf( stderr, "mstats %u\nfree:\t", (unsigned int)pthread_self() );

  for( i=0; i!=NBUCKETS; i++) {
      for( j=0,p=hp->nextf[i]; p; p=p->nextf,j++) {
      }
      fprintf( stderr, " %d", j );
      totfree += j * (1 << (i + 3));
  }
  fprintf( stderr, "\nused:\t" );
  for( i=0; i!=NBUCKETS; i++) {
      fprintf(stderr, " %d/%d", hp->nmalloc[i], hp->nfree[i]);
      totused += (hp->nmalloc[i]-hp->nfree[i]) * (1 << (i + 3));
  }
  fprintf(stderr, "\n\tTotal in use: %d, total free: %d\n",
	    totused, totfree);
}

void *calloc(size_t nelem, size_t elsize)
{
perror("calloc error");
exit(1);
  return( NULL );
}

void *memalign(size_t alignment, size_t size)
{
perror("memalign error");
exit(1);

  return( NULL ); /* not supported */
}

void *valloc(size_t size)
{
  if( pagesz == 0 ) {
      if( init() < 0 ) {
          return( NULL );
      }
  }
  return( memalign(pagesz,size) );
}

int mallopt(int cmd, int value)
{
perror("mallopt error");
exit(1);

  return(1); /* not supported */
}

struct mallinfo mallinfo(void)
{
  static struct mallinfo ret;
perror("mallinfo error");
exit(1);

  return( ret ); /* not supported */
}
