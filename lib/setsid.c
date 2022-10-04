/*
 *  linux/lib/setsid.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

// 创建会话, 群组leader, 如果调用者之前不是group leader则没有控制终端
// pid_t setsid()
_syscall0(pid_t,setsid)
