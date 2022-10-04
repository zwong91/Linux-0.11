/*
 *  linux/lib/write.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

// 写文件操作函数, int write(int fd, char *buf, off_t count);
// fd - 文件描述符, buf - 写缓冲区指针, count - 写字节数
_syscall3(int,write,int,fd,const char *,buf,off_t,count)
