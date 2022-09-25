/*
 *  linux/kernel/system_call.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  system_call.s  contains the system-call low-level handling routines.
 * This also contains the timer-interrupt handler, as some of the code is
 * the same. The hd- and flopppy-interrupts are also here.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call. Ordinary interrupts
 * don't handle signal-recognition, as that would clutter them up totally
 * unnecessarily.
 *  包含了系统调用底层处理, 时钟中断处理, 硬盘中断等处理, 在每次时钟中断和系统调用之后处理信号, 其他中断不处理页没必要处理信号
 * Stack layout in 'ret_from_system_call': 系统调用返回时的堆栈内容
 *
 *	 0(%esp) - %eax
 *	 4(%esp) - %ebx
 *	 8(%esp) - %ecx
 *	 C(%esp) - %edx
 *	10(%esp) - %fs
 *	14(%esp) - %es
 *	18(%esp) - %ds
 *	1C(%esp) - %eip
 *	20(%esp) - %cs
 *	24(%esp) - %eflags
 *	28(%esp) - %oldesp
 *	2C(%esp) - %oldss
 */

SIG_CHLD	= 17	# 定义SIG_CHLD信号, 子进程终止或者结束触发

EAX		= 0x00		# 堆栈中各个寄存器的偏移位置
EBX		= 0x04
ECX		= 0x08
EDX		= 0x0C
FS		= 0x10
ES		= 0x14
DS		= 0x18
EIP		= 0x1C
CS		= 0x20
EFLAGS		= 0x24
OLDESP		= 0x28		# 特权级变化栈切换, 用户栈指针保存在内核态栈中
OLDSS		= 0x2C

state	= 0		# these are offsets into the task-struct. 进程状态
counter	= 4		# 进程运行时间片滴答数
priority = 8	# 进程运行优先级， 初始 priority = counter, 重置 priority = counter / 2 + priority
signal	= 12	# 信号位图, 每一个比特表示一种信号
sigaction = 16		# MUST be 16 (=len of sigaction) sigaction 结构数据长度为16字节
blocked = (33*16)	# block住的信号位图偏移量

# offsets within sigaction
sa_handler = 0		# 信号处理handler描述符
sa_mask = 4			# 信号屏蔽码
sa_flags = 8		# 信号集合
sa_restorer = 12	# 恢复函数指针

nr_system_calls = 74	# 系统调用总数

/*
 * Ok, I get parallel printer interrupts while using the floppy for some
 * strange reason. Urgel. Now I just ignore them.
 */
 # 定义入口点标识符
.globl system_call,sys_fork,timer_interrupt,sys_execve
.globl hd_interrupt,floppy_interrupt,parallel_interrupt
.globl device_not_available, coprocessor_error

.align 2		# 2^2 四字节对齐
bad_sys_call:	# 中断调用出错
	movl $-1,%eax
	iret
.align 2
reschedule:		# 重新执行调度
	pushl $ret_from_sys_call
	jmp schedule
# int 0x80 linux 系统调用入口点, eax 为调用号
.align 2
system_call:
	cmpl $nr_system_calls-1,%eax	# 调用号 > system_call_table的长度, eax = -1并退出
	ja bad_sys_call
	push %ds		# 保存原寄存器的值
	push %es
	push %fs
	pushl %edx
	pushl %ecx		# push %ebx,%ecx,%edx as parameters, C ABI 调用约定从左往右依次入栈
	pushl %ebx		# to the system call
	movl $0x10,%edx		# set up ds,es to kernel space 指向内核全局描述符表中数据段描述符
	mov %dx,%ds
	mov %dx,%es
	movl $0x17,%edx		# fs points to local data space 指向局部描述符表中进程数据段描述符
	mov %dx,%fs
	call *sys_call_table(,%eax,4)	# *([sys_call_table + %eax * 4]) 间接调用指定功能的 C 函数
	pushl %eax						# 系统调用返回值入栈
	# 检查当前进程的状态, state != 0重新调度, state = 0 && 时间片 = 0 也调度
	movl current,%eax
	cmpl $0,state(%eax)		# state
	jne reschedule
	cmpl $0,counter(%eax)		# counter
	je reschedule
# 从系统调用C函数返回后, 对信号的处理
ret_from_sys_call:
	movl current,%eax		# task = task[0] cannot have signals
	cmpl task,%eax			# idle进程0不必处理信号
	je 3f					# 向前跳转到标号3退出中断处理
	# 原调用函数代码段描述符与0x000f(RPL = 3, 局部描述符表, 第一个段(代码段))比较, 是否为用户进程
	cmpw $0x0f,CS(%esp)		# was old code segment supervisor ?
	jne 3f					# 不是用户态进程, forward到3号标签退出, 否则就是某个中断服务
	# 原堆栈段选择符与0x17(用户堆栈段)
	cmpw $0x17,OLDSS(%esp)		# was stack segment = 0x17 ?
	jne 3f					# 也不是用户进程, 退出
	# 处理当前进程的信号
	movl signal(%eax),%ebx	# 获取信号位图ebx, 每一位代表一种信号, 总共32种信号
	movl blocked(%eax),%ecx	# 获取block住的信号位图ecx
	notl %ecx				# ecx 按位取反
	andl %ebx,%ecx			# 获取可用的信号位图
	bsfl %ecx,%ecx			# 从低位0开始扫描位图, 有1则ecx保留该位的偏移值(0-31), 没有信号则forward 标号3
	je 3f
	btrl %ecx,%ebx			# ebx原位图信息最小信号值置0复位
	movl %ebx,signal(%eax)	# 重新保存current->signal 位图信息
	incl %ecx				# 将信号调整为从1开始的数 (1-32), 信号值入栈作为do_signal的参数之一
	pushl %ecx
	call do_signal			# 调用 C 信号处理函数
	popl %eax

3:	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	pop %fs
	pop %es
	pop %ds
	iret

# 80387 fpu检测错误时, 通过ERROR引脚通知cpu
.align 2
coprocessor_error:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax		# ds,es指向全局描述符表数据段描述符
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax		# fs 指向局部描述符表中用户进程数据段
	mov %ax,%fs
	pushl $ret_from_sys_call	# 中断调用返回的地址入栈
	jmp math_error

# 设备不存在或者80387 fpu不存在
.align 2
device_not_available:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	pushl $ret_from_sys_call
	clts				# clear TS so that we can use math 
	movl %cr0,%eax
	testl $0x4,%eax			# EM (math emulation bit)CR0的EM置0
	je math_state_restore
	pushl %ebp
	pushl %esi	
	pushl %edi
	call math_emulate
	popl %edi
	popl %esi
	popl %ebp
	ret

# int 0x20 时钟中断处理, 中断频率设置为100HZ即10ms触发一次时钟
# 定时芯片8253/8254 在sched.c 初始化了
.align 2
timer_interrupt:
	push %ds		# save ds,es and put kernel data space
	push %es		# into them. %fs is used by _system_call
	push %fs
	pushl %edx		# we save %eax,%ecx,%edx as gcc doesn't
	pushl %ecx		# save those across function calls. %ebx
	pushl %ebx		# is saved as we use that in ret_sys_call
	pushl %eax
	movl $0x10,%eax	# ds,es 指向全局描述符表数据段
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax	# fs指向局部描述符表用户进程数据段
	mov %ax,%fs
	incl jiffies		# jiffies每10ms加1
	movb $0x20,%al		# EOI to interrupt controller #1 发送中断结束EOI给8259控制器结束该硬件中断
	outb %al,$0x20
	movl CS(%esp),%eax	# 获取系统调用代码选择符cs
	andl $3,%eax		# %eax is CPL (0 or 3, 0=supervisor) 用当前特权级作为参数入栈
	pushl %eax
	call do_timer		# 'do_timer(long CPL)' does everything from
	addl $4,%esp		# task switching to accounting ...
	jmp ret_from_sys_call
.align 2

# eip[0] = eip, eip[3] = esp, 化身新进程骚操作
sys_execve:
	lea EIP(%esp),%eax	# eax指向堆栈中保存的用户进程eip指针处 = EIP + %esp
	pushl %eax
	call do_execve		# fs/exec.c
	addl $4,%esp
	ret
# 创建子进程, system_call 功能 2
.align 2
sys_fork:
	call find_empty_process		# 获取一个新的进程号pid
	testl %eax,%eax				# eax中返回进程号pid, 为 -则退出
	js 1f
	push %gs
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call copy_process			# 调用C函数 copy_process 设置内存页只读,为写时复制埋点 fork.c
	addl $20,%esp				# 丢弃这里所有的入栈内容
1:	ret

# int 0x2E 硬盘中断处理函数, 响应硬件中断请求IRQ14
# 当请求的硬盘操作完成或出错发出此中断信号 blk_drv/hd.c
hd_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax		# ds,es 指向全局描述符表数据段
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax		# fs 指向局部描述符表的进程数据段
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0xA0		# EOI to interrupt controller #1  向8259A从中断控制芯片发送结束硬件中断指令EOI
	jmp 1f			# give port chance to breathe  jmp 起到延时作用
1:	jmp 1f
1:	xorl %edx,%edx
	xchgl do_hd,%edx	# 获取do_hd函数指针放入edx中, 并置do_hd 为NULL
	testl %edx,%edx		# 判断edx是否为空, 为空则指向unexpected_hd_interrupt用于显示出错信息
	jne 1f
	movl $unexpected_hd_interrupt,%edx
1:	outb %al,$0x20	# 向8259A主中断控制芯片发送EOI指令,并调用edx指向的函数 read_intr/write_intr/unexpected_hd_interrupt
	call *%edx		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

# dont care, same as hd_interrupt
floppy_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0x20		# EOI to interrupt controller #1
	xorl %eax,%eax
	xchgl do_floppy,%eax
	testl %eax,%eax
	jne 1f
	movl $unexpected_floppy_interrupt,%eax
1:	call *%eax		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret
# int 0x27 并口中断处理, 对应硬件中断请求IRQ7, 当前版本未实现, 这里只是发送EOI指令
parallel_interrupt:
	pushl %eax
	movb $0x20,%al
	outb %al,$0x20
	popl %eax
	iret
