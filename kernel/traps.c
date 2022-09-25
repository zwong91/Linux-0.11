/*
 *  linux/kernel/traps.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'Traps.c' handles hardware traps and faults after we have saved some
 * state in 'asm.s'. Currently mostly a debugging-aid, will be extended
 * to mainly kill the offending process (probably by giving it a signal,
 * but possibly by killing it outright if necessary).
 */
#include <string.h> 

#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

// 取段seg中地址addr处的一个字节
// 参数: seg 段选择符, addr 段内指定的内存地址
// 输出: $0 - eax(_res), 输入 %1 - eax(esg), %2 -段内地址(*(addr))
#define get_seg_byte(seg,addr) ({ \
register char __res; \
__asm__("push %%fs;mov %%ax,%%fs;movb %%fs:%2,%%al;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

// 取段seg中地址addr处的4个字节
// 参数: seg 段选择符, addr 段内指定的内存地址
// 输出: $0 - eax(_res), 输入 %1 - eax(esg), %2 -段内地址(*(addr))
#define get_seg_long(seg,addr) ({ \
register unsigned long __res; \
__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

// 取fs段寄存器的值(选择符)
// 输出: $0 - eax(__res)
#define _fs() ({ \
register unsigned short __res; \
__asm__("mov %%fs,%%ax":"=a" (__res):); \
__res;})

// 以下定义了一些函数原型
int do_exit(long code);

void page_exception(void);  // page_fault  mm/page.s

// 定义了一些中断处理函数原型, 用于在trap_init中设置对应的中断描述符
void divide_error(void);		// kernel/asm.s, 下同
void debug(void);
void nmi(void);
void int3(void);
void overflow(void);
void bounds(void);
void invalid_op(void);
void device_not_available(void);	// kernel/system_call.s
void double_fault(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment(void);
void general_protection(void);
void page_fault(void);			// mm/pages.s
void coprocessor_error(void);	// kernel/system_call.s
void reserved(void);
void parallel_interrupt(void);	// kernel/system_call.s
void irq13(void);				// 80387 fpu 中断处理

// 输出异常调试信息堆栈
static void die(char * str,long esp_ptr,long nr)
{
	long * esp = (long *) esp_ptr;
	int i;
	// 输出出错中断名称, 错误号
	printk("%s: %04x\n\r",str,nr&0xffff);
	// 显示当前调用进程的cs:eip, ss:esp的值
	// esp[1] = 段选择符cs, esp[0] = eip
	// esp[2] = eflags
	// esp[4] = 原ss, esp[3] = 原esp
	printk("EIP:\t%04x:%p\nEFLAGS:\t%p\nESP:\t%04x:%p\n",
		esp[1],esp[0],esp[2],esp[4],esp[3]);
	// fs寄存器的值(描述符)
	printk("fs: %04x\n",_fs());
	// 段基址和段长度信息
	printk("base: %p, limit: %p\n",get_base(current->ldt[1]),get_limit(0x17));
	// 如果堆栈在用户数据段, 输出16字节的堆栈内容
	if (esp[4] == 0x17) {
		printk("Stack: ");
		for (i=0;i<4;i++)
			printk("%p ",get_seg_long(0x17,i+(long *)esp[3]));
		printk("\n");
	}
	str(i); // 获取当前运行进程的进程号
	printk("Pid: %d, process nr: %d\n\r",current->pid,0xffff & i);
	// 10 字节的指令码
	for(i=0;i<10;i++)
		printk("%02x ",0xff & get_seg_byte(esp[1],(i+(char *)esp[0])));
	printk("\n\r");
	do_exit(11);		/* play segment exception */
}

// 以下以do_xx_xx 打头函数是asm.s 中对应的中断处理程序调用的 C 函数
void do_double_fault(long esp, long error_code)
{
	die("double fault",esp,error_code);
}

void do_general_protection(long esp, long error_code)
{
	die("general protection",esp,error_code);
}

void do_divide_error(long esp, long error_code)
{
	die("divide error",esp,error_code);
}

// 参数为进入中断后被顺序压入栈中的寄存器的值
void do_int3(long * esp, long error_code,
		long fs,long es,long ds,
		long ebp,long esi,long edi,
		long edx,long ecx,long ebx,long eax)
{
	int tr;

	__asm__("str %%ax":"=a" (tr):"0" (0));	// 取tr寄存器的值
	printk("eax\t\tebx\t\tecx\t\tedx\n\r%8x\t%8x\t%8x\t%8x\n\r",
		eax,ebx,ecx,edx);
	printk("esi\t\tedi\t\tebp\t\tesp\n\r%8x\t%8x\t%8x\t%8x\n\r",
		esi,edi,ebp,(long) esp);
	printk("\n\rds\tes\tfs\ttr\n\r%4x\t%4x\t%4x\t%4x\n\r",
		ds,es,fs,tr);
	printk("EIP: %8x   CS: %4x  EFLAGS: %8x\n\r",esp[0],esp[1],esp[2]);
}

void do_nmi(long esp, long error_code)
{
	die("nmi",esp,error_code);
}

void do_debug(long esp, long error_code)
{
	die("debug",esp,error_code);
}

void do_overflow(long esp, long error_code)
{
	die("overflow",esp,error_code);
}

void do_bounds(long esp, long error_code)
{
	die("bounds",esp,error_code);
}

void do_invalid_op(long esp, long error_code)
{
	die("invalid operand",esp,error_code);
}

void do_device_not_available(long esp, long error_code)
{
	die("device not available",esp,error_code);
}

void do_coprocessor_segment_overrun(long esp, long error_code)
{
	die("coprocessor segment overrun",esp,error_code);
}

void do_invalid_TSS(long esp,long error_code)
{
	die("invalid TSS",esp,error_code);
}

void do_segment_not_present(long esp,long error_code)
{
	die("segment not present",esp,error_code);
}

void do_stack_segment(long esp,long error_code)
{
	die("stack segment",esp,error_code);
}

void do_coprocessor_error(long esp, long error_code)
{
	if (last_task_used_math != current)
		return;
	die("coprocessor error",esp,error_code);
}

void do_reserved(long esp, long error_code)
{
	die("reserved (15,17-47) error",esp,error_code);
}

// 异常中断程序初始化, 设置他们的中断向量
// set_trap_gate和set_system_gate都使用了中断描述符IDT中的 trap gate
// set_trap_gate设置的特权级为0内核态, set_system_gate设置的特权级为3用户态
void trap_init(void)
{
	int i;

	set_trap_gate(0,&divide_error);
	set_trap_gate(1,&debug);
	set_trap_gate(2,&nmi);
	set_system_gate(3,&int3);	/* int3-5 can be called from all */
	set_system_gate(4,&overflow);
	set_system_gate(5,&bounds);
	set_trap_gate(6,&invalid_op);
	set_trap_gate(7,&device_not_available);
	set_trap_gate(8,&double_fault);
	set_trap_gate(9,&coprocessor_segment_overrun);
	set_trap_gate(10,&invalid_TSS);
	set_trap_gate(11,&segment_not_present);
	set_trap_gate(12,&stack_segment);
	set_trap_gate(13,&general_protection);
	set_trap_gate(14,&page_fault);
	set_trap_gate(15,&reserved);
	set_trap_gate(16,&coprocessor_error);
	for (i=17;i<48;i++)
		set_trap_gate(i,&reserved);
	set_trap_gate(45,&irq13);	// 80387 fpu 0x2d = 45, 允许中断
	outb_p(inb_p(0x21)&0xfb,0x21);	// 允许8259A 主中断控制芯片IRQ2中断请求
	outb(inb_p(0xA1)&0xdf,0xA1);	// 允许8259A 从中断控制芯片IRQ13中断请求
	set_trap_gate(39,&parallel_interrupt);	// 0x27 = 39, 并口中断请求
}
