/*
 *  linux/lib/sethostname.c
 *
 *  (C) 2022  Wong
 */
#define __LIBRARY__
#include <unistd.h>

_syscall2(int, sethostname, char *, hostname, int, len);