/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
// 下面定义了调度程序头文件, 定义了任务结构task_struct, 第一个初始任务0的代码和数据
// 还有以宏的形式定义有关描述符参数设置和获取的嵌入汇编函数
#include <linux/sched.h>
#include <linux/kernel.h>	// 内核头文件，含有一些内核常用函数原型定义
#include <linux/sys.h>		// 系统调用头文件, 含有74个系统调用C函数处理, 以'_sys'打头
#include <linux/fdreg.h>	// 软驱控制头文件
#include <asm/system.h>		// 系统头文件， 定义了设置或修改描述符/中断gate等嵌入汇编
#include <asm/io.h>			// io 头文件, 定义了外设输入输出宏嵌入汇编
#include <asm/segment.h>	// 有关段操作寄存器嵌入汇编

#include <signal.h>			// 信号头文件, 定义信号常量, sigaction结构, 函数原型

// 信号编号1-32, 比如信号5的位图数值 = 1 << (5 - 1) = 16 = 00010000b
#define _S(nr) (1<<((nr)-1))
// 除了SIGKILL和SIGSTOP以外, 其他信号都是可阻塞block的, 1011_1111_1110_1111_1111b
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

// 内核调试函数, 显示任务号nr的进程号,进程状态, 内核堆栈大约空闲字节数
void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	// 检测指定任务数据结构后为0的字节数
	while (i<j && !((char *)(p+1))[i])
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

// 显示所有任务的任务号, 进程号，进程状态, 内核堆栈大约空闲字节数
void show_stat(void)
{
	int i;

	for (i=0;i<NR_TASKS;i++)
		if (task[i])
			show_task(i,task[i]);
}
// 8253定时芯片频率振荡初值 =  1.193180MHZ / 100HZ, 定时器发出中断频率为100HZ
#define LATCH (1193180/HZ)

extern void mem_use(void);	// 没有定义和使用

extern int timer_interrupt(void);	// 时钟中断处理函数 kernel/system_call.S
extern int system_call(void);		// 系统调用中断处理函数 kernel/system_call.S

// 每个任务(进程)在内核态运行时都有自己的内核态堆栈, 这里定义了内核态堆栈结构
// 定义任务联合结构, 一个任务的数据结构和其内核态堆栈在同一内存页中, 从堆栈段寄存器ss获取其数据段描述符
union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

// 定义初始任务0的数据和代码
static union task_union init_task = {INIT_TASK,};

//volatile可见性，有序性 volatile的内存语义确保共享变量得到一致和可靠的更新，可以通过锁，也可以通过更轻量的 volatile 关键字，
//volatile保证线程间可见性和禁止指令重排序
//1. 线程 1 将新值写入缓存后，立刻刷新到内存中。
//2. 这个写入内存的操作，使线程 2 的缓存无效。若想读取该共享变量，则需要重新从内存中获取
long volatile jiffies=0;
long startup_time=0;	// 开机时间, 从1970-1-1以来的秒数
struct task_struct *current = &(init_task.task); // 当前任务指针, 初始执行了任务0
struct task_struct *last_task_used_math = NULL;	//使用过387 fpu的任务指针

struct task_struct * task[NR_TASKS] = {&(init_task.task), };	//定义任务指针数组

// 定义用户态堆栈, 总共1K项, 4K字节, init/main初始化过程中当作内核栈head.s。初始化完成后作为任务0的用户态堆栈
// 即 运行任务0前时内核栈, 之后作为任务0idle和任务1 init的用户态栈
long user_stack [ PAGE_SIZE>>2 ] ;
// 设置ss:esp(数据段选择符, 指针)堆栈指针, ss为内核数据段选择符0x10, esp指针指向user_stack最后一项的尾部, 栈从高到底保存栈数据
struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
// 387 fpu 上下文切换和保存恢复
void math_state_restore()
{
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
	last_task_used_math=current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);
		current->used_math=1;
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */

	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
			(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}

/* this is the scheduler proper: */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(next);
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state=0;
}

void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=tmp;
	if (tmp)
		tmp->state=0;
}

void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

	if (!fn)
		return;
	cli();
	if (jiffies <= 0)
		(fn)();
	else {
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)
				break;
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p; // 插入定时器链表头部
		// 定时器链表项从小到大排序, 只要判断前面的是否到期
		while (p->next && p->next->jiffies < p->jiffies) {
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;

			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;

			p = p->next;
		}
	}
	sti();
}

void do_timer(long cpl)
{
	extern int beepcount;
	extern void sysbeepstop(void);

	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

	if (cpl)
		current->utime++;
	else
		current->stime++;

	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}
	if (current_DOR & 0xf0)
		do_floppy_timer();
	if ((--current->counter)>0) return;
	current->counter=0;
	if (!cpl) return;
	schedule();
}

int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

void sched_init(void)
{
	int i;
	struct desc_struct * p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	//在全局描述符表中手动初始化进程0的代码段描述符和局部数据段描述符
	set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
	// 剩余252 项置为NULL, i从1开始哦
	p = gdt+2+FIRST_TSS_ENTRY;
	for(i=1;i<NR_TASKS;i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
/* Clear NT, so that we won't have troubles with that later on */
// 清除eflags的Nested Task NT 递归调用位, NT 指出TSS中的back_link字段是否有效, 这样当前中断任务iret会任务切换
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	ltr(0);	// 将任务0的TSS段选择符加载到寄存器tr
	lldt(0);	// 将局部描述符表段选择符加载到ldtr局部描述符表寄存器
	// 初始化8253/8254定时器, 通道0, 模式3, 二进制计数方式
	// 向8254写入的控制字0x36=0b00110110, 初始计数值为1193180/100即计数器0每10ms发出方波上升沿信号触发IRQ0
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */

	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	set_system_gate(0x80,&system_call);
}
