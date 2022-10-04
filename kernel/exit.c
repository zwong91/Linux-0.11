/*
 *  linux/kernel/exit.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>		// Linus 从minix引入
#include <signal.h>
#include <sys/wait.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <asm/segment.h>

int sys_pause(void);	// 将进程设置为睡眠状态, 直到收到信号
int sys_close(int fd);	// 关闭指定文件的系统调用

// 释放指定进程占用的任务slots和内存页, 在sys_kill/sys_waitpid中使用
void release(struct task_struct * p)
{
	int i;

	if (!p)
		return;
	for (i=1 ; i<NR_TASKS ; i++)
		if (task[i]==p) {
			task[i]=NULL;
			free_page((long)p);
			schedule();	// 似乎没必要重新调度
			return;
		}
	panic("trying to release non-existent task");
}

// 向指定进程发送sig信号
static inline int send_sig(long sig,struct task_struct * p,int priv)
{
	if (!p || sig<1 || sig>32)
		return -EINVAL;
	if (priv || (current->euid==p->euid) || suser())
		p->signal |= (1<<(sig-1));
	else
		return -EPERM;
	return 0;
}

// 终止会话
static void kill_session(void)
{
	struct task_struct **p = NR_TASKS + task;
	
	// 遍历除任务0外所有任务
	while (--p > &FIRST_TASK) {
		if (*p && (*p)->session == current->session)
			(*p)->signal |= 1<<(SIGHUP-1);
	}
}

/*
 * XXX need to check permissions needed to send signals to process
 * groups, etc. etc.  kill() permissions semantics are tricky! 巧妙
 */
int sys_kill(int pid,int sig)
{
	struct task_struct **p = NR_TASKS + task;
	int err, retval = 0; // sig 为0时, 返回值错误检查

	if (!pid) while (--p > &FIRST_TASK) {		// pid = 0 当前进程为进程组leader, 信号发送给当前进程组的成员
		if (*p && (*p)->pgrp == current->pid) 
			if ((err=send_sig(sig,*p,1)))
				retval = err;
	} else if (pid>0) while (--p > &FIRST_TASK) {	// pid > 0, 信号发送给指定pid进程
		if (*p && (*p)->pid == pid) 
			if ((err=send_sig(sig,*p,0)))
				retval = err;
	} else if (pid == -1) while (--p > &FIRST_TASK) {	// pid = -1, 信号发送给除进程0 idle外所有进程
		if ((err = send_sig(sig,*p,0)))
			retval = err;
	} else while (--p > &FIRST_TASK)			//  pid < -1, 信号发送给进程组-pid的成员
		if (*p && (*p)->pgrp == -pid)
			if ((err = send_sig(sig,*p,0)))
				retval = err;
	return retval;
}

// 通知父进程, 子进程退出, 没有找到爸爸, 则自己释放, POSIX 要求子进程被init 1 收容孤儿
static void tell_father(int pid)
{
	int i;

	if (pid)
		for (i=0;i<NR_TASKS;i++) {
			if (!task[i])
				continue;
			if (task[i]->pid != pid)
				continue;
			task[i]->signal |= (1<<(SIGCHLD-1));
			return;
		}
/* if we don't find any fathers, we just release ourselves */
/* This is not really OK. Must change it to make father 1 */
	printk("BAD BAD - no father found\n\r");
	release(current);
}

// 程序退出函数, 被sys_exit调用
int do_exit(long code)
{
	int i;
	// 释放当前进程代码段和数据段占用的内存页, 第一个参数为CPU线性地址空间起始基址, 第二个参数为要释放的字节长度
	// ldt[1] 进程代码段描述符位置, ldt[2] 进程数据段描述符位置
	// 0x0f 进程代码段选择符, 0x17 进程数据段描述符
	free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));
	free_page_tables(get_base(current->ldt[2]),get_limit(0x17));
	// 如果当前进程有子进程, 子进程的爸爸改为 init 1, 子进程已经是ZOMBIE, SIGCHLD通知爸爸
	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i] && task[i]->father == current->pid) {
			task[i]->father = 1;
			if (task[i]->state == TASK_ZOMBIE)
				/* assumption task[1] is always init */
				(void) send_sig(SIGCHLD, task[1], 1);
		}
	// 关闭当前进程打开的所有文件
	for (i=0 ; i<NR_OPEN ; i++)
		if (current->filp[i])
			sys_close(i);
	// 对当前进程的pwd / root / exec 文件inode 同步, 回收inode并分别置空
	iput(current->pwd);
	current->pwd=NULL;
	iput(current->root);
	current->root=NULL;
	iput(current->executable);
	current->executable=NULL;
	// 当前进程是会话leader且有控制终端, 释放该终端
	if (current->leader && current->tty >= 0)
		tty_table[current->tty].pgrp = 0;
	// 当前进程使用过387 fpu 则重置
	if (last_task_used_math == current)
		last_task_used_math = NULL;
	// 如果是会话leader, 终止会话下的所有相关进程
	if (current->leader)
		kill_session();
	current->state = TASK_ZOMBIE;	// 设置当前进程ZOMBIE
	current->exit_code = code;		// 错误码 $?
	tell_father(current->father);	// 通知父进程子进程终止
	schedule();						// 重新调度, 父进程料理后事
	return (-1);	/* just to suppress warnings */
}

int sys_exit(int error_code)
{
	// (error code & 0xff)<< 8, 低字节 wait/waitpid要求
	return do_exit((error_code&0xff)<<8);
}

int sys_waitpid(pid_t pid,unsigned long * stat_addr, int options)
{
	// flag 表示后面选出的子进程处于就绪/睡眠状态
	int flag, code;
	struct task_struct ** p;

	verify_area(stat_addr,4);
repeat:
	flag=0;
	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if (!*p || *p == current)
			continue;
		if ((*p)->father != current->pid)
			continue;
		if (pid>0) {			// pid > 0, 等待进程号为pid的子进程
			if ((*p)->pid != pid)
				continue;
		} else if (!pid) {		// pid = 0 , 等待进程组id 为当前进程组id的任意子进程
			if ((*p)->pgrp != current->pgrp)
				continue;
		} else if (pid != -1) {	// pid < -1, 等待进程组id 为 -pid的任意子进程
			if ((*p)->pgrp != -pid)
				continue;
		}
		// pid 为 -1, 跳过前面检测, 等待任意子进程
		switch ((*p)->state) {
			case TASK_STOPPED:
				if (!(options & WUNTRACED))	// 如果子进程是停止的, 无需跟踪直接return, 这里没有置位continue
					continue;
				put_fs_long(0x7f,stat_addr);	//0x7f表示WIFSTOPPED为true, 保存状态信息, 立刻返回子进程pid
				return (*p)->pid;
			case TASK_ZOMBIE:
				// 僵尸子进程的用户态和内核态运行的CPU时间累计到父进程
				current->cutime += (*p)->utime;
				current->cstime += (*p)->stime;
				// 取出子进程pid和状态码, 并释放子进程, 返回子进程的pid和status_code
				flag = (*p)->pid;
				code = (*p)->exit_code;
				release(*p);
				put_fs_long(code,stat_addr);
				return flag;
			// 这个子进程即不是停止也是僵尸, flag置1, 表示找到过符合要求的子进程, 但它处于运行/睡眠状态
			default:
				flag=1;
				continue;
		}
	}
	if (flag) {
		if (options & WNOHANG)	// 没有子进程退出直接return
			return 0;
		// 当前进程置为可中断等待状态, 重新调度
		current->state=TASK_INTERRUPTIBLE;
		schedule();
		// 下次再调度本进程时, 没有收到除SIGCHLD外的信号, repeat
		if (!(current->signal &= ~(1<<(SIGCHLD-1))))
			goto repeat;
		else
			return -EINTR;	// 上层应用程序根据这个错误码应该继续调用本函数, 等收尸
	}
	return -ECHILD;
}


