#ifndef _SCHED_H
#define _SCHED_H

#define NR_TASKS 64	// 系统中同时最大进程数量
#define HZ 100		// 每10ms发出一次时钟中断

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

#define TASK_RUNNING			0	// 就绪/正在运行
#define TASK_INTERRUPTIBLE		1	// 可中断等待
#define TASK_UNINTERRUPTIBLE	2	// 不可中断等待, 用于IO操作等待
#define TASK_ZOMBIE				3	// 僵尸状态, 等着收尸
#define TASK_STOPPED			4	// 进程终止

#ifndef NULL
#define NULL ((void *) 0)
#endif

// 复制进程的页目录页表, 最复杂的函数之一
extern int copy_page_tables(unsigned long from, unsigned long to, long size);
// 释放页表及其指定的内存块
extern int free_page_tables(unsigned long from, unsigned long size);

// 调度初始化
extern void sched_init(void);
// 调度函数
extern void schedule(void);
// 异常中断初始化
extern void trap_init(void);
// 内核出错, 进入死循环
#ifndef PANIC
void panic(const char * str);
#endif
// 往tty写指定长度的字符串
extern int tty_write(unsigned minor,char * buf,int count);

typedef int (*fn_ptr)();	// 函数指针类型

// 387 fpu 状态数据
struct i387_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

// 任务状态段结构
struct tss_struct {
	long	back_link;	/* 16 high bits zero */
	long	esp0;   //内核态堆栈
	long	ss0;		/* 16 high bits zero */
	long	esp1;
	long	ss1;		/* 16 high bits zero */
	long	esp2;
	long	ss2;		/* 16 high bits zero */
	long	cr3;
	long	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;		/* 16 high bits zero */
	long	cs;		/* 16 high bits zero */
	long	ss;		/* 16 high bits zero */
	long	ds;		/* 16 high bits zero */
	long	fs;		/* 16 high bits zero */
	long	gs;		/* 16 high bits zero */
	long	ldt;		/* 16 high bits zero */
	long	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	struct i387_struct i387;
};

// 任务(进程)数据结构
struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */ //任务的运行状态 (-1 不可运行, 0 可运行(就绪), >0 已停止)
	long counter; // 任务运行时间计数(递减), 滴答数, 运行时间片
	long priority; // 运行优先级。任务开始或重置时 counter=priority, 越大运行时间越长
	long signal;  // 信号位图, 每个bits代表一种信号， 信号值=位偏移值+1
	struct sigaction sigaction[32]; //信号执行属性结构, 对应信号的操作
	long blocked;	/* bitmap of masked signals */ //进程信号屏蔽码
/* various fields */
	int exit_code;	// 任务执行停止退出码, 父进程可获取
	unsigned long start_code,end_code,end_data,brk,start_stack; // 代码段地址, 代码段长度字节数, 代码长度+数据长度, 总长度, 堆栈段地址
	long pid,father,pgrp,session,leader; // 进程号,父进程号,父进程组号,会话号,会话leader
	unsigned short uid,euid,suid; // 用户id, 有效用户id,保存的用户id
	unsigned short gid,egid,sgid; // 组id, 有效组id,保存的组id
	long alarm;	//报警定时值滴答数
	long utime,stime,cutime,cstime,start_time;//用户态运行时间滴答数, 内核态运行时间滴答数,子进程用户态运行时间, 子进程内核态运行时间, 进程开始运行时刻
	unsigned short used_math; // 是否使用math 387处理标志
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */ // 进程使用的tty子设备号, -1 表示没有使用
	unsigned short umask; // 文件创建属性屏蔽位
	struct m_inode * pwd; // 当前工作目录inode 节点结构
	struct m_inode * root;// 根目录inode节点结构
	struct m_inode * executable; // 执行文件inode节点结构
	unsigned long close_on_exec; // 执行时关闭文件fd bits标志 include/fcntl.h
	struct file * filp[NR_OPEN]; // 打开的文件fd表, 最多32个
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3]; // 进程局部描述符表 0 为空, 1 代码段cs  2 数据段和堆栈段 ds&ss
/* tss for this task */
	struct tss_struct tss;  // 进程上下文信息结构, CPU寄存器的值, 进程状态, 堆栈内容构成。
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0, \
/* fs info */	-1,0022,NULL,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt code/data 640KB,DPL=3,type= ox0a/0x02*/	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}

// 任务指针数组
extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
// 当前进程结构指针
extern struct task_struct *current;
extern long volatile jiffies;
extern long startup_time;
// 当前时间(s)
#define CURRENT_TIME (startup_time+jiffies/HZ)

// 添加定时器
extern void add_timer(long jiffies, void (*fn)(void));
// 不可中断睡眠
extern void sleep_on(struct task_struct ** p);
// 可中断睡眠
extern void interruptible_sleep_on(struct task_struct ** p);
// 唤醒睡眠的进程
extern void wake_up(struct task_struct ** p);

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
// 全局描述符表中第一个任务状态段TSS0选择符索引和第一个局部描述符表LDT0选择符索引
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
// 计算在全局描述符表中第n个任务TSS选择符值和LDT选择符值 <<3 该描述符在GDT表的起始偏移位置, n << 4 TSS/LDT偏移位置 
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))
// tss 加载到tr寄存器
#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))
// ldt 加载到ldtr寄存器
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))

// 取当前运行任务的数组索引,任务号
/*
* tr寄存器中tss选择符复制到ax
* eax = eax - FIRST_TSS_ENTRY * 8
* eax = eax / 16 = 当前任务号
*/
#define str(n) \
__asm__("str %%ax\n\t" \
	"subl %2,%%eax\n\t" \
	"shrl $4,%%eax" \
	:"=a" (n) \
	:"a" (0),"i" (FIRST_TSS_ENTRY<<3))
/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 */
/**
 * @brief 切换当前任务到任务n
 * %0 - 指向__temp.a + 0 未定义long  a的值32位偏移值
 * %1 - 指向temp.b + 4, 用于存放新的TSS选择符  word 低2字节是新TSS段选择符, 高2字节不用
 * __temp + 6, 未定义 word, __temp用于far jmp 指令操作数, 6字节操作数由4字节偏移地址+2字节段选择符
 * jmp 16位选择符: 32位偏移值, 内存中表示顺序刚好相反, 造成任务切换的长跳转a无用, 太骚气了
 * %2 - dx 新任务n的TSS选择符
 * %3 - ecx 新任务n的task[n]
*/
// #define switch_to(n) {\
// struct {long a,b;} __tmp; \
// __asm__("cmpl %%ecx,current\n\t" \		//任务n 就是当前任务, 啥也不干,跳到1 标号退出
// 	"je 1f\n\t" \
// 	"movw %%dx,%1\n\t" \				// 将新任务的16位选择符放入__tmp.b
// 	"xchgl %%ecx,current\n\t" \			// current = task[n], ecx = 被切换的原任务
// 	"ljmp *%0\n\t" \					// 长跳转到*&__tmp, 造成任务切换
// 	"cmpl %%ecx,last_task_used_math\n\t" \		// 任务切换回来后, 原任务ecx上次使用过387 fpu吗
// 	"jne 1f\n\t" \								// 没有则跳到1标号退出
// 	"clts\n" \							// 原任务上次使用过387 fpu, 清除cr0中的任务切换
// 	"1:" \
// 	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
// 	"d" (_TSS(n)),"c" ((long) task[n])); \
// }

#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__("cmpl %%ecx,current\n\t" \
	"je 1f\n\t" \
	"movw %%dx,%1\n\t" \
	"xchgl %%ecx,current\n\t" \
	"ljmp *%0\n\t" \
	"cmpl %%ecx,last_task_used_math\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	"d" (_TSS(n)),"c" ((long) task[n])); \
}

//页面地址对齐,  代码未使用
#define PAGE_ALIGN(n) (((n)+0xfff)&0xfffff000)

/**
 * @brief 设置addr地址处描述符的base基址字段
 * %0 - addr地址偏移 2
 * %1 - addr地址偏移 4
 * %2 - addr地址偏移 7
 * %3 - edx = base基址
*/
// #define _set_base(addr,base)  \
// __asm__ ("push %%edx\n\t" \
// 	"movw %%dx,%0\n\t" \					// base基址低16位, 0-15 = [addr + 2]
// 	"rorl $16,%%edx\n\t" \					// edx中基址高16位 16-31 = dx
// 	"movb %%dl,%1\n\t" \					// 基址高16位中的低8位 16-23 = [addr + 4]
// 	"movb %%dh,%2\n\t" \					// 基址高16位中的高8位 24-31 = [addr + 7]
// 	"pop %%edx" \
// 	::"m" (*((addr)+2)), \
// 	 "m" (*((addr)+4)), \
// 	 "m" (*((addr)+7)), \
// 	 "d" (base) \
// 	)

#define _set_base(addr,base)  \
__asm__ ("push %%edx\n\t" \
	"movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %%dl,%1\n\t" \
	"movb %%dh,%2\n\t" \
	"pop %%edx" \
	::"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7)), \
	 "d" (base) \
	)
/**
 * @brief 设置addr地址处描述符的limit字段
 * %0 - addr地址
 * %1 - addr地址偏移 6
 * %2 - edx = limit 限长
*/
// #define _set_limit(addr,limit) \
// __asm__ ("push %%edx\n\t" \
// 	"movw %%dx,%0\n\t" \				// 段长linit低16位 = [addr]
// 	"rorl $16,%%edx\n\t" \				// edx 中段长高4位 = dl
// 	"movb %1,%%dh\n\t" \				// 取原[addr+6]字节= dh
// 	"andb $0xf0,%%dh\n\t" \				// 清除dh的低4位
// 	"orb %%dh,%%dl\n\t" \				// 将原高4位 | 段长高4位合成1字节 = dh
// 	"movb %%dl,%1\n\t" \				// 放到[addr+6]处
// 	"pop %%edx" \
// 	::"m" (*(addr)), \
// 	 "m" (*((addr)+6)), \
// 	 "d" (limit) \
// 	)

#define _set_limit(addr,limit) \
__asm__ ("push %%edx\n\t" \
	"movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %1,%%dh\n\t" \
	"andb $0xf0,%%dh\n\t" \
	"orb %%dh,%%dl\n\t" \
	"movb %%dl,%1\n\t" \
	"pop %%edx" \
	::"m" (*(addr)), \
	 "m" (*((addr)+6)), \
	 "d" (limit) \
	)

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , (base) )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

/**
#define _get_base(addr) ({\
unsigned long __base; \
__asm__("movb %3,%%dh\n\t" \
	"movb %2,%%dl\n\t" \
	"shll $16,%%edx\n\t" \
	"movw %1,%%dx" \
	:"=d" (__base) \
	:"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7)) \
        :"memory"); \
__base;})
**/

/**
 * @brief 从addr地址处描述符取段基址
 * %0 - 存放基址__base
 * %1 - addr地址偏移2
 * %2 - addr地址偏移4
 * %3 - addr地址偏移7
*/
static inline unsigned long _get_base(char * addr)
{
	unsigned long __base;
	__asm__("movb %3,%%dh\n\t"			// 取[addr+7]处基址高16位中的高8位 = dh
			"movb %2,%%dl\n\t"			// 取[addr+4]处基址高16位的低8位 = dl
			"shll $16,%%edx\n\t"		// 基地址高16位移到edx的高16位
			"movw %1,%%dx"				// 取[addr+2]处基址低16位 = dx
			:"=&d" (__base)				// edx中含有32位的段基址
			:"m" (*((addr)+2)),
			"m" (*((addr)+4)),
			"m" (*((addr)+7)));
	return __base;
}

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

// 取段选择符segment指定的描述符中的段限长  lsl = Load Segment Limit
// %0 - 段长值字节数, %1 - 段选择符segment
#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif
