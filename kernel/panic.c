/*
 *  linux/kernel/panic.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This function is used through-out the kernel (includeinh mm and fs)
 * to indicate a major problem.
 */
#define PANIC

#include <linux/kernel.h>
#include <linux/sched.h>

void sys_sync(void);	/* it's really int */

// Douglas Adams 《 Hitch hikers Guide to the Galaxy》 Don't Panic!
// 显示内核不可恢复错误, 并同步刷新缓冲区到磁盘文件系统, 然后进入死循环
void panic(const char * s)
{
	printk("Kernel panic: %s\n\r",s);
	if (current == task[0])
		printk("In swapper task - not syncing\n\r");
	else
		sys_sync();
	for(;;);
}
