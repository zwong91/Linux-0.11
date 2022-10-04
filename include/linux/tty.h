/*
 * 'tty.h' defines some structures used by tty_io.c and some defines.
 *
 * NOTE! Don't touch this without checking that nothing in rs_io.s or
 * con_io.s breaks. Some constants are hardwired into the system (mainly
 * offsets into 'tty_queue'
 */

#ifndef _TTY_H
#define _TTY_H

#include <termios.h>

#define TTY_BUF_SIZE 1024		// tty 缓冲区队列大小
// tty 等待队列
struct tty_queue {
	unsigned long data;		// 等待队列缓冲区当前数据指针
	unsigned long head;		// 缓冲区数据头指针
	unsigned long tail;		// 缓冲区数据尾指针
	struct task_struct * proc_list;	// 等待进程列表
	char buf[TTY_BUF_SIZE];		// 队列的缓冲区
};

// 缓冲区指针前移1个字节, 若越界则指针循环
#define INC(a) ((a) = ((a)+1) & (TTY_BUF_SIZE-1))
// 缓冲区指针后退1个字节, 若越界则指针循环
#define DEC(a) ((a) = ((a)-1) & (TTY_BUF_SIZE-1))
// 清空缓冲区队列
#define EMPTY(a) ((a).head == (a).tail)
// 获取空闲区长度
#define LEFT(a) (((a).tail-(a).head-1)&(TTY_BUF_SIZE-1))
// 缓冲区最后一个位置
#define LAST(a) ((a).buf[(TTY_BUF_SIZE-1)&((a).head-1)])
// 缓冲区满
#define FULL(a) (!LEFT(a))
// 缓冲区已存放字符长度
#define CHARS(a) (((a).head-(a).tail)&(TTY_BUF_SIZE-1))
// 从queue队列取一个字符, tail处, tail += 1
#define GETCH(queue,c) \
(void)({c=(queue).buf[(queue).tail];INC((queue).tail);})
// 往queue放一个字符, head处, head += 1
#define PUTCH(c,queue) \
(void)({(queue).buf[(queue).head]=(c);INC((queue).head);})

// 判断终端键盘字符类型
#define INTR_CHAR(tty) ((tty)->termios.c_cc[VINTR])		// 发送中断信号SIGINT, 中断字符
#define QUIT_CHAR(tty) ((tty)->termios.c_cc[VQUIT])		// 发送中断信号SIGQUIT, 退出字符
#define ERASE_CHAR(tty) ((tty)->termios.c_cc[VERASE])	// Backspace characters, 删除字符
#define KILL_CHAR(tty) ((tty)->termios.c_cc[VKILL])		// 删除一行字符
#define EOF_CHAR(tty) ((tty)->termios.c_cc[VEOF])		// 文件结束符
#define START_CHAR(tty) ((tty)->termios.c_cc[VSTART])	// 开始恢复输出字符
#define STOP_CHAR(tty) ((tty)->termios.c_cc[VSTOP])		// 停止输出字符
#define SUSPEND_CHAR(tty) ((tty)->termios.c_cc[VSUSP])	// 发送信号SIIGTSTP, 挂起字符

struct tty_struct {
	struct termios termios;		// 终端io属性和控制字符数据结构
	int pgrp;					// 所属进程组
	int stopped;				// 停止标记
	void (*write)(struct tty_struct * tty);		// write函数指针
	struct tty_queue read_q;		// 读队列
	struct tty_queue write_q;		// 写队列
	struct tty_queue secondary;		// cooked 格式化后标准字符
	};

extern struct tty_struct tty_table[];	// tty结构数组

// 默认终端termios结构中c_cc[]的初始值, 若定义了\0, 禁止使用对应的特殊字符, 8进制表示
/*	intr=^C		quit=^|		erase=del	kill=^U
	eof=^D		vtime=\0	vmin=\1		sxtc=\0
	start=^Q	stop=^S		susp=^Z		eol=\0
	reprint=^R	discard=^U	werase=^W	lnext=^V
	eol2=\0
*/
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

void rs_init(void);
void con_init(void);	// 控制终端初始化
void tty_init(void);	// tty 初始化

int tty_read(unsigned c, char * buf, int n);
int tty_write(unsigned c, char * buf, int n);

void rs_write(struct tty_struct * tty);
void con_write(struct tty_struct * tty);

void copy_to_cooked(struct tty_struct * tty);

#endif
