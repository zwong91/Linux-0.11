#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/types.h>

#define _LOW(v)		( (v) & 0377)       // 0377 = 0x00FF, 取低字节
#define _HIGH(v)	( ((v) >> 8) & 0377)    // 取高字节

/* options for waitpid, WUNTRACED supported */
#define WNOHANG		1
#define WUNTRACED	2

#define WIFEXITED(s)	(!((s)&0xFF)
#define WIFSTOPPED(s)	(((s)&0xFF)==0x7F)
#define WEXITSTATUS(s)	(((s)>>8)&0xFF)
#define WTERMSIG(s)	((s)&0x7F)
#define WSTOPSIG(s)	(((s)>>8)&0xFF)
#define WIFSIGNALED(s)	(((unsigned int)(s)-1 & 0xFFFF) < 0xFF)

// 挂起当前进程, 等待字进程退出信号SIGCHLD, 如果pid = -1, options = 0, waitpid 等同wait
pid_t wait(int *stat_loc);
pid_t waitpid(pid_t pid, int *stat_loc, int options);

#endif
