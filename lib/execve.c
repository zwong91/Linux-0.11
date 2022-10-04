/*
 *  linux/lib/execve.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

// 加载并执行子进程(其他程序), 化茧成蝶 int execve(const char * file, char **argv, char ** envp)
// params: file - 被执行文件路径名, argv - 命令行参数指针数组, envp - 环境变量参数指针数组
_syscall3(int,execve,const char *,file,char **,argv,char **,envp)
