/*
 *  linux/lib/close.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

// 关闭文件函数, int clone(int fd) 直接调用int 0x80, 参数__NR_close
_syscall1(int,close,int,fd)
