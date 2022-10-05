/*
 *  linux/lib/dup.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

// 复制文件描述符, 不共享close-on-exec, 宏展开 int dup(int fd), 系统调用int 0x80, 功能号__NR_dup
_syscall1(int,dup,int,fd)
