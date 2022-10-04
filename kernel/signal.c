/*
 *  linux/kernel/signal.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <signal.h>

/**
 * @brief signal 执行流程:
 * do_signal执行完后, system_call.s把进程内核态栈上eip以下全弹出。
 * 执行完iret指令后, CPU 把内核态堆栈cs:eip, eflags, ss:esp 弹出, 恢复到用户态执行程序
 * 由于eip替换为指向信号的handler, 会立即执行用户自定义的handler,处理完了通过ret指令, CPU 跳转到sa_restorer的恢复程序执行
 * sa_restorer做用户态堆栈清理, 跳过signr并把系统调用后的 返回值eax, 寄存器ecx,edx, eflags弹出恢复系统调用后的上下文状态
 * 最后sa_restorer通过ret指令弹出原来用户态的eip = old_eip, 继续执行用户程序。
*/

void do_exit(int error_code);

// 获取当前任务信号blocked位图 signal-get-mask
int sys_sgetmask()
{
	return current->blocked;
}

// 设置新的信号blocked位图, SIGKILL不能屏蔽
int sys_ssetmask(int newmask)
{
	int old=current->blocked;

	current->blocked = newmask & ~(1<<(SIGKILL-1));
	return old;
}

// 复制sigaction到fs数据段描述符即从内核空间复制到用户任务数据段中
static inline void save_old(char * from,char * to)
{
	int i;

	verify_area(to, sizeof(struct sigaction));
	for (i=0 ; i< sizeof(struct sigaction) ; i++) {
		put_fs_byte(*from,to);
		from++;
		to++;
	}
}

// 把sigaction数据从fs数据段复制到to即从用户任务数据段中复制内核数据段中
static inline void get_new(char * from,char * to)
{
	int i;

	for (i=0 ; i< sizeof(struct sigaction) ; i++)
		*(to++) = get_fs_byte(from++);
}

/// signal 系统调用
// signum 指定的信号, handler - 信号处理函数 ,restorer 恢复函数指针 Libc库中用于信号结束恢复用户栈就好像没有执行过信号处理直接返回用户程序
int sys_signal(int signum, long handler, long restorer)
{
	struct sigaction tmp;

	if (signum<1 || signum>32 || signum==SIGKILL)
		return -1;
	tmp.sa_handler = (void (*)(int)) handler;
	tmp.sa_mask = 0;
	tmp.sa_flags = SA_ONESHOT | SA_NOMASK;
	tmp.sa_restorer = (void (*)(void)) restorer;
	handler = (long) current->sigaction[signum-1].sa_handler;
	current->sigaction[signum-1] = tmp;
	return handler;
}

// sigaction 系统调用
int sys_sigaction(int signum, const struct sigaction * action,
	struct sigaction * oldaction)
{
	struct sigaction tmp;

	if (signum<1 || signum>32 || signum==SIGKILL)
		return -1;
	tmp = current->sigaction[signum-1];
	// 在sigaction信号结构设置新的action
	get_new((char *) action,
		(char *) (signum-1+current->sigaction));
	// oldaction不为空, 原操作保存到oldaction中
	if (oldaction)
		save_old((char *) &tmp,(char *) oldaction);
	// 允许信号在自己的handler中再次收到, mask 设置为0, 否则屏蔽掉signum
	if (current->sigaction[signum-1].sa_flags & SA_NOMASK)
		current->sigaction[signum-1].sa_mask = 0;
	else
		current->sigaction[signum-1].sa_mask |= (1<<(signum-1));
	return 0;
}

/**
 * @brief 将信号的处理handler插入到用户程序堆栈中, 并在系统调用结束返回后立即执行handler
 * // 函数参数为进入系统调用处理system_call.s开始直到调用本函数前(system_call.s+126))逐步压入堆栈的值
 * 参数:
 * 1. CPU 执行0x80 中断指令压入的用户栈 ss/esp/eflags/cs/eip
 * 2. 刚进入system_call压入栈的寄存器ds/es/fs/edx/ecx/ebx
 * 3. 调用sys_call_table后压入栈的系统调用返回eax
 * 4. 压入栈中的当前处理信号值signr
*/
void do_signal(long signr,long eax, long ebx, long ecx, long edx,
	long fs, long es, long ds,
	long eip, long cs, long eflags,
	unsigned long * esp, long ss)
{
	unsigned long sa_handler;
	long old_eip=eip;
	struct sigaction * sa = current->sigaction + signr - 1; // current->sigaction[signum-1]
	int longs;
	unsigned long * tmp_esp;

	sa_handler = (unsigned long) sa->sa_handler;
	if (sa_handler==SIG_IGN /*1 忽略信号,直接返回*/)
		return;
	// SIG_DFL = 0, 默认处理
	if (!sa_handler) {
		// SIGCHLD 也直接返回
		if (signr==SIGCHLD)
			return;
		else
			do_exit(1<<(signr-1));	// 终止进程, 参数可作为wait/waitpid的子进程退出状态码或原因
	}
	// 信号一旦被处理,则将handler置空恢复默认handler了
	if (sa->sa_flags & SA_ONESHOT)
		sa->sa_handler = NULL;
	// 原用户程序返回地址eip, cs被保存在内核栈中, 这里修改了内核态上用户系统调用返回下一条指令指针eip指向handler
	// 本次系统调用中断返回用户态时会首先指向用户自定义handler信号处理, 然后再继续指向用户程序
	// 这里函数是从汇编程序被调用的, 并且在函数返回后汇编程序并没有把调用do_signal时所有参数丢弃, eip仍在
	*(&eip) = sa_handler;
	// sigaction结构的sa_mask信号集和引起当前信号处理的signal在当前信号处理handler全都应该屏蔽, 
	// 不过若sa_flags 设置了SA_NOMASK, 那么引起当前信号处理的signal可重入的
	
	// 将原调用用户程序用户堆栈esp 向下扩 7 or 8 个 长字(4字节), 用来存放用户自定义信号handler的参数等
	longs = (sa->sa_flags & SA_NOMASK)?7:8;
	*(&esp) -= longs;
	// 检查内存使用, 越界则分配新的页
	verify_area(esp,longs*4);
	tmp_esp=esp;
	// 从下到上依次将sa_restorer/signr/block(如果有)/eax/ecx/edx/eflags/old_eip作为参数压入用户栈
	put_fs_long((long) sa->sa_restorer,tmp_esp++);
	put_fs_long(signr,tmp_esp++);
	// 若引起当前信号处理的signal可重入的, 也压入栈
	if (!(sa->sa_flags & SA_NOMASK))
		put_fs_long(current->blocked,tmp_esp++);
	put_fs_long(eax,tmp_esp++);
	put_fs_long(ecx,tmp_esp++);
	put_fs_long(edx,tmp_esp++);
	put_fs_long(eflags,tmp_esp++);
	put_fs_long(old_eip,tmp_esp++);
	current->blocked |= sa->sa_mask;
}
