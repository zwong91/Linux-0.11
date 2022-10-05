/*
 *  linux/kernel/printk.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * When in kernel-mode, we cannot use printf, as fs is liable to
 * point to 'interesting' things. Make a printf with fs-saving, and
 * all is well.
 */
#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>

static char buf[1024];

extern int vsprintf(char * buf, const char * fmt, va_list args);
// 内核使用输出函数, 功能与libc中printf一样, 内核中不能直接使用用户模式的fs寄存器
// 由于实际tty_write中需要输出字符串取自fs用户程序数据段, printk显示信息在内核数据段ds, 得临时保存到fs再调用tty_write
int printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	// 格式化输出到buf
	i=vsprintf(buf,fmt,args);
	va_end(args);
	__asm__("push %%fs\n\t"	// 保存fs
		"push %%ds\n\t"
		"pop %%fs\n\t"		// fs = ds 指向内核数据段, 后入栈的ds弹出pop, 赋给fs寄存器咯
		"pushl %0\n\t"		// 三个参数入栈, 字符串长度
		"pushl $buf\n\t"	// buf 地址入栈
		"pushl $0\n\t"		// 值0 入栈, channel
		"call tty_write\n\t"	// 调用tty_write函数
		"addl $8,%%esp\n\t"	// 跳过丢弃两个入栈参数buf/channel
		"popl %0\n\t"		// 弹出eax 字符串长度返回
		"pop %%fs"			// 恢复fs寄存器
		::"r" (i):"ax","cx","dx");	// 通知编译器, ax/cx/dx可能已经改变
	return i;	// 返回字符串长度 eax
}
