/*
 * Copyright (c) 2001 by W-Technologies, Inc.
 * All Rights Reserved.
 */

Entry points  for  malloc_debug(),  mallocmap(),  mallopt(),
     mallinfo(),  memalign(),  and  valloc(), are empty routines,
     and are provided only to protect the user from  mixing  mal-
     loc() functions from different implementations.

#ifdef _REENTRANT
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int    size_t;
#endif

#ifndef NULL
#define NULL    0
#endif

#include <stdlib.h>
extern void *calloc(size_t, size_t);
extern void free(void *);
extern void *malloc(size_t);
extern void *realloc(void *, size_t);

extern void *valloc(size_t);

extern void *memalign(size_t, size_t);


void *malloc(size_t size);
void *calloc(size_t nelem, size_t elsize);
void free(void *ptr);
void *memalign(size_t alignment, size_t size);
void *realloc(void *ptr, size_t size);
void *valloc(size_t size);

#include <alloca.h>
void *alloca(size_t size);

#include <malloc.h>
int mallopt(int cmd, int value);
struct mallinfo mallinfo(void);


/* -------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define getpagesize() sysconf(_SC_PAGE_SIZE)

union overhead {
  union	overhead *ov_next;	/* when free */
  struct {
    unsigned char	ovu_magic; /* magic number */
    unsigned char	ovu_index; /* bucket # */
  } ovu;
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
};

#define	MAGIC		0xef		/* magic # on accounting info */

#define	NBUCKETS 30
static	union overhead *nextf[NBUCKETS];

static	int pagesz;			/* page size */
static	int pagebucket;			/* page size bucket */

static  void dumpstats( void );

#ifdef MSTATS
static	unsigned int nmalloc[NBUCKETS];
static  void mstats( char *s );
#endif

static void morecore(int bucket);

void * malloc(size_t nbytes)
{
  union overhead *op;
  int bucket, n;
  unsigned amt;

  if (pagesz == 0) {
    pagesz = getpagesize();
    n = pagesz;
    op = (union overhead *)sbrk(0);
    n = n - sizeof (*op) - ((int)op & (n - 1));
    if (n < 0)
      n += pagesz;
    if (n) {
      if (sbrk(n) == (char *)-1)
	return (NULL);
    }
    bucket = 0;
    amt = 8;
    while (pagesz > amt) {
      amt <<= 1;
      bucket++;
    }
    pagebucket = bucket;
    atexit(dumpstats);
  }

  if (nbytes <= (n = pagesz - sizeof (*op))) {
    amt = 8;
    bucket = 0;
    n = -(sizeof (*op) );
  } else {
    amt = pagesz;
    bucket = pagebucket;
  }
  while (nbytes > amt + n) {
    amt <<= 1;
    if (amt == 0)
      return (NULL);
    bucket++;
  }

  if ((op = nextf[bucket]) == NULL) {
    morecore(bucket);
    if ((op = nextf[bucket]) == NULL)
      return (NULL);
  }

  nextf[bucket] = op->ov_next;
  op->ov_magic = MAGIC;
  op->ov_index = bucket;
#ifdef MSTATS
  nmalloc[bucket]++;
#endif
  return ((char *)(op + 1));
}

static void morecore(int bucket)
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
  if ((int)op == -1)
    return;

  nextf[bucket] = op;
  while (--nblks > 0) {
    op->ov_next = (union overhead *)((char *)op + sz);
    op = (union overhead *)((char *)op + sz);
  }
  op->ov_next = 0;
}

void free(void *cp)
{   
  int size;
  union overhead *op;

  if (cp == NULL)
    return;
  op = (union overhead *)((char *)cp - sizeof (union overhead));
  if (op->ov_magic != MAGIC)
    return;

  size = op->ov_index;
  op->ov_next = nextf[size];
  nextf[size] = op;
#ifdef MSTATS
  nmalloc[size]--;
#endif
}

void * realloc(void *cp, size_t nbytes)
{   
  unsigned int onb;
  int i;
  union overhead *op;
  char *res;
  int was_alloced = 0;

  if (cp == NULL)
    return (malloc(nbytes));
  op = (union overhead *)((char *)cp - sizeof (union overhead));
  if (op->ov_magic == MAGIC)
  {
    was_alloced++;
    i = op->ov_index;
  }
  else
  {
    return 0;
  }
  onb = 1 << (i + 3);
  if (onb < pagesz)
    onb -= sizeof (*op);
  else
    onb += pagesz - sizeof (*op);

  if (was_alloced) {
    if (i) {
      i = 1 << (i + 2);
      if (i < pagesz)
	i -= sizeof (*op);
      else
	i += pagesz - sizeof (*op);
    }
    if (nbytes <= onb && nbytes > i) {
      return(cp);
    }
    else
      test_free(cp);
  }
  if ((res = malloc(nbytes)) == NULL)
    return (NULL);
  if (cp != res)
    memcpy(res, cp, (nbytes < onb) ? nbytes : onb);
  return (res);
}

static void dumpstats( void )
{
  int i, count;
  char *rv;
  for (i=0; i<NBUCKETS; i++) {
    count=0;
    for (rv=nextf[i]; rv; rv=*(char **)rv) count++;
    if (count)
      fprintf(stderr, "%2d  %12d  %12d\n", i, count, (1<<(i+3))*count);
  }
}

#ifdef MSTATS
static void mstats(char *s)
{
  	int i, j;
  	union overhead *p;
  	int totfree = 0,
  	totused = 0;

  	fprintf(stderr, "Memory allocation statistics %s\nfree:\t", s);
  	for (i = 0; i < NBUCKETS; i++) {
  		for (j = 0, p = nextf[i]; p; p = p->ov_next, j++)
  			;
  		fprintf(stderr, " %d", j);
  		totfree += j * (1 << (i + 3));
  	}
  	fprintf(stderr, "\nused:\t");
  	for (i = 0; i < NBUCKETS; i++) {
  		fprintf(stderr, " %d", nmalloc[i]);
  		totused += nmalloc[i] * (1 << (i + 3));
  	}
  	fprintf(stderr, "\n\tTotal in use: %d, total free: %d\n",
	    totused, totfree);
}
#endif
