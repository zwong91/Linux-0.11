#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include <sys/types.h>

// Unix Timesharing System name, POSIX 标准要求字符数组不指定长度, 这里 9 不符合标准
struct utsname {
	char sysname[9];		// 当前操作系统名称
	char nodename[9];		// 在网络中node节点名称
	char release[9];		// release信息
	char version[9];		// 系统版本号
	char machine[9];		// 系统运行硬件机器名称
};

extern int uname(struct utsname * utsbuf);

#endif
