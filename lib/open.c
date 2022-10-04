/*
 *  linux/lib/open.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

// 打开文件函数, 可能创建新文件
// %0 - eax 返回值
// %1 - eax 系统调用功能号 __NR_open
// %2 - ebx filename 文件名
// %3 - ecx flag 文件打开标志
// %4 - edx 随后参数文件属性mode
int open(const char * filename, int flag, ...)
{
	// register int res asm("ax") 变量在寄存器中更高效
	register int res;
	va_list arg;
	// 获取flag后面参数指针
	va_start(arg,flag);
	__asm__("int $0x80"
		:"=a" (res)
		:"0" (__NR_open),"b" (filename),"c" (flag),
		"d" (va_arg(arg,int)));
	if (res>=0)
		return res;
	errno = -res;
	return -1;
}
