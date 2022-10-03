#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>		// 定义基本数据类型

typedef int sig_atomic_t;	// 定义信号原子操作类型
typedef unsigned int sigset_t;		/* 32 bits 信号集类型*/

#define _NSIG             32	// 定义32种信号
#define NSIG		_NSIG

// POSIX标准, 信号作为异步事件/软中断, 进程间通信ipc, 可以控制进程状态
#define SIGHUP		 1			// HangUp 挂起控制中断或进程
#define SIGINT		 2			// Interrupt 键盘中断信号
#define SIGQUIT		 3			// Quit 键盘退出信号
#define SIGILL		 4			// Illegale 非法指令
#define SIGTRAP		 5			// Trap 跟踪调试断点
#define SIGABRT		 6			// Abort异常结束退出
#define SIGIOT		 6			// Io Trap
#define SIGUNUSED	 7			// Unused 未使用
#define SIGFPE		 8			// 387 FPU错误
#define SIGKILL		 9			// Kill 强迫进程终止
#define SIGUSR1		10			// User1用户信号进程可使用
#define SIGSEGV		11			// Segment Violation无效的内存引用
#define SIGUSR2		12			// USer2用户信号进程可使用
#define SIGPIPE		13			// Pipe管道写错误无读者
#define SIGALRM		14			// Alarm实时定时器报警
#define SIGTERM		15			// Terminate 进程终止
#define SIGSTKFLT	16			// Stack Fault FPU
#define SIGCHLD		17			// Child子进程停止
#define SIGCONT		18			// Continue 恢复进程继续执行
#define SIGSTOP		19			// Stop停止进程执行
#define SIGTSTP		20			// TTY Stop 可忽略
#define SIGTTIN		21			// TTY In 后台进程请求输入
#define SIGTTOU		22			// TTY Out 后台进程请求输出

/* Ok, I haven't implemented sigactions, but trying to keep headers POSIX  已实现sigactions*/
#define SA_NOCLDSTOP	1		// 当子进程处于停止状态, 不对SIGCHLD处理
#define SA_NOMASK	0x40000000	// 放行在指定的信号处理中再收到该信号
#define SA_ONESHOT	0x80000000	// 信号handler一旦被调用就恢复到默认的handler

#define SIG_BLOCK          0	/* for blocking signals 在block信号集加上给定信号*/
#define SIG_UNBLOCK        1	/* for unblocking signals 在block信号集删除指定信号*/
#define SIG_SETMASK        2	/* for setting the signal mask 设置block信号集*/

#define SIG_DFL		((void (*)(int))0)	/* default signal handling */
#define SIG_IGN		((void (*)(int))1)	/* ignore signal */

struct sigaction {
	void (*sa_handler)(int);	//信号处理函数, SIG_IGN/SIG_DFL 默认handler
	sigset_t sa_mask;			//信号掩码, 在信号处理中阻塞对这些信号处理
	int sa_flags;				//改变信号处理过程的信号集
	void (*sa_restorer)(void);	//恢复函数指针,libc库提供用于清理用户栈, 触发信号处理的信号也阻塞(除非设置SA_NOMASK)
};

//为_sig添加一个信号处理_func, 上面的0 1 和这里最后的一个int都是callback 函数指针
void (*signal(int _sig, void (*_func)(int)))(int);
// 发送信号, raise = kill(getpid(), sig), 用于向当前进程自身发送信号
int raise(int sig);
// 用于向任何进程或进程组发送信号
int kill(pid_t pid, int sig);

// 操作进程block信号集, 当前内核未实现
int sigaddset(sigset_t *mask, int signo);	// 对信号集中信号增, 删修改
int sigdelset(sigset_t *mask, int signo);
int sigemptyset(sigset_t *mask);			// 初始化进程block信号集
int sigfillset(sigset_t *mask);
int sigismember(sigset_t *mask, int signo); /* 清空block的所有信号即响应所有的信号 1 - is, 0 - not, -1 error */

// 在set中返回进程中当前被block信号集, 检查挂起的信号
int sigpending(sigset_t *set);
// 改变进程当前的被block信号集, 若oldset != NULL, 返回oldset block信号集, 若set != NULL, 根据how 修改进程block信号集合
int sigprocmask(int how, sigset_t *set, sigset_t *oldset);
// 临时替换进程的block信号集, 暂停该进程直到收到信号, 信号处理返回时该函数也返回, 恢复到调用前的block信号集
int sigsuspend(sigset_t *sigmask);
// 改变进程收到的指定信号的handler处理
int sigaction(int sig, struct sigaction *act, struct sigaction *oldact);

#endif /* _SIGNAL_H */
