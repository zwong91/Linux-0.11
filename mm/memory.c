/*
 *  linux/mm/memory.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * demand-loading started 01.12.91 - seems it is high on the list of
 * things wanted, and it should be easy to implement. - Linus
 */

/*
 * Ok, demand-loading was easy, shared pages a little bit tricker. Shared
 * pages started 02.12.91, seems to work. - Linus.
 *
 * Tested sharing by executing about 30 /bin/sh: under the old kernel it
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * Also corrected some "invalidate()"s - I wasn't doing enough of them.
 */

// 内存初始化, 页目录和二级页表管理机制, 虚拟内存地址映射物理内存页
/// 内存分页管理机制:
// 当前版本中, 所有进程都使用一个页目录表, 每个进程都有自己的页表, 页目录表中每一项4B找到一个页表, 然后每一个页表项4B找到一页物理内存
// 1个页目标表 4KB = 1024 页 * 1024 物理页 * 4096 每个物理页 = 4GB线性地址空间
// 0.11版本 16MB物理内存 =  1个页目录表, 4个页表  = 线性地址 = 10(页目录项)_10(页表项)_12(页框偏移量)
#include <signal.h>

#include <asm/system.h>

#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

// 进程退出处理 kernel/exit.c
void do_exit(long code);

// OOM
static inline void oom(void)
{
	printk("out of memory\n\r");
	do_exit(SIGSEGV);
}

// 486 CPU 之后 将最近使用过的页表数据放在L1/L2/L3, 局部性原理
// 修改过页表信息后, 刷新缓存区, 这里重新加载页目录cr3 寄存器刷新, eax = 0即页目录线性地址=物理地址存放处
#define invalidate() \
__asm__("movl %%eax,%%cr3"::"a" (0))

/* these are not to be changed without changing head.s etc */
// 0.11 默认支持16MB, 若修改配合head.s 一起
#define LOW_MEM 0x100000		// 1M位置
#define PAGING_MEMORY (15*1024*1024)	// 分配页的内存最大15M
#define PAGING_PAGES (PAGING_MEMORY>>12)	// 分页的最大物理内存页数3840
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)	// 指定线性地址映射的页框的页号
#define USED 100		// 页占用标识

// 判断指定的线性地址是否在当前进程代码段中, 4095 = 0xfff, (((addr)+0xfff)&~0xfff) 取addr所在内存页的末端位置
#define CODE_SPACE(addr) ((((addr)+4095)&~4095) < \
current->start_code + current->end_code)

// 实际物理内存尾部位置
static long HIGH_MEMORY = 0;

// 从from处复制1页(4KB)内存到to处
#define copy_page(from,to) \
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024))

// mem_map 1字节代表1页内存是否占用, 对1M以上的main memory 内存页动态分配管理
static unsigned char mem_map [ PAGING_PAGES ] = {0,};

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 * 
 * 在physical memory获取一页空闲物理内存, 但并没有映射到某进程的地址空间去, put_page才映射
 * 其实不需要再使用put_page映射, 内存代码和数据16MB之前对等映射的物理空间
 * %0 - 返回值eax = 物理内存起始地址
 * %1 - ax = 0
 * %2 - LOW_MEM
 * %3 - cx = PAGING_PAGES
 * %4 - edi = mem_map+PAGING_PAGES-1 指向mem_map[]最后一个元素, 逆序遍历, bitmap里面放着是否占用mark 字节
 */
unsigned long get_free_page(void)
{
register unsigned long __res asm("ax");

__asm__("std ; repne ; scasb\n\t"	// 置方向位正向esi/edi++,  循环al = 0空闲 与 倒序对应的每个di 内容比较
	"jne 1f\n\t"					// 没有为 0 空闲 的字节, 跳到1 标号, 返回0结束
	"movb $1,1(%%edi)\n\t"			// [edi + 1] = 1, mem_map 页内存标记置为1占用
	"sall $12,%%ecx\n\t"			// ecx = 页面数 * 4K = 相对页起始地址
	"addl %2,%%ecx\n\t"				// ecx = ecx + LOW_MEM = 页面实际物理起始地址
	"movl %%ecx,%%edx\n\t"			// edx = 页面实际物理起始地址
	"movl $1024,%%ecx\n\t"			// ecx 置为 1024 计数器
	"leal 4092(%%edx),%%edi\n\t"	// edi = 4092 偏移量 + edx 页面实际物理最后四个字节处存放即该页面末端
	"rep ; stosl\n\t"				// 重复1024次,反方向将eax(0)的值拷贝到ES:EDI即指向的整页4K内存清零
	" movl %%edx,%%eax\n"			// eax 返回值为物理页面起始地址
	"1: cld"
	:"=a" (__res)
	:"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
	"D" (mem_map+PAGING_PAGES-1)
	);
return __res;
}

/*
 * Free a page of memory at physical address 'addr'. Used by
 * 'free_page_tables()' 释放物理内存addr处的一页
 */
void free_page(unsigned long addr)
{
	// addr 必须 >= 1M地址, addr < HIGH_MEMORY
	if (addr < LOW_MEM) return;
	if (addr >= HIGH_MEMORY)
		panic("trying to free nonexistent page");
	// 页面号 = (addr - LOW_MEM) / 4096, 页框页面号从0开始计数
	addr -= LOW_MEM;
	addr >>= 12;
	// bitmap置0空闲
	if (mem_map[addr]--) return;
	mem_map[addr]=0;
	panic("trying to double free page");
}

/*
 * This function frees a continuos block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
 * 根据指定的线性地址和页表个数, 释放页表指定的内存块并置页表项空闲, 1个页表映射4M物理内存
 * 
 * 页目录表: 线性地址/物理地址 [0-0x1000)处,  每项占4B, 1024 目录项 = 4K
 * 4个内核页表: [0x1000, 0x2000), [0x2000, 0x3000), [0x3000, 0x4000), [0x4000, 0x5000) 每项占4B, 1024 目录项 = 4K
 * 除进程0/1的各进程页表: 内核在进程创建时在physical memory分配, 每个页表项对应1页物理内存
 */
int free_page_tables(unsigned long from,unsigned long size)
{
	unsigned long *pg_table;
	unsigned long * dir, nr;

	// 检查from的线性基址是否在4M对齐边界 0x40_0000 - 1即0x3fffff = 0x11_1xxxx1, 减1永远记住程序员的世界从0开始的
	if (from & 0x3fffff)
		panic("free_page_tables called with wrong alignment");
	if (!from) // 0 试图释放内核和缓冲区的空间
		panic("Trying to free up swapper memory space");
	
	// 所占页表数 = 以4M -1 为跨度, 向上取整, 然后取高10位页目录索引即可 = 页目录项数
	size = (size + 0x3fffff) >> 22;
	//计算from的起始目录项, 页目录表存放0地址, 每个页目录项占4B, 实际目录项指针 = (目录项号(from >> 22)) << 2
	//0xffc = 0b1111_1111_1100, 因为只移动了20位, 最后00两位丢弃, 确保dir指针有效
	dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */

	// 此时size = 要释放的页表个数即页目录项数, dir 为起始目录指针
	// 循环操作页目录项, 依次释放每个页表中的页表项
	for ( ; size-->0 ; dir++) {
		if (!(1 & *dir))	// 当前目录项 P位 = 0, 该目录项未使用, 对应页表不存在内存中 continue
			continue;
		// 从目录项中取出页表地址
		pg_table = (unsigned long *) (0xfffff000 & *dir);
		// 对该页中1024个页表项处理, 释放 P = 1有效页表项对应4K物理内存
		for (nr=0 ; nr<1024 ; nr++) {
			if (1 & *pg_table)
				free_page(0xfffff000 & *pg_table);
			*pg_table = 0;
			pg_table++;
		}
		// 当一个页表所有页表项处理完毕则释放该页表自身占据的一页物理内存
		free_page(0xfffff000 & *dir);
		*dir = 0;
	}
	// 最后刷新页目录表到tr3 寄存器
	invalidate();
	return 0;
}

/*
 *  Well, here is one of the most complicated functions in mm. It
 * copies a range of linerar addresses by copying only the pages.
 * Let's hope this is bug-free, 'cause this one I don't want to debug :-)
 *
 * Note! We don't copy just any chunks of memory - addresses have to
 * be divisible by 4Mb (one page-directory entry), as this makes the
 * function easier. It's used only by fork anyway.
 *
 * NOTE 2!! When from==0 we are copying kernel space for the first
 * fork(). Then we DONT want to copy a full page-directory entry, as
 * that would lead to some serious memory waste - we just copy the
 * first 160 pages - 640kB. Even that is more than we need, but it
 * doesn't take any more memory - we don't copy-on-write in the low
 * 1 Mb-range, so the pages can be shared with the kernel. Thus the
 * special case for nr=xxxx.
 * 
 * 
 * 复制页目录表项和页表项, 复制后之前的物理内存被两套页表映射共享, 父子进程共享内存区, 设置页面只读, 直到有一个进程执行写操作COW
 * from 源线性地址, to 目的线性地址, size 需要复制的内存长度字节数
 */
int copy_page_tables(unsigned long from,unsigned long to,long size)
{
	unsigned long * from_page_table;
	unsigned long * to_page_table;
	unsigned long this_page;
	unsigned long * from_dir, * to_dir;
	unsigned long nr;

	if ((from&0x3fffff) || (to&0x3fffff))
		panic("copy_page_tables called with wrong alignment");
	from_dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
	to_dir = (unsigned long *) ((to>>20) & 0xffc);
	size = ((unsigned) (size+0x3fffff)) >> 22;
	for( ; size-->0 ; from_dir++,to_dir++) {
		if (1 & *to_dir)
			panic("copy_page_tables: already exist");
		if (!(1 & *from_dir))
			continue;
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
		// 对每个目录项申请1页内存保存对应页表
		if (!(to_page_table = (unsigned long *) get_free_page()))
			return -1;	/* Out of memory, see freeing */
		*to_dir = ((unsigned long) to_page_table) | 7 /*0b111 User, R/W, Present*/;
		nr = (from==0)?0xA0:1024; // 内核 idle 0 ->fork init1,  160页 = 640KB, 否则 init1 fork -> user process 1024页 = 4M
		for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
			this_page = *from_page_table;
			if (!(1 & this_page))
				continue;
			this_page &= ~2;	// ~2 = 0b01, R/W位置0
			*to_page_table = this_page;
			// 1M 以下的内核空间, idle 0 fork -> init 1, 此时热乎的复制的页面还在内核代码区域(sys_fork)
			// idle 0 内核进程 this_page 肯定小于1M哦, idle 0 的页仍然可读写, 只有在fork + execve (用户进程)并加载执行新程序代码才会出现 
			if (this_page > LOW_MEM) {
				*from_page_table = this_page;  // 源页表也只读, 对应父子进程内存页都是只读, 为后面 COW 伏笔
				// 计算页框页面号, 放入bitmap数组中已占用页
				this_page -= LOW_MEM;
				this_page >>= 12;
				mem_map[this_page]++; // 引用次数++ >1 Shared 状态
			}
		}
	}
	// 刷新页目录页表到 tr3 寄存器
	invalidate();
	return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory (either when trying to access page-table or
 * page.)
 * 
 * 把一物理内存页面反向映射到线性地址address处
 * 在相关的页目录项和页表项设置指定的页面, 若成功返回物理页面地址。
 * 在处理缺页异常do_no_page会调用此函数, 对页表修改时, 并不需要刷新CPU的TLB Transaction Lookaside Buffer, 
 * 无效的页不会缓存不需要刷新即不用调Invalidate()函数
 */
// 参数page 分配的physical memory中的某一页面/页帧/页框指针, address 线性地址
unsigned long put_page(unsigned long page,unsigned long address)
{
	unsigned long tmp, *page_table;

/* NOTE !!! This uses the fact that _pg_dir=0 */
	// 检查给定的物理内存页是否有效, physical memory <= 6M, main_memory_start = LOW_MEM
	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n",page,address);
	// 检查page页面是否已申请的页面, bitmap 中是否有置1
	if (mem_map[(page-LOW_MEM)>>12] != 1)
		printk("mem_map disagrees with %p at %p\n",page,address);
	// 计算address在页目录表中对应的目录项指针, 获取二级页表的地址
	page_table = (unsigned long *) ((address>>20) & 0xffc);
	if ((*page_table)&1) // P = 1, 指定的页表在内存中, 获取页表地址放在page_table
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
		if (!(tmp=get_free_page())) // 申请一空闲页给页表使用
			return 0;
		*page_table = tmp|7/* 7 - User、U/S, R/W*/;
		page_table = (unsigned long *) tmp;
	}
	// 在页表page_table中设置相关页表项内容, 该页表项在页表中索引 = (address>>12) & 0x3ff 位21 - 位12 的10 bits
	// 每个页表有 0 - 0x3ff 共1024项
	page_table[(address>>12) & 0x3ff] = page | 7/*U/S、R/W、P*/;
/* no need for invalidate */
	return page;
}

// 取消页写保护, 用于页异常中断写时复制COW, 内核创建进程时, 新进程和父进程共享同一个设置为Shared 代码和数据内存页
// 这些内存页均为只读页, 当新进程或父进程需要向内存页面写数据时, CPU触发写页面异常中断
// 内核首先判断要写的页面是否Shared, 不是Shared则设置页面可写然后退出
// 若是Shared状态, 重新申请新页并复制旧页面数据,  重置Shared
void un_wp_page(unsigned long * table_entry)
{
	unsigned long old_page,new_page;
	// 获取指定页表项中物理页面地址/页面号
	old_page = 0xfffff000 & *table_entry;
	// 原页面地址在physical memory中并且 bitmap 为1仅引用一次,不是Shared状态
	// 即不是内核进程, 该页面只被一个进程使用, 修改页为可写即可
	if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {
		*table_entry |= 2 /*0b10 R/W位,可写*/;
		// 页表被修改了刷新到cr3
		invalidate();
		return;
	}
	// mem_map[] > 1 页面是Shared状态, 在physical memory 申请新的内存页
	if (!(new_page=get_free_page()))
		oom();
	if (old_page >= LOW_MEM)
		mem_map[MAP_NR(old_page)]--; // 引用计数--, 取消Shared状态
	*table_entry = new_page | 7/*U/S、R/W、P*/;
	invalidate();
	copy_page(old_page,new_page);
}	

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 *
 * If it's in code space we exit with a segment error.
 * 只读页面写异常中断处理, 在page.s 中被调用
 * 参数 error_code CPU写异常中断错误码, address 线性地址
 */
void do_wp_page(unsigned long error_code,unsigned long address)
{
#if 0
/* we cannot do this yet: the estdio library writes to code space */
/* stupid, stupid. I really want the libc.a from GNU */
// 代码空间写操作, 傻逼
	if (CODE_SPACE(address))
		do_exit(SIGSEGV);
#endif
// 参数为线性地址address指向的页面在页表中的页表项指针
// 1. 计算页表项在页表中的偏移地址pte_offset = (address >> 12)页表索引值 << 2 & 0b_1111_1111_1100 = (address>>10) & 0xffc
// 2. 计算页目录项中页表的偏移地址pgd_offset = (address >> 22)页目录索引值 << 2 & 0b_1111_1111_1100 = (address>>20) & 0xffc
//  page_table = *((unsigned long *)pgd_offset) 取页目标表中表项中对应的页表内容即放着的不就是页表的物理地址
//  page_table = 0xfffff000 & page_table 4K对齐
// 3. 页表项指针table_entry = page_table + pte_offset
	un_wp_page((unsigned long *)
		(((address>>10) & 0xffc) + (0xfffff000 &
		*((unsigned long *) ((address>>20) &0xffc)))));

}

void write_verify(unsigned long address)
{
	unsigned long page;

	if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1))
		return;
	page &= 0xfffff000;
	page += ((address>>10) & 0xffc);
	if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */
		un_wp_page((unsigned long *) page);
	return;
}

void get_empty_page(unsigned long address)
{
	unsigned long tmp;

	if (!(tmp=get_free_page()) || !put_page(tmp,address)) {
		free_page(tmp);		/* 0 is ok - ignored */
		oom();
	}
}

/*
 * try_to_share() checks the page at address "address" in the task "p",
 * to see if it exists, and if it is clean. If so, share it with the current
 * task.
 *
 * NOTE! This assumes we have checked that p != current, and that they
 * share the same executable.
 */
static int try_to_share(unsigned long address, struct task_struct * p)
{
	unsigned long from;
	unsigned long to;
	unsigned long from_page;
	unsigned long to_page;
	unsigned long phys_addr;

	from_page = to_page = ((address>>20) & 0xffc);
	from_page += ((p->start_code>>20) & 0xffc);
	to_page += ((current->start_code>>20) & 0xffc);
/* is there a page-directory at from? */
	from = *(unsigned long *) from_page;
	if (!(from & 1))
		return 0;
	from &= 0xfffff000;
	from_page = from + ((address>>10) & 0xffc);
	phys_addr = *(unsigned long *) from_page;
/* is the page clean and present? */
	if ((phys_addr & 0x41) != 0x01)
		return 0;
	phys_addr &= 0xfffff000;
	if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM)
		return 0;
	to = *(unsigned long *) to_page;
	if (!(to & 1)) {
		if ((to = get_free_page()))
			*(unsigned long *) to_page = to | 7;
		else
			oom();
	}
	to &= 0xfffff000;
	to_page = to + ((address>>10) & 0xffc);
	if (1 & *(unsigned long *) to_page)
		panic("try_to_share: to_page already exists");
/* share them: write-protect */
	*(unsigned long *) from_page &= ~2;
	*(unsigned long *) to_page = *(unsigned long *) from_page;
	invalidate();
	phys_addr -= LOW_MEM;
	phys_addr >>= 12;
	mem_map[phys_addr]++;
	return 1;
}

/*
 * share_page() tries to find a process that could share a page with
 * the current one. Address is the address of the wanted page relative
 * to the current data space.
 *
 * We first check if it is at all feasible by checking executable->i_count.
 * It should be >1 if there are other tasks sharing this inode.
 */
static int share_page(unsigned long address)
{
	struct task_struct ** p;

	if (!current->executable)
		return 0;
	if (current->executable->i_count < 2)
		return 0;
	for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if (!*p)
			continue;
		if (current == *p)
			continue;
		if ((*p)->executable != current->executable)
			continue;
		if (try_to_share(address,*p))
			return 1;
	}
	return 0;
}

void do_no_page(unsigned long error_code,unsigned long address)
{
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block,i;

	address &= 0xfffff000;
	tmp = address - current->start_code;
	if (!current->executable || tmp >= current->end_data) {
		get_empty_page(address);
		return;
	}
	if (share_page(tmp))
		return;
	if (!(page = get_free_page()))
		oom();
/* remember that 1 block is used for header */
	block = 1 + tmp/BLOCK_SIZE;
	for (i=0 ; i<4 ; block++,i++)
		nr[i] = bmap(current->executable,block);
	bread_page(page,current->executable->i_dev,nr);
	i = tmp + 4096 - current->end_data;
	tmp = page + 4096;
	while (i-- > 0) {
		tmp--;
		*(char *)tmp = 0;
	}
	if (put_page(page,address))
		return;
	free_page(page);
	oom();
}

void mem_init(long start_mem, long end_mem)
{
	int i;

	HIGH_MEMORY = end_mem;
	for (i=0 ; i<PAGING_PAGES ; i++)
		mem_map[i] = USED;
	i = MAP_NR(start_mem);
	end_mem -= start_mem;
	end_mem >>= 12;
	while (end_mem-->0)
		mem_map[i++]=0;
}

void calc_mem(void)
{
	int i,j,k,free=0;
	long * pg_tbl;

	for(i=0 ; i<PAGING_PAGES ; i++)
		if (!mem_map[i]) free++;
	printk("%d pages free (of %d)\n\r",free,PAGING_PAGES);
	for(i=2 ; i<1024 ; i++) {
		if (1&pg_dir[i]) {
			pg_tbl=(long *) (0xfffff000 & pg_dir[i]);
			for(j=k=0 ; j<1024 ; j++)
				if (pg_tbl[j]&1)
					k++;
			printk("Pg-dir[%d] uses %d pages\n",i,k);
		}
	}
}
