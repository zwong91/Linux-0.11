/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */
// 以inline内联方式包括定义再unistd.h的内嵌汇编代码
#define __LIBRARY__
#include <unistd.h>
#include <time.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
// 进程0在创建首个新进程1之前要求不要使用其用户态堆栈即fork不能以函数的形式调用
// 以inline内联的方式, 新进程用户态栈中没有进程0的多余信息
// 这里系统调用使用的是进程内核态栈, 而且每个进程都有独立的内核态栈
static inline fork(void) __attribute__((always_inline));
static inline pause(void) __attribute__((always_inline));
static inline _syscall0(int,fork)
// int pause() 系统调用, 暂停进程0的执行, 可中断睡眠状态, 直到收到一个信号
static inline _syscall0(int,pause)
// int setup(void * BIOS) 系统调用， 仅用于linux初始化, 仅在这个程序使用
static inline _syscall1(int,setup,void *,BIOS)
// int sync() 刷新缓冲区到文件系统
static inline _syscall0(int,sync)

#include <linux/tty.h>	// tty头文件, 定义有关tty_io
#include <linux/sched.h>	// 调度程序头文件, 定义任务结构task_struct, 第一个初始任务的数据
#include <linux/head.h>		// 定义段描述符和选择符
#include <asm/system.h>		// 系统头文件, 以宏的形式定义描述符和中断程序内嵌汇编
#include <asm/io.h>			// io 头文件, 以宏的形式定义io端口操作内嵌汇编

#include <stddef.h>			// 标准定义头文件, 定义NULL, offsetof(TYPE, MEMBER)
#include <stdarg.h>			// 标准参数头文件, 以宏的形式定义可变参数列表 va_list, va_start, va_end, va_arg, vsprintf
#include <unistd.h>			
#include <fcntl.h>			// 文件控制头文件, 文件和描述符操作控制常量
#include <sys/types.h>		// 类型头文件, 定义基本数据类型

#include <linux/fs.h>		// 文件系统头文件, file, buffer_head, m_inode等

static char printbuf[1024];		//静态字符串数组, 用于内核显示信息的buf

extern int vsprintf();			// 格式化输出到字符串
extern void init(void);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void hd_init(void);
extern void floppy_init(void);
extern void mem_init(long start, long end);
extern long rd_init(long mem_start, int length);	// ramdisk 虚拟磁盘初始化
extern long kernel_mktime(struct tm * tm);			// 计算系统开机启动时间(s)
extern long startup_time;							// 内核启动时间(s)

/*
 * This is set up by the setup-routine at boot-time
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

// 读取CMOS实时时钟信息, outb_p和inb_p 宏include/asm/io.h 端口输入输出
// 0x70 写地址端口号, 0x80 | addr 读取CMOS的内存地址
// 0x71 读数据端口号
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

// BCD码 二进制编码的十进制数转换二进制数, 1 个字节 表示2位的十进制数, 4个bits 表示一位十进制数
// (val)&15 == (val) & 0xF 低4位为10进制的个位, val)>>4)高四位为10进制的十位
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);		// 均是BCD码
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));	// CMOS时间误差控制在1秒以内
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;			//tm中tm_month为 0 -11
	startup_time = kernel_mktime(&time);	// 计算开机时间
}

// 内存规划三个临界区值
static long memory_end = 0;	 // 机器物理内存大小 Bytes
static long buffer_memory_end = 0;	// 缓冲区末端地址
static long main_memory_start = 0;	// 主内存(用户进程,分页)开始位置

struct drive_info { char dummy[32]; } drive_info;	// 32个字节表示的硬盘参数表信息

// 内核初始化函数, 初始化结束后以任务0(idle)继续运行, 操作系统就是个中断驱动的死循环
void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
	//1. 初始化环境
 	ROOT_DEV = ORIG_ROOT_DEV; // 0x901FC 根文件系统设备号
 	drive_info = DRIVE_INFO; // 复制0x90080 硬盘参数表信息 
	memory_end = (1<<20) + (EXT_MEM_K<<10);	// 机器内存大小Bytes = 1MB + 1M以上的扩展内存
	memory_end &= 0xfffff000;		// 忽略不足4KB(1页)的内存, 裁剪页对齐
	// 规划内存临界值 0-end 内核区, end - buffer_end 磁盘缓冲区, buffer_end - memory_end 用户区
	if (memory_end > 16*1024*1024)
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024) 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
	// 各个组件初始化工作
	mem_init(main_memory_start,memory_end);	// 内存初始化
	trap_init();	// 中断向量初始化
	blk_dev_init();	// 块设备初始化 ll_rw_block
	chr_dev_init();	// 字符设备初始化 tty_io
	tty_init();		// tty初始化
	time_init();	// 设置开启启动时间(s)
	sched_init();	// 进程调度程序初始化, 加载了task 0 的tr, ldtr
	buffer_init(buffer_memory_end);	//buffer_head + hashmap 缓冲区lru
	hd_init();		// 硬盘初始化
	floppy_init();
	//2. 切换用户态
	sti();			// 所有初始化工作完成, 开中断EFLAGS IF位置1
	move_to_user_mode();
	//3. main化身为idle进程0 执行, 继续使用0x10:user_stack[1k] 堆栈作为用户态堆栈, 内核态堆栈则是iretd中断返回手动设置0x17:user_stack[1k] 0-640KB
	if (!fork()) {		/* we count on this going ok */
		//4. init进程1的使用自己的堆栈, 包括用户态堆栈和内核态堆栈
		init();
	}
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
	// 对于任何其他的任务, pause()后必须等待接受信号从可中断等待状态->就绪状态
	// task 0 是个例外(schedule), 只要调度程序没有其他任何进程运行都会切换到task 0, 不依赖task 0的状态
	// cpu 空闲状态都会激活, 隔这loop
	for(;;) pause();
}

static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

// 读取并执行/etc/rc文件非交互式用到的命令行参数和运行环境参数
static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL };

// 读取并执行tty终端交互式用到的命令行参数和运行环境参数
static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

// init 1 进程
void init(void)
{
	int pid,i;
	// 加载根文件系统, 以文件系统的形式管理hd数据
	setup((void *) &drive_info);
	// 0 stdin, 1 stdout 2 stderr 
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
	// 一个以 rc 为标准输入的 shell, 普通文件作为输入读取后shell退出
	// 以非交互式的方式, 执行/etc/rc文件中的命令如登录, 执行完毕, 进程退出
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))  // 重定向stdin到/etc/rc文件, 非交互式运行执行完rc文件立刻对出
			_exit(1);	// 操作不允许
		execve("/bin/sh",argv_rc,envp_rc);  // 进程2化身为shell
		_exit(2); // 文件或目录不存在
	}
	// 等待这个 shell 结束, 给/etc/rc的shell收尸
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	while (1) {
		// 一个以 tty0 终端为标准输入的 shell
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);		// 新shell关闭遗留的stdin/stdout/stderr
			setsid();	// 创建新会话, 设置进程组号
			// 重新打开tty0作为stdin, 并复制成stdout, stderr
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		// 这个 shell 退出了, 设置父进程为init1, 继续进大的死循环
		while (1)
			if (pid == wait(&i))
				break;
		// 外层的init1 进程负责给孤儿们收尸
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();	// 同步操作, 刷新缓冲区
	}
	_exit(0);	/* NOTE! _exit, not exit() */  // 直接就是sys_exit, 没有先擦屁股
}
