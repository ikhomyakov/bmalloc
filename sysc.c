#include <unistd.h>
#include <stdio.h>

#define print(s) (printf(#s),printf(" %d\n",sysconf(s)))

int main(void)
{
print(_SC_CHILD_MAX);
print(_SC_CLK_TCK);
print(_SC_NGROUPS_MAX);
print(_SC_OPEN_MAX);
print(_SC_PAGE_SIZE);
print(_SC_NPROCESSORS_CONF);
print(_SC_NPROCESSORS_ONLN);
print(_SC_PHYS_PAGES);
print(_SC_AVPHYS_PAGES);
print(_SC_THREAD_STACK_MIN);
print(_SC_THREAD_THREADS_MAX);
print(_SC_COHER_BLKSZ);
print(_SC_SPLIT_CACHE);
print(_SC_ICACHE_SZ);
print(_SC_DCACHE_SZ);
print(_SC_ICACHE_LINESZ);
print(_SC_DCACHE_LINESZ);
print(_SC_ICACHE_BLKSZ);
print(_SC_DCACHE_BLKSZ);
print(_SC_DCACHE_TBLKSZ);
print(_SC_ICACHE_ASSOC);
print(_SC_DCACHE_ASSOC);
  return 0;
}
