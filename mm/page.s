/*
 *  linux/mm/page.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * page.s contains the low-level page-exception code.
 * the real work is done in mm.c
 */

.globl page_fault	# 导出符号page_fault, 在trap.c 设置

# 缺页中断和页写保护COW处理, error_cde CPU中断0x14产生并压入栈, address 线性地址从 cr2 专门存放页错误获取 
page_fault:
	xchgl %eax,(%esp)	# eax = 取出错误码
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%edx		# 设置内核数据段选择符索引
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	movl %cr2,%edx		# 取出引起页异常的线性地址
	pushl %edx			# 参数 edx, eax 即线性地址, 错误码入栈
	pushl %eax
	testl $1,%eax		# Present = 1, jump not equal 为 0不是缺页引起异常则跳转到标号 1
	jne 1f
	call do_no_page		# 缺页异常处理
	jmp 2f
1:	call do_wp_page		# 只读页写保护COW
2:	addl $8,%esp
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret
