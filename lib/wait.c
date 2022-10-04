/*
 *  linux/lib/wait.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <sys/wait.h>

// 挂起当前进程, 等待子进程结束
_syscall3(pid_t,waitpid,pid_t,pid,int *,wait_stat,int,options)

// wait 系统调用, 直接调用waitpid
pid_t wait(int * wait_stat)
{
	return waitpid(-1,wait_stat,0);
}
