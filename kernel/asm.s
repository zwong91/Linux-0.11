/*
 *  linux/kernel/asm.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * asm.s contains the low-level code for most hardware faults.
 * page_exception is handled by the mm, so that isn't here. This
 * file also handles (hopefully) fpu-exceptions due to TS-bit, as
 * the fpu must be properly saved/resored. This hasn't been tested.
 */

/*
中断异常处理底层代码int 0x0-int0x16, 缺页异常在mm里, fpu ts位异常
*/

# 以下是一些全局函数名的声明, 原型在traps.c
.globl divide_error,debug,nmi,int3,overflow,bounds,invalid_op
.globl double_fault,coprocessor_segment_overrun
.globl invalid_TSS,segment_not_present,stack_segment
.globl general_protection,coprocessor_error,irq13,reserved

# 以下是没有错误码中断异常
# 0x0除零异常, 把调用函数do_divide_error地址入栈
divide_error:
	pushl $do_divide_error
no_error_code:		# 无中断错误码处理入口
	xchgl %eax,(%esp)	# do_divide_error地址eax 交换入栈 eax = esp1, 中断前保存的为esp0 = esp1 -4
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs
	pushl $0		# error code 0作为错误码入栈
	lea 44(%esp),%edx
	pushl %edx		# 取堆栈中原来eip中断返回地址入栈, 作为esp3
	movl $0x10,%edx # 初始化段寄存器ds,es,fs,加载内核数据段描述符偏移0x10
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	call *%eax		# 调用引起本次异常的c函数如do_divide_error
	addl $8,%esp	# 弹出最后入栈的两个c函数参数, sp 重新指向fs入栈处esp2
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret			# esp 0

# 0x1 调试中断异常
debug:
	pushl $do_int3		# _do_debug C函数指针入栈, 下同
	jmp no_error_code
# 0x2 NMI 信号 非屏蔽中断异常, 随后的所有硬件中断被忽略
nmi:
	pushl $do_nmi
	jmp no_error_code
# 0x3 调试器断点中断异常
int3:
	pushl $do_int3
	jmp no_error_code
# 0x4 计算溢出 EFLAGS 0F标志位
overflow:
	pushl $do_overflow
	jmp no_error_code
# 0x5 操作数范围边界检查出错中断
bounds:
	pushl $do_bounds
	jmp no_error_code
# 0x6 无效操作码指令异常
invalid_op:
	pushl $do_invalid_op
	jmp no_error_code

# 0x9 fpu 浮点数指令操作数过大
coprocessor_segment_overrun:
	pushl $do_coprocessor_segment_overrun
	jmp no_error_code

# 0x15 其他Intel保留中断异常
reserved:
	pushl $do_reserved
	jmp no_error_code
# 0x45 = 0x20 + 13, 80387 fpu 继续执行指令之前, cpu响应本中断
irq13:
	pushl %eax
	xorb %al,%al
	outb %al,$0xF0
	movb $0x20,%al
	outb %al,$0x20		# 向8259 主中断控制芯片发生EOI中断结束信号
	jmp 1f				# 延时作用
1:	jmp 1f
1:	outb %al,$0xA0		# 再向8259 从中断控制芯片发送EOI中断结束信号
	popl %eax
	jmp coprocessor_error	# system_call.s

# 以下是有错误码的中断异常
# 0x8 中断返回前将错误码入栈, 不允许异常嵌套串行处理, 有错误码
double_fault:
	pushl $do_double_fault
error_code:
	xchgl %eax,4(%esp)		# error code <-> %eax eax原来的值error_code入栈(eax=error_code)  esp0
	xchgl %ebx,(%esp)		# &function <-> %ebx  ebx原来的值double_double_fault函数地址 = ebx  esp1 
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs			# esp2
	pushl %eax			# error code 错误码入栈
	lea 44(%esp),%eax		# offset 原程序中断返回处eip 入栈  esp3
	pushl %eax
	movl $0x10,%eax		# 0x10 内核数据段描述符偏移
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	call *%ebx			# 间接调用对应C函数, 2个参数已入栈
	addl $8,%esp		# 丢弃弹出入栈的2个C函数参数, 重新指向fs esp2
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret

# 0x10 无效的任务描述符TSS, TSS长度超过104字节, 异常在当前任务产生切换被终止了, 其他问题则在切换后的新任务产生异常
invalid_TSS:
	pushl $do_invalid_TSS
	jmp error_code

# 0x11 段描述符标识的段不在内存中
segment_not_present:
	pushl $do_segment_not_present
	jmp error_code

# 0x12 堆栈段范围溢出或者堆栈段不在内存中, 异常为程序分配更多的栈空间
stack_segment:
	pushl $do_stack_segment
	jmp error_code

# 0x13 一般性的保护中断异常, 未定义行为
general_protection:
	pushl $do_general_protection
	jmp error_code

# 0x14 page_fault 缺页异常  mm/page.s
# 0x20 时钟中断异常 kernel/system_call.s
# 0x80 系统调用 kernel/system_call.s

