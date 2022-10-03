/*
 * 'kernel.h' contains some often-used function prototypes etc
 */
// 验证内存块是否有效, 只读/对齐  kernel/fork.c
void verify_area(void * addr,int count);
// 显示内核出错, 进入死循环
void panic(const char * str);
// 标准输出函数 init/main.c
int printf(const char * fmt, ...);
// 内核专用输出函数
int printk(const char * fmt, ...);
// 往tty上写指定长度的字符串
int tty_write(unsigned ch,char * buf,int count);

// 通用内核内存分配 lib/molloc.c
void * malloc(unsigned int size);
// 释放指定对象占用的内存 lib/molloc.c
void free_s(void * obj, int size);

#define free(x) free_s((x), 0)

/*
 * This is defined as a macro, but at some point this might become a
 * real subroutine that sets a flag if it returns true (to do
 * BSD-style accounting where the process is flagged if it uses root
 * privs).  The implication of this is that you should do normal
 * permissions checks first, and check suser() last.
 */

// 常规权限检查, 是否为超级用户
#define suser() (current->euid == 0)

