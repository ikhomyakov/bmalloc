# bmalloc — Fast Lock-Free Custom Memory Allocator

`bmalloc` is a very fast memory allocator inspired by `djgpmalloc.c` that allocates blocks in a few fixed power-of-two sizes and achieves high efficiency in multi-threaded workloads entirely without locks or mutexes.


## Features

* **Thread-local heaps**: Each thread maintains its own heap structure for allocation.
* **Bucketed allocation**: Memory requests are served from size-classed buckets.
* **Statistics dumping**: On program exit, memory usage statistics are printed.
* **Basic libc replacements**: Implements `malloc`, `calloc`, `realloc`, and `free`.
* **Error detection**: Checks "magic numbers" to detect invalid frees and corruption.
* **Page alignment logic**: Uses system page size (`sysconf`) to align heap growth.
* **Simple `sbrk`-based growth**: Expands memory by requesting more from the OS.


## Limitations

* Functions such as `memalign`, `mallopt`, and `mallinfo` are **not implemented** and will terminate with an error.
* `valloc` internally calls `memalign` and will fail.
* Uses `sbrk`, which is considered obsolete on many systems.
* Maximum thread ID supported: **1024** (configurable by `MAXTD`).
* Minimal error handling—misuse often results in abrupt termination.


## Key Design Principles

* **Thread-local heaps**:
  Each thread allocates from its own private heap structure, reducing contention.

* **Cross-thread freeing**:
  When a block allocated by one thread is freed by another, the memory is reclaimed into the **heap of the freeing thread**.

* **Completely lock-free**:
  Because of this ownership transfer, the allocator never uses locks or mutexes — all operations are fast, local, and free from synchronization overhead.


## Trade-offs

This design comes with both benefits and drawbacks:

✅ **Benefits**

* Lock-free allocation and freeing.
* Very high performance under multi-threaded loads.
* Predictable allocation sizes due to 2^n bucket scheme.

⚠️ **Trade-offs**

* Memory locality can be lost if frees happen in a different thread than the allocation.
* Uneven heap utilization: one thread may accumulate lots of freed memory while another remains starved.

## Build

```bash
gcc -pthread -Wall -O2 -c bmalloc.c -o bmalloc.o
```

To link into a project:

```bash
gcc your_program.c bmalloc.o -o your_program -pthread
```

## Usage

Since `bmalloc.c` overrides standard allocation functions (`malloc`, `calloc`, `free`, `realloc`), simply linking the object file will cause your program to use this allocator automatically.

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    char *msg = (char *)malloc(50);
    snprintf(msg, 50, "Hello from bmalloc!");
    printf("%s\n", msg);
    free(msg);
    return 0;
}
```

At program exit, memory usage statistics are dumped to `stderr`.

## Example Output

```
V Memory Statistics (bmalloc: $Revision$)
B 0/3(64) 5 10/5/5 320/320
H 0 320/320
S 320/320
```

* `B` lines show bucket-level stats.
* `H` lines summarize per-thread heap usage.
* `S` line shows total usage across all heaps.

---

## License

This project is licensed under the **GNU Lesser General Public License v3.0** (LGPL-3.0).
See [LICENSE](https://www.gnu.org/licenses/lgpl-3.0.html) for details.
