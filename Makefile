libbmalloc.so: bmalloc.c
	gcc -G -D_REENTRANT -g -o libbmalloc.so bmalloc.c
main: main.c
	gcc -g -D_REENTRANT -o main main.c -L. -lbmalloc -lpthread
