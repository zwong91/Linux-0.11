/*
 *  linux/lib/_exit.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

/// @brief 内核使用的程序退出函数, 直接调用0x80, %1 - eax __NR_exit 功能号, %2 - ebx
/// @param exit_code 
void _exit(int exit_code)
{
	__asm__ __volatile__ ("int $0x80"::"a" (__NR_exit),"b" (exit_code));
}
