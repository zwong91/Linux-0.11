/*
 *  linux/kernel/sys.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>

int sys_ftime()
{
	return -ENOSYS;
}

int sys_break()
{
	return -ENOSYS;
}

int sys_ptrace()
{
	return -ENOSYS;
}

int sys_stty()
{
	return -ENOSYS;
}

int sys_gtty()
{
	return -ENOSYS;
}

int sys_rename()
{
	return -ENOSYS;
}

int sys_prof()
{
	return -ENOSYS;
}

// 设置当前进程的real/effective group id
int sys_setregid(int rgid, int egid)
{
	if (rgid>0) {
		if ((current->gid == rgid) || 
		    suser())
			current->gid = rgid;
		else
			return(-EPERM);
	}
	if (egid>0) {
		if ((current->gid == egid) ||
		    (current->egid == egid) ||
		    (current->sgid == egid) ||
		    suser())
			current->egid = egid;
		else
			return(-EPERM);
	}
	return 0;
}

int sys_setgid(int gid)
{
	return(sys_setregid(gid, gid));
}

int sys_acct()
{
	return -ENOSYS;
}

// 反向映射物理内存到进程虚拟地址空间
int sys_phys()
{
	return -ENOSYS;
}

int sys_lock()
{
	return -ENOSYS;
}

int sys_mpx()
{
	return -ENOSYS;
}

int sys_ulimit()
{
	return -ENOSYS;
}

// 返回从1970-1-1 0时 GMT 的时间(s)
// 参数tloc指向用户空间, 使用put_fs_long存储值; 进入内核时 fs 默认指向当前用户数据空间
int sys_time(long * tloc)
{
	int i;

	i = CURRENT_TIME;
	if (tloc) {
		verify_area(tloc,4);
		put_fs_long(i,(unsigned long *)tloc);
	}
	return i;
}

/*
 * Unprivileged users may change the real user id to the effective uid
 * or vice versa. 设置real/effective uid
 */
int sys_setreuid(int ruid, int euid)
{
	int old_ruid = current->uid;
	
	if (ruid>0) {
		if ((current->euid==ruid) ||
                    (old_ruid == ruid) ||
		    suser())
			current->uid = ruid;
		else
			return(-EPERM);
	}
	if (euid>0) {
		if ((old_ruid == euid) ||
                    (current->euid == euid) ||
		    suser())
			current->euid = euid;
		else {
			current->uid = old_ruid;
			return(-EPERM);
		}
	}
	return 0;
}

int sys_setuid(int uid)
{
	return(sys_setreuid(uid, uid));
}

// 设置系统开机时间, 参数tptr 为1970-1-1 0时 GMT经过的时间s
int sys_stime(long * tptr)
{
	if (!suser())
		return -EPERM;
	startup_time = get_fs_long((unsigned long *)tptr) - jiffies/HZ; // 当前时间值s - 已经运行的时间值s
	return 0;
}

// 当前进程CPU运行时间统计
int sys_times(struct tms * tbuf)
{
	if (tbuf) {
		verify_area(tbuf,sizeof *tbuf);
		put_fs_long(current->utime,(unsigned long *)&tbuf->tms_utime);
		put_fs_long(current->stime,(unsigned long *)&tbuf->tms_stime);
		put_fs_long(current->cutime,(unsigned long *)&tbuf->tms_cutime);
		put_fs_long(current->cstime,(unsigned long *)&tbuf->tms_cstime);
	}
	return jiffies;
}

// break调整heap 指向的位置为数据段后, 堆栈段-16KB 前, 由libc封装并且返回二次处理
int sys_brk(unsigned long end_data_seg)
{
	if (end_data_seg >= current->end_code &&
	    end_data_seg < current->start_stack - 16384/*16KB*/)
		current->brk = end_data_seg;
	return current->brk;
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
// 设置指定的pid进程的 group id, 给自己找master
int sys_setpgid(int pid, int pgid)
{
	int i;

	// pid = 0 或 pgid = 0, 使用当前进程的pid, 这里不符合 POSIX 标准描述
	if (!pid)
		pid = current->pid;
	if (!pgid)
		pgid = current->pid;

	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i] && task[i]->pid==pid) {
			if (task[i]->leader) // 本身就是leader, 不用给人做slave 组员 小组概念更小
				return -EPERM;
			if (task[i]->session != current->session)	// 不是同一个会话, 群员概念更大
				return -EPERM;
			task[i]->pgrp = pgid;
			return 0;
		}
	return -ESRCH;
}

// 等同getpgid(0)
int sys_getpgrp(void)
{
	return current->pgrp;
}

// 创建一个会话, leader = 1, session_id = pgid = pid
int sys_setsid(void)
{
	if (current->leader && !suser())
		return -EPERM;
	current->leader = 1;
	current->session = current->pgrp = current->pid;
	current->tty = -1;	// 没有控制终端
	return current->pgrp;	// 返回会话id
}

// 硬编码hard-code
static struct utsname thisname = {
	"linux .0","nodename","release ","version ","machine "
};

// 获取系统信息, utsname逐字节复制到用户缓冲区
int sys_uname(struct utsname * name)
{
	int i;

	if (!name) return -ERROR;
	verify_area(name,sizeof *name);
	for(i=0;i<sizeof *name;i++)
		put_fs_byte(((char *) &thisname)[i],i+(char *) name);
	return 0;
}

// 设置文件访问权限
int sys_umask(int mask)
{
	int old = current->umask;

	current->umask = mask & 0777;
	return (old);
}

#define MAXHOSTNAME 8
int sys_sethostname(char *hostname, int len)
{
	int i;
	if (!suser())
	{
		return -EPERM;
	}
	if (len > MAXHOSTNAME)
	{
		return -EINVAL;
	}
	for (i = 0;i < len; i++) {
		if ((thisname.nodename[i] = get_fs_byte(hostname + i)) == 0) 
			break;
	}
	if (thisname.nodename[i])
	{
		thisname.nodename[i > MAXHOSTNAME ? MAXHOSTNAME : i] = 0;
	}

	return 0;
}