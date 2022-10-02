#ifndef _TIMES_H
#define _TIMES_H

#include <sys/types.h>

// 文件访问与修改时间结构
struct tms {
	time_t tms_utime;		// 用户使用的cpu时间
	time_t tms_stime;		// 系统内核使用的cpu时间
	time_t tms_cutime;		// 已终止的子进程使用的用户cpu时间
	time_t tms_cstime;		// 已终止的子进程使用的内核cpi时间
};

extern time_t times(struct tms * tp);

#endif
