#ifndef _HEAD_H
#define _HEAD_H

// 段描述符结构每项 8字节, 每个描述符表共256项
typedef struct desc_struct {
	unsigned long a,b;
} desc_table[256];

// 内存页目录数组, 从物理地址0开始, 每个目录项4字节
extern unsigned long pg_dir[1024];
// 中断描述符表, 全局描述符表
extern desc_table idt,gdt;

#define GDT_NUL 0		// 全局描述符表第0项, dummy
#define GDT_CODE 1		// 第一项内核代码段描述符项
#define GDT_DATA 2		// 第二项内核数据段描述符项
#define GDT_TMP 3		// 第三项 Linux没用到

#define LDT_NUL 0		// 局部描述符表第0项, dummy
#define LDT_CODE 1		// 第一项用户程序代码段描述符项
#define LDT_DATA 2		// 第二项用户程序数据段描述符项

#endif
