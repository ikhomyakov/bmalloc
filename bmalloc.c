#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>

#define getpagesize() sysconf(_SC_PAGESIZE)
#define	MAGIC 0xef
#define	NBUCKETS 30


union overhead {
  union	overhead *next;
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

static void dumpstats( void );
static void mstats( char *s );
static void morecore(int bucket);
static int  init( void );
static struct heap *gethp( void );
static void key_destructor(void *value);

static size_t pagesz;
static int    pagebucket;
static pthread_key_t   key;

static int init( void )
{
  static pthread_mutex_t initlock;
  int bucket
  size_t n, amt;

  pthread_mutex_lock(&initlock);
  if( pagesz != 0 ) {
      pthread_mutex_unlock(&initlock);
      return 0;
  }

  pagesz = getpagesize();
  n = pagesz;
  op = (union overhead *)sbrk(0);
  n = n - sizeof(*op) - ((int)op & (n - 1));
  if (n < 0 ) {
    n += pagesz;
  }
  if( n ) {
      if( (void *)sbrk(n) == (void *)-1 ) {
          pagesz = 0;
          pthread_mutex_unlock(&initlock);
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

  pthread_key_create(&key, heap_dtor);

  pthread_mutex_unlock(&initlock);
  return( 0 );
}

static void heap_dtor(void *value)
{
  /* dump stats */
}

static struct heap *gethp( void )
{
  struct heap *hp = NULL;
  hp = (struct heap *)pthread_getspecific(key);
  if( hp == NULL ) {
      hp = (struct heap *)sbrk(pagesz);
      if( (void *)hp == (void *)-1 ) {
          return( NULL );
      }
      pthread_setspecific(key, (void *)hd );
  }
  return( hp );
}

void *malloc( size_t nbytes)
{
  struct heap *hp;
  union overhead *op;
  int bucket
  size_t n, amt;

  if( pagesz == 0 ) {
      if( init() < 0 ) {
          return( NULL );
      }
  }

  hp = gethp();
  if( hp == NULL ) {
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
          return (NULL);
      }
      bucket++;
  }

  if( (op=hp->nextf[bucket]) == NULL ) {
      morecore(hp,bucket);
      if( (op=hp->nextf[bucket]) == NULL ) {
          return (NULL);
      }
  }

  hp->nextf[bucket] = op->ov.next;
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
  if ((void *)op == (void *)-1)
    return;

  hp->nextf[bucket] = op;
  while (--nblks > 0) {
    op->ov.next = (union overhead *)((char *)op + sz);
    op = (union overhead *)((char *)op + sz);
  }
  op->ov.next = 0;
}

void free(void *cp)
{   
  struct heap *hp;
  union overhead *op;
  int bucket;

  if (cp == NULL)
    return;
  op = (union overhead *)((char *)cp - sizeof (union overhead));
  if (op->ov.magic != MAGIC)
    return;

  hp = gethp();
  if( hp == NULL ) {
      return( NULL );
  }

  bucket = op->ov.bucket;
  op->ov.next = hp->nextf[bucket];
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

  if( (res=malloc(nbytes)) == NULL ) {
    return( NULL );
  }

  if( cp != res ) {
    memcpy( res, cp, (nbytes<onb)?nbytes:onb );
  }

  return( res );
}

static void dumpstats( struct heap *hp )
{
  int i, count;
  union overhead *rv;
  for( i=0; i!=NBUCKETS; i++ ) {
      count=0;
      for( rv=hp->nextf[i]; rv; rv=rv->next ) {
          count++;
      }
      if( count )
          fprintf( stderr, "t%u b%2d c%12d ~s%12d\n", 
                   (unsigned int)pthread_self(), i, count, (1<<(i+3))*count
          );
  }
}

static void mstats(char *s)
{
  int i, j;
  union overhead *p;
  int totfree = 0,
  totused = 0;

  fprintf( stderr, "Memory allocation statistics %s %u\nfree:\t", 
           s, (unsigned int)pthread_self() );

  for( i=0; i!=NBUCKETS; i++) {
      for( j=0,p=hp->nextf[i]; p; p=p->next,j++) {
      }
      fprintf( stderr, " %d", j );
      totfree += j * (1 << (i + 3));
  }
  fprintf( stderr, "\nused:\t" );
  for( i=0; i!=NBUCKETS; i++) {
      fprintf(stderr, " %d", nmalloc[i]);
      totused += (hp->nmalloc[i]-hp->nfree[i]) * (1 << (i + 3));
  }
  fprintf(stderr, "\n\tTotal in use: %d, total free: %d\n",
	    totused, totfree);
}

void *calloc(size_t nelem, size_t elsize)
{
  return( NULL );
}

void *memalign(size_t alignment, size_t size)
{
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
  return(1); /* not supported */
}

struct mallinfo mallinfo(void)
{
  static struct mallinfo ret;
  return( ret ); /* not supported */
}
