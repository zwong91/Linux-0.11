/*
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <string.h>
#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

// 写页面验证, 若页面不可写, 则复制页面
extern void write_verify(unsigned long address);

// 新进程号pid, 由get_empty_process生成
long last_pid=0;

// 386 执行内核代码不理会用户空间内存是否页保护, 写时复制不起作用
// 486之后 CR0的标志WP位置1可达到本函数进程空间区域写前验证效果
// 对[addr, addr +size]以页对齐写前检查, 找出addr所在页的起始地址start + 进程数据段基址 = 3G线性空间地址
// 循环调用write_verify 对指定4K页面写前验证, 若页面只读则共享和复制页面(COW写时复制)
void verify_area(void * addr,int size)
{
	unsigned long start;

	start = (unsigned long) addr;
	size += start & 0xfff;	// 4K一页对齐, 计算start所在页起始位置和addr这中间的偏移量加上
	start &= 0xfffff000;	// start 指向所在页起始位置
	start += get_base(current->ldt[2]);	// start 进程逻辑地址 + 进程数据段在线性地址空间起始基址 = 指向线性地址空间起始地址
	while (size>0) {
		size -= 4096;
		write_verify(start);
		start += 4096;
	}
}

// 复制父进程内存页表, nr 新任务号, p 新的任务结构指针
// COW 机制, 并没有实际为进程分配物理内存页, 父子共享内存页面
int copy_mem(int nr,struct task_struct * p)
{
	unsigned long old_data_base,new_data_base,data_limit;
	unsigned long old_code_base,new_code_base,code_limit;
	// 获取当前进程LDT中代码段描述符和数据段描述符项中的限长字节数
	code_limit=get_limit(0x0f/*代码段选择符*/);
	data_limit=get_limit(0x17/*数据段选择符*/);
	// 获取当前进程代码段和数据段在线性地址空间中的基址
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	// 当前还不支持代码和数据分离
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
	// 设置新进程在线性地址空间的基址 = 64MB * 任务号
	new_data_base = new_code_base = nr * 0x4000000;
	p->start_code = new_code_base;
	// 继续用该值设置新进程LDT表中段描述符基址
	set_base(p->ldt[1],new_code_base);
	set_base(p->ldt[2],new_data_base);

	// 通过复制current父进程PGD和PTE,设置新进程的页目录表和页表
	if (copy_page_tables(old_data_base,new_data_base,data_limit)) {
		printk("free_page_tables: from copy_mem\n");
		free_page_tables(new_data_base,data_limit);
		return -ENOMEM;
	}
	return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */

/**
 * @brief 复制current父进程task_struct, 设置必要的寄存器, 整个复制数据段
 * 参数: 进入system_call.s开始, 直到调用sys_fork, 然后调用本函数前的压栈的
 * 
 * 1. CPU 执行中断指令压入用户栈 ss/esp/eflags/cs/eip
 * 2. 刚进入system_call.s 压入栈的ds/es/fs/edx/ecx/ebx
 * 3. 调用sys_call_table中sys_fork函数压入栈的 返回地址 none
 * 4. 调用copy_process之前压入栈的gs/esi/edi/ebp/nr(eax)
*/
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
		long ebx,long ecx,long edx,
		long fs,long es,long ds,
		long eip,long cs,long eflags,long esp,long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;
	// 为新进程分配1页内存, 占用task_struct slots槽位, 把当前进程task_struct复制给新进程
	p = (struct task_struct *) get_free_page();
	if (!p)
		return -EAGAIN;
	task[nr] = p;
	
	// NOTE!: the following statement now work with gcc 4.3.2 now, and you
	// must compile _THIS_ memcpy without no -O of gcc.#ifndef GCC4_3
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */

	// 把复制来的进程结构修改下作为自己的task_struct
	p->state = TASK_UNINTERRUPTIBLE;	// 新进程状态不可中断等待状态 not ready yet
	p->pid = last_pid;	// 新进程pid
	p->father = current->pid;	// 新进程的父进程pid
	p->counter = p->priority;	// 新进程CPU运行时间片初始值 15 滴答
	p->signal = 0;	 // 新进程重置信号bitmap
	p->alarm = 0;	 // 新进程重置报警定时值
	p->leader = 0;		/* 新进程会话leader process leadership doesn't inherit */
	p->utime = p->stime = 0; // 新进程在内核态和用户态的CPU运行时间
	p->cutime = p->cstime = 0; // 新进程的Child子进程内核态和用户态的CPU运行时间
	p->start_time = jiffies;	// 新进程开始运行当前系统时间滴答

	// 修改新进程TSS数据
	p->tss.back_link = 0;
	p->tss.esp0 = PAGE_SIZE + (long) p;		// esp0正好指向新进程页顶端
	p->tss.ss0 = 0x10;						// 0x10为内核数据段选择符, ss0:esp0 内核态栈指针
	p->tss.eip = eip;
	p->tss.eflags = eflags;
	p->tss.eax = 0;		// fork返回时新(子)进程返回值0
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;	// 段描述符仅16位有效的
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	// 每个任务(进程)在GDT表中都有一个任务TSS描述符和一个任务LDT描述符
	// 这里放置GDT中该任务的LDT描述符的选择符, CPU任务切换自动把TSS中LDT描述符的选择符加载到ldtr
	p->tss.ldt = _LDT(nr);
	p->tss.trace_bitmap = 0x80000000;	// 高16位有效
	// 如果使用了387 fpu 保存上下文
	if (last_task_used_math == current)
		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
	// 复制父进程的页表
	if (copy_mem(nr,p)) {
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}
	// 创建的子进程和父进程共享打开的文件, 共享inode 节点
	// 如果父进程有文件是open的, 对应文件引用次数+1
	for (i=0; i<NR_OPEN;i++)
		if ((f=p->filp[i]))
			f->f_count++;
	// 如果父进程的当前工作目录pwd/根目录root/可执行文件路径用到了, 引用次数+1
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
	// 设置GDT表新任务TSS段和LDT段描述符项, 这两个段限长均为104字节
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
	p->state = TASK_RUNNING;	/* 新进程设置就绪状态 do this last, just in case */
	return last_pid;
}

// 生成唯一的新进程pid, 返回任务数组索引 = 任务号
int find_empty_process(void)
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)
		if (!task[i])
			return i;
	return -EAGAIN;
}
