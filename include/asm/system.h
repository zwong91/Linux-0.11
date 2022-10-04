
// 骚操作, 利用iret指令实现从cp 0 到cp 3变更和堆栈切换, 手动切换到idle 0 执行, 
// 这里的iret不会造成CPU去执行任务切换, eflags NT位 已经在sehed_init被置0了
/**
 * 切换到用户模式运行
*/
// #define move_to_user_mode() \
// __asm__ ("movl %%esp,%%eax\n\t" \	// 保存堆栈指针esp到eax中
// 	"pushl $0x17\n\t" \				// SS 入栈
// 	"pushl %%eax\n\t" \				// ESP 入栈
// 	"pushfl\n\t" \					// eflags 入栈
// 	"pushl $0x0f\n\t" \				// task 0 的代码段选择符cs入栈
// 	"pushl $1f\n\t" \				// 将foreward 1 下面的标号偏移地址eip入栈
// 	"iret\n" \						// 中断返回则会跳转到 标号1处
// 	"1:\tmovl $0x17,%%eax\n\t" \	// 开始执行 任务0, 初始化段寄存器指向当前局部描述符数据段
// 	"movw %%ax,%%ds\n\t" \
// 	"movw %%ax,%%es\n\t" \
// 	"movw %%ax,%%fs\n\t" \
// 	"movw %%ax,%%gs" \
// 	:::"ax")
#define move_to_user_mode() \
__asm__ ("movl %%esp,%%eax\n\t" \
	"pushl $0x17\n\t" \
	"pushl %%eax\n\t" \
	"pushfl\n\t" \
	"pushl $0x0f\n\t" \
	"pushl $1f\n\t" \
	"iret\n" \
	"1:\tmovl $0x17,%%eax\n\t" \
	"movw %%ax,%%ds\n\t" \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")

#define sti() __asm__ ("sti"::)		// 开中断
#define cli() __asm__ ("cli"::)		// 关中断
#define nop() __asm__ ("nop"::)		// 空操作

#define iret() __asm__ ("iret"::)	// 中断返回

/**
 * @brief 设置位于gate_addr的门描述符宏
 * 参数: gate_addr 描述符地址, type 描述符类型域值, dpl 描述符特权级, addr 内核代码段/数据段偏移地址
 * %0 - 由 dpl, type组成的类型标识
 * %1 - 描述符低4字节地址
 * %2 - 描述符高4字节地址
 * %3 - edx 程序偏移地址addr
 * %4 - eax 高字中含有段描述符0x8
*/
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \
	"movl %%edx,%2" \
	: \
	: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	"o" (*((char *) (gate_addr))), \
	"o" (*(4+(char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000))

/**
 * @brief 设置中断门函数
 * @param[in] n - 中断号, addr 中断程序偏移地址
 * &idt[n] 中断描述符表中断号n对应项的偏移值, 中断描述符类型 14, 特权级 0 
*/
#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

/**
 * @brief 设置陷阱门函数
 * @param[in] n - 中断号, addr 中断程序偏移地址
 * &idt[n] 中断描述符表中断号n对应项的偏移值, 中断描述符类型 15, 特权级 0 
*/
#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

/**
 * @brief 设置系统陷阱门函数
 * @param[in] n - 中断号, addr 中断程序偏移地址
 * &idt[n] 中断描述符表中断号n对应项的偏移值, 中断描述符类型 15, 特权级 3
 * 中断处理过程能被所有程序执行, 如 单步调试, 溢出, 除零等
*/
#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

// 段描述符格式, 没有使用
#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*((gate_addr) + 1) = ((base) & 0xff000000) | \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*(gate_addr) = (((base) & 0x0000ffff)<<16) | \
		((limit) & 0x0ffff); }

/*
 * @brief 全局描述符中设置任务状态段tss和局部描述符表ldt, 长度均为104字节
 * 参数: n - 全局描述符表中n项对应的地址, addr - tss/ldt 所在的内存基址, type - 描述符中标志类型
 * %0 - eax 地址addr
 * %1 - 描述符项n的地址
 * %2 - 描述符项n的地址 + 2 偏移处
 * %3 - 描述符项n的地址 + 4 偏移处
 * %4 - 描述符项n的地址 + 5 偏移处
 * %5 - 描述符项n的地址 + 6 偏移处
 * %6 - 描述符项n的地址 + 7 偏移处
*/
// #define _set_tssldt_desc(n,addr,type) \
// __asm__ ("movw $104,%1\n\t" \		// 将TSS/LDT 长度放入描述符第 0 -1 字节
// 	"movw %%ax,%2\n\t" \			// 将基址低字放入描述符第 2 -3 字节
// 	"rorl $16,%%eax\n\t" \			// 将基址高字右循环移入ax中,低字则进入高字处
// 	"movb %%al,%3\n\t" \			// 将基址高字中低字节放入描述符底4个字节
// 	"movb $" type ",%4\n\t" \		// 将标志类型字节放入描述符第5个字节
// 	"movb $0x00,%5\n\t" \			// 将描述符第6个字节置0
// 	"movb %%ah,%6\n\t" \			// 将基址高字中高字节放入描述符第7个字节
// 	"rorl $16,%%eax" \				// 再右循环16bit, eax恢复原值
// 	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
// 	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
// 	)
#define _set_tssldt_desc(n,addr,type) \
__asm__ ("movw $104,%1\n\t" \
	"movw %%ax,%2\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,%3\n\t" \
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x89")
#define set_ldt_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x82")

