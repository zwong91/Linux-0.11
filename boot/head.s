/*
 *  linux/boot/head.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  head.s contains the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */
.text
.globl idt,gdt,pg_dir,tmp_floppy_area
pg_dir:		# 页目录将会存放这里0x0000_0000
.globl startup_32
startup_32:
	movl $0x10,%eax # 0b0000_0000_00010:0 :00 描述符表项的选择符, 请求特权级,全局描述符, 表项第2项数据段描述符
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs # 置ds,es,fs,gs中的选择符为0x10全局描述符表的数据段
	lss stack_start,%esp # 将堆栈放置在stack_start指向的user_stack[1024]的边界上 _stack_start = ss:esp
	call setup_idt	# 新的中断描述符表
	call setup_gdt	# 新的全局描述符表
	movl $0x10,%eax		# reload all the segment registers
	mov %ax,%ds		# after changing gdt. CS was already
	mov %ax,%es		# reloaded in 'setup_gdt'
	mov %ax,%fs
	mov %ax,%gs
	lss stack_start,%esp # 保护模式下ss:esp 0x10:user_stack[1k] sched.c+67-72 内核程序自己使用的4K大小堆栈
	xorl %eax,%eax
1:	incl %eax		# check that A20 really IS enabled
	movl %eax,0x000000	# loop forever if it isn't
	cmpl %eax,0x100000 # 测试A20地址线是否开启,0x0000_0000和0x10_0000 1M比较, 一直相同的话 loop
	je 1b			# 1b 表示向后backward跳转到标号1, 若5f 表示向前forward跳转到标号5

/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 */
	movl %cr0,%eax		# check math chip
	andl $0x80000011,%eax	# Save PG,PE,ET
/* "orl $0x10020,%eax" here for 486 might be good */
	orl $2,%eax		# set MP
	movl %eax,%cr0
	call check_x87  # 287 387老的辅助数学运算浮点等芯片, 现在都是继承到CPU里面了
	jmp after_page_tables

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
check_x87:
	fninit
	fstsw %ax
	cmpb $0,%al
	je 1f			/* no coprocessor: have to set bits */
	movl %cr0,%eax
	xorl $6,%eax		/* reset MP, set EM */
	movl %eax,%cr0
	ret
.align 2	# 调整到地址最后两位为0, 即按4字节方式对齐内存地址
1:	.byte 0xDB,0xE4		/* fsetpm for 287, ignored by 387 */
	ret

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 *  中断描述符表项 8字节 Gate Descriptor 0-1, 6-7字节为偏移量, 2-3字节为选择符, 4-5字节为一些标志  
 */
setup_idt:
	lea ignore_int,%edx		# 将ignore_int的有效地址值 = edx
	movl $0x00080000,%eax	# 将选择符0x0008置于eax的高16位中
	movw %dx,%ax		/* selector = 0x0008 = cs */
	movw $0x8E00,%dx	/* interrupt gate - dpl=0, present 低4字节的值 */

	lea idt,%edi
	mov $256,%ecx
rp_sidt:
	movl %eax,(%edi)	# 将默认中断描述符存入表中
	movl %edx,4(%edi)
	addl $8,%edi		# edi指向表中的下一项
	dec %ecx
	jne rp_sidt
	lidt idt_descr		# 加载中断描述符表寄存器值
	ret

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */
setup_gdt:
	lgdt gdt_descr	# 加载全局描述符表寄存器
	ret

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 */
.org 0x1000		# 从0x1000开始为第一个页表， 0x0000存放页目录
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000 	# 定义下面的内存数据块从偏移的0x5000开始
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */
tmp_floppy_area:
	.fill 1024,1,0

/*
*	pushl用于为调用init/main.c程序和返回准备的, 手动模拟中断压栈, 中断返回
*/
after_page_tables:
	pushl $0		# These are the parameters to main :-) envp -> SS
	pushl $0		# argv 指针		-> ESP
	pushl $0		# argc值		-> EFLAGS
	pushl $L6		# return address for main, if it decides to. main返回跳转到L6  -> CS
	pushl $main		# 将main.c的地址压入栈
	jmp setup_paging  # setup_paging完成 ret返回的将main.c弹出, 跳转到main.c了, 骚  -> EIP
L6:
	jmp L6			# main should never return here, but
				# just in case, we know what happens.

/* This is the default interrupt "handler" :-) */
int_msg:
	.asciz "Unknown interrupt\n\r"
.align 2	# 4字节方式对齐内存地址
ignore_int:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds	# ds,es,fs,gs虽然是16位寄存器,但入栈以32位的形式即占用4字节的堆栈空间
	push %es
	push %fs

	movl $0x10,%eax # 使ds,es,fs段选择符指向gdt表的数据段
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs

	pushl $int_msg	# printk函数的参数指针入栈
	call printk		# /kernel/printl.c
	popl %eax

	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret			# 中断返回


/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won't guarantee that's all :-( )
 * 在内存物理地址0x0处开始存放1页4KB页目标表, 4页页表 ,页目标表系统所有进程公用的,
 * 这里的4页页表则是内核专用, 对于新的进程则是主内存区申请页面存放页表
 */
.align 2	# 按 4字节对齐
setup_paging:
	# 首先对5页=1页目录+4页页表 内存清零
	movl $1024*5,%ecx		/* 5 pages - pg_dir+4 page tables */
	xorl %eax,%eax
	xorl %edi,%edi			/* pg_dir is at 0x000 */
	cld;rep;stosl
	# 页目录表项和页表中的项结构都是4个字节位1项
	# 第一个页表地址 = 0x0000_0fff & 0xffff_f000 = 0x1000
	# 第一个页表属性标志 = 0x0000_1007 & 0x0000_0fff = 0x07
	movl $pg0+7,pg_dir		/* 0x0000_1007 set present bit/user r/w */
	movl $pg1+7,pg_dir+4		/*  --------- " " --------- */
	movl $pg2+7,pg_dir+8		/*  --------- " " --------- */
	movl $pg3+7,pg_dir+12		/*  --------- " " --------- */
	# 4 个页表中所有项为 4(页表) * 1024(项) = 4096 项 0-0xfff => 4096 *4KB = 16MB物理内存
	# 最后一页的最后一项edi开始填写
	movl $pg3+4092,%edi
	movl $0xfff007,%eax	 /*最后一项对应物理内存地址为0xfff007 = 0xfff000 + 属性标志7 = 16Mb - 4096 + 7 (r/w user,p) */
	std				# 方向标志, edi递减4字节
1:	stosl			/* fill pages backwards - more efficient :-) */
	subl $0x1000,%eax	# 每填写好一项, 地址减0x1000
	jge 1b				# 如果小于0说明全填写好了
	cld
	# 设置页目录表cr3寄存器的值, 指向页目录表
	xorl %eax,%eax		/* pg_dir is at 0x0000 */
	movl %eax,%cr3		/* cr3 - page directory start */
	movl %cr0,%eax
	orl $0x80000000,%eax	# PG 位置1
	movl %eax,%cr0		/* set paging (PG) bit 启用分页 */
	ret			/* this also flushes prefetch-queue, 另一个作用是将堆栈中main程序地址弹出,并开始运行/init/main.c */

.align 2	# 按4字节方式对齐内存地址边界
.word 0
# lidt指令的6字节操作数: 长度, 基址
idt_descr:
	.word 256*8-1		# idt contains 256 entries
	.long idt
.align 2
.word 0
# lgdt 指令的6字节操作数: 长度, 基址
gdt_descr:
	.word 256*8-1		# so does gdt (not that that's any
	.long gdt		# magic number, but it works for me :^)

	.align 8		# 按 8字节方式对齐内存地址边界
idt:	.fill 256,8,0		# idt is uninitialized 256项, 每项8字节, 填0

gdt:	.quad 0x0000000000000000	/*第一项为空保留 NULL descriptor */
	.quad 0x00c09a0000000fff	/* 代码段描述符, 0x08, 内核代码段最大长度16Mb */
	.quad 0x00c0920000000fff	/* 数据段描述符16Mb 0x10, 内核数据段最大长度16Mb*/
	.quad 0x0000000000000000	/* 系统保留 TEMPORARY - don't use */
	.fill 252,8,0			/* 预留252项空间, 后续进程局部描述表LDT和对应的任务状态符TSS, space for LDT's and TSS's etc */
