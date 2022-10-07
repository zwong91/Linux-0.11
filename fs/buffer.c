/*
 *  linux/fs/buffer.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'buffer.c' implements the buffer-cache functions. Race-conditions have
 * been avoided by NEVER letting a interrupt change a buffer (except for the
 * data, of course), but instead letting the caller do it. NOTE! As interrupts
 * can wake up a caller, some cli-sti sequences are needed to check for
 * sleep-on-calls. These should be extremely quick, though (I hope).
 * 
 * 实现高速缓冲区缓存LRU算法, 通过让调用者执行而不是中断处理过程改变缓冲区, 避免 Race-conditions
 */

/*
 * NOTE! There is one discordant note here: checking floppies for
 * disk change. This is where it fits best, I think, as it should
 * invalidate changed floppy-disk-caches.
 */

#include <stdarg.h>
 
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/io.h>

// ld设置的etext和edata值分别表示代码段后和数据段后的开始位置
// ld设置的end=data_start+datasize+bss_size =  bss段结束后位置 = 内核模块末端 = 高速缓存起始位置, 通过System.map查看下符号地址
extern int end;
extern void put_super(int);
extern void invalidate_inodes(int);

struct buffer_head * start_buffer = (struct buffer_head *) &end;
struct buffer_head * hash_table[NR_HASH]; // 307 素数均衡
static struct buffer_head * free_list;		// 空闲缓冲块链表头指针, 最近最多使用这端
static struct task_struct * buffer_wait = NULL; // 等待系统空闲缓冲块而睡眠的进程列表, 而 bh->b_wait等待指定缓冲区的进程等待队列头指针
int NR_BUFFERS = 0;	// 缓冲块个数, 内核初始化后不再改变的值

// 等待指定的缓冲块解锁
static inline void wait_on_buffer(struct buffer_head * bh)
{
	// 每个进程都在自己的TSS段保存了eflags寄存器的值, schedule时CPU当前eflags随之改变, 并不影响其他进程context响应中断
	cli();
	// bh指向的缓冲块已加锁, 让当前进程不可中断睡在该缓冲块等待队列b_wait
	while (bh->b_lock)
		sleep_on(&bh->b_wait);
	//这里缓冲块解锁后, 调用wake_up唤醒等待队列b_wait上所有进程
	sti();
}

// 同步设备和内存高速缓冲区中数据
// 第一步sync_inodes同步修改的 inode 到缓冲区 第二步 ll_rw_block 将缓冲区数据写盘请求
int sys_sync(void)
{
	int i;
	struct buffer_head * bh;

	sync_inodes();		/* write out inodes into buffers */
	bh = start_buffer;
	for (i=0 ; i<NR_BUFFERS ; i++,bh++) {
		wait_on_buffer(bh);
		if (bh->b_dirt)
			ll_rw_block(WRITE,bh);
	}
	return 0;
}

// 对指定设备如硬盘同步高速缓冲区和硬盘上数据
// 先同步指定dev的所有修改过的缓冲块数据, 再同步inodes 数据写入高速缓冲, 把inode引起变脏的缓冲块同步到设备, 高效, 安全
int sync_dev(int dev)
{
	int i;
	struct buffer_head * bh;

	bh = start_buffer;
	for (i=0 ; i<NR_BUFFERS ; i++,bh++) {
		if (bh->b_dev != dev)
			continue;
		wait_on_buffer(bh);
		//再判断一次dev/dirt, 睡眠期间该缓冲块有可能释放或者修改
		if (bh->b_dev == dev && bh->b_dirt)
			ll_rw_block(WRITE,bh);
	}
	sync_inodes();
	bh = start_buffer;
	for (i=0 ; i<NR_BUFFERS ; i++,bh++) {
		if (bh->b_dev != dev)
			continue;
		wait_on_buffer(bh);
		if (bh->b_dev == dev && bh->b_dirt)
			ll_rw_block(WRITE,bh);
	}
	return 0;
}

// 使指定设备再高速缓冲区中的数据无效, uptodate和dirt 置0
static void inline invalidate_buffers(int dev)
{
	int i;
	struct buffer_head * bh;

	bh = start_buffer;
	for (i=0 ; i<NR_BUFFERS ; i++,bh++) {
		if (bh->b_dev != dev)
			continue;
		wait_on_buffer(bh);
		//再判断一次dev, 睡眠期间该缓冲块有可能释放或者修改, 缓冲区是否指定设备的, 凉粉还是不是凉粉了
		if (bh->b_dev == dev)
			bh->b_uptodate = bh->b_dirt = 0;
	}
}

/*
 * This routine checks whether a floppy has been changed, and
 * invalidates all buffer-cache-entries in that case. This
 * is a relatively slow routine, so we have to try to minimize using
 * it. Thus it is called only upon a 'mount' or 'open'. This
 * is the best way of combining speed and utility, I think.
 * People changing diskettes in the middle of an operation deserve
 * to loose :-)
 *
 * NOTE! Although currently this is only for floppies, the idea is
 * that any additional removable block-device will use this routine,
 * and that mount/open needn't know that floppies/whatever are
 * special.
 */
void check_disk_change(int dev)
{
	int i;

	if (MAJOR(dev) != 2)
		return;
	if (!floppy_change(dev & 0x03))
		return;
	for (i=0 ; i<NR_SUPER ; i++)
		if (super_block[i].s_dev == dev)
			put_super(super_block[i].s_dev);
	invalidate_inodes(dev);
	invalidate_buffers(dev);
}


// hash表查询O(1)的, hash均匀性, 这里采用关键字除留余数法, xor 和取模
// dev 设备号, block 缓冲块号, 在buffer init的时建立key value映射关系
#define _hashfn(dev,block) (((unsigned)(dev^block))%NR_HASH)
#define hash(dev,block) hash_table[_hashfn(dev,block)]

// 从hash队列和空闲缓冲双向链表删除一个块
static inline void remove_from_queues(struct buffer_head * bh)
{
/* remove from hash-queue */
	if (bh->b_next)
		bh->b_next->b_prev = bh->b_prev;
	if (bh->b_prev)
		bh->b_prev->b_next = bh->b_next;
	if (hash(bh->b_dev,bh->b_blocknr) == bh)
		hash(bh->b_dev,bh->b_blocknr) = bh->b_next;
/* remove from free list */
	if (!(bh->b_prev_free) || !(bh->b_next_free))
		panic("Free block list corrupted");
	bh->b_prev_free->b_next_free = bh->b_next_free;
	bh->b_next_free->b_prev_free = bh->b_prev_free;
	if (free_list == bh)
		free_list = bh->b_next_free;
}

// 将缓冲块插入空闲链表尾部, 同时放入hash队列中
static inline void insert_into_queues(struct buffer_head * bh)
{
/* put at end of free list */
	bh->b_next_free = free_list;
	bh->b_prev_free = free_list->b_prev_free;
	free_list->b_prev_free->b_next_free = bh;
	free_list->b_prev_free = bh; // 以pre_xx为尾
/* put the buffer in new hash-queue if it has a device */
	bh->b_prev = NULL;
	bh->b_next = NULL;
	if (!bh->b_dev)
		return;
	// 第一次插入时 hash肯定为0
	bh->b_next = hash(bh->b_dev,bh->b_blocknr);
	hash(bh->b_dev,bh->b_blocknr) = bh;
	// fixed bh->b_next 为NULL
	if (bh->b_next)
		bh->b_next->b_prev = bh;
}

// 利用Hash O(1) 查找缓冲块
static struct buffer_head * find_buffer(int dev, int block)
{		
	struct buffer_head * tmp;

	for (tmp = hash(dev,block) ; tmp != NULL ; tmp = tmp->b_next)
		if (tmp->b_dev==dev && tmp->b_blocknr==block)
			return tmp;
	return NULL;
}

/*
 * Why like this, I hear you say... The reason is race-conditions.
 * As we don't lock buffers (unless we are readint them, that is),
 * something might happen to it while we sleep (ie a read-error
 * will force it bad). This shouldn't really happen currently, but
 * the code is ready.
 */
struct buffer_head * get_hash_table(int dev, int block)
{
	struct buffer_head * bh;

	for (;;) {
		if (!(bh=find_buffer(dev,block)))
			return NULL;
		// 引用计数+1, Race-conditions 如果有睡眠期间该缓冲块有可能释放/修改/读出错, 缓冲区是否指定设备
		bh->b_count++;
		wait_on_buffer(bh);
		if (bh->b_dev == dev && bh->b_blocknr == block)
			return bh;
		bh->b_count--;
	}
}

/*
 * Ok, this is getblk, and it isn't very clear, again to hinder
 * race-conditions. Most of the code is seldom used, (ie repeating),
 * so it should be much more efficient than it looks.
 *
 * The algoritm is changed: hopefully better, and an elusive bug removed.
 */
// 0bxx  位1 -dirt, 位0 - lock 
#define BADNESS(bh) (((bh)->b_dirt<<1)+(bh)->b_lock)
// 获取指定空闲缓冲块, 处理 Race-conditions
struct buffer_head * getblk(int dev,int block)
{
	struct buffer_head * tmp, * bh;

repeat:
	if ((bh = get_hash_table(dev,block))) // 如果指定块已经在高速缓冲中, 则返回对于块的缓冲头指针
		return bh;
	// 找一个BADNESS最小的缓冲块
	tmp = free_list;
	do {
		if (tmp->b_count)	// 引用计数 > 0, 块正在被使用, 下一个
			continue;
		if (!bh || BADNESS(tmp)<BADNESS(bh)) {
			bh = tmp;
			if (!BADNESS(tmp)) // 0x00 dirt = 0, lock = 0 good good
				break;
		}
/* and repeat until we find something good */
	} while ((tmp = tmp->b_next_free) != free_list);

	if (!bh) {	// 所有的缓冲块都在被使用, 睡了吧, schdule 出去, wake_up醒来再看
		sleep_on(&buffer_wait);
		goto repeat;
	}

	// 执行到这里说明我们已经找到比较适合的空闲块, 如果该缓冲区lock了,睡眠等待schedule出去
	wait_on_buffer(bh);
	if (bh->b_count)
		goto repeat;	// 醒来后看等待睡眠期间该缓冲块又被占用, fuck fuck 重来
	while (bh->b_dirt) {	// 数据被修改了, 将数据写盘
		sync_dev(bh->b_dev);
		wait_on_buffer(bh);
		if (bh->b_count)
			goto repeat;	// 第三次被占用, 我累了, 你不累吗 
	}
/* NOTE!! While we slept waiting for this block, somebody else might */
/* already have added "this" block to the cache. check it */
	if (find_buffer(dev,block)) // 进程为了等待该缓冲块, 其他进程乘我们睡眠之际可能已经将其加入高速缓冲中
		goto repeat;		// 第四次被play了, 我太难了。。
/* OK, FINALLY we know that this buffer is the only one of it's kind, */
/* and that it's unused (b_count=0), unlocked (b_lock=0), and clean */
// 终于可以操你了吧, 占有你, 从LRU = hahs表 + 链表的最为空闲即最近最少使用的缓冲块先拿掉, 再重新插入hash+链表的队列pre
	bh->b_count=1;
	bh->b_dirt=0;
	bh->b_uptodate=0;
	remove_from_queues(bh);
	bh->b_dev=dev;
	bh->b_blocknr=block;
	insert_into_queues(bh);
	return bh;
}

// 释放指定的缓冲块即引用计数--, wake_up唤醒等待空闲缓冲块的进程队列
void brelse(struct buffer_head * buf)
{
	if (!buf)
		return;
	wait_on_buffer(buf);
	if (!(buf->b_count--))
		panic("Trying to double free buffer");
	wake_up(&buffer_wait);
}

/*
 * bread() reads a specified block and returns the buffer that contains
 * it. It returns NULL if the block was unreadable.
 * 
 * 从指定设备中读取数据块
 */
struct buffer_head * bread(int dev,int block)
{
	struct buffer_head * bh;

	if (!(bh=getblk(dev,block)))	// 申请一块空闲缓冲区
		panic("bread: getblk returned NULL\n");
	if (bh->b_uptodate) // 如果缓冲区有效已更新即缓冲块数据和设备上同步的, 直接返回该缓冲区
		return bh;
	ll_rw_block(READ,bh);	//否则 从设备中读取指定数据块到bh
	wait_on_buffer(bh);
	if (bh->b_uptodate)	// 已更新的数据可直接使用, 返回bh
		return bh;
	brelse(bh);		// 读设备操作失败, 释放该缓冲区, 返回NULL
	return NULL;
}

// 从from地址处复制1块即1024字节数据到to地址
#define COPYBLK(from,to) \
__asm__("cld\n\t" \
	"rep\n\t" \
	"movsl\n\t" \
	::"c" (BLOCK_SIZE/4),"S" (from),"D" (to) \
	)

/*
 * bread_page reads four buffers into memory at the desired address. It's
 * a function of its own, as there is some speed to be got by reading them
 * all at the same time, not waiting for one to be read, and then another
 * etc.
 * 
 * 一次读取1页数据即4块数据到内存指定地址
 */
void bread_page(unsigned long address,int dev,int b[4])
{
	struct buffer_head * bh[4];
	int i;
	// 循环4次即随意读取1-4块最多1页, 优先从高速缓冲中获取指定dev和block的已更新可直接使用的缓冲块, 否则就读设备请求块数据
	for (i=0 ; i<4 ; i++)
		if (b[i]) {
			if ((bh[i] = getblk(dev,b[i])))
				if (!bh[i]->b_uptodate)
					ll_rw_block(READ,bh[i]);
		} else
			bh[i] = NULL;
	// 4个缓冲区内容顺序的复制到address起始位置后
	for (i=0 ; i<4 ; i++,address += BLOCK_SIZE)
		if (bh[i]) {
			wait_on_buffer(bh[i]); // 复制前检查unlock, 才可使用缓冲块
			if (bh[i]->b_uptodate) // 再检查之前睡眠期间缓冲块数据是最新的,同步好的
				COPYBLK((unsigned long) bh[i]->b_data,address);
			brelse(bh[i]);	// 复制完后, 释放该缓冲块(引用计数--)
		}
}

/*
 * Ok, breada can be used as bread, but additionally to mark other
 * blocks for reading as well. End the argument list with a negative
 * number.
 * 
 * 从指定设备中读取一些块 Ahead预读
 * fmt 可变参数为一系列块号, 需要一个负数表示参数列表结束
 */
struct buffer_head * breada(int dev,int first, ...)
{
	va_list args;
	struct buffer_head * bh, *tmp;

	va_start(args,first);
	if (!(bh=getblk(dev,first)))
		panic("bread: getblk returned NULL\n");
	if (!bh->b_uptodate)
		ll_rw_block(READ,bh);
	// 预读其他块号, 只需读进高速缓冲区不立即使用, 其他暂时不用理它
	while ((first=va_arg(args,int))>=0) {
		tmp=getblk(dev,first); // b_count++
		if (tmp) {
			if (!tmp->b_uptodate)
				ll_rw_block(READA,tmp);
			tmp->b_count--; // b_count--
		}
	}
	// 可变参数表所有参数处理完毕
	va_end(args);
	wait_on_buffer(bh);
	if (bh->b_uptodate) // 等待第一个缓冲块, 数据依然有效可直接使用
		return bh;
	brelse(bh);			// 否则释放缓冲块, 返回NULL
	return (NULL);
}

// 缓冲区初始化, start_buffer-buffer_end = [end - 4M]
void buffer_init(long buffer_end)
{
	struct buffer_head * h = start_buffer; // start_buffer = end 处
	void * b;
	int i;
	// 确定实际的缓冲区尾部 b, [640K-1M) 为显存和ROM BIOS使用, 则buffer = [end内核末端 ~ 640KB]
	if (buffer_end == 1<<20) // 1<<20 = 1M
		b = (void *) (640*1024); // 0xA0000
	else
		b = (void *) buffer_end;
	// 建立空闲缓冲块双向链表buffer_head, 计算缓冲块数量
	// 从缓冲区b即高端地址开始划分为1个1个1KB大小的块, 同时在缓冲区低端地址建立buffer_end解释缓冲块
	while ( (b -= BLOCK_SIZE) >= ((void *) (h+1)) /*b 指向内存块地址 >= h缓冲头末端即 h+1, 确保足够长度保存一个bh结构*/) {
		h->b_dev = 0;
		h->b_blocknr = 0; // default
		h->b_dirt = 0;
		h->b_count = 0;
		h->b_lock = 0;
		h->b_uptodate = 0;
		h->b_wait = NULL;
		h->b_next = NULL;
		h->b_prev = NULL;
		h->b_data = (char *) b;
		h->b_prev_free = h-1;
		h->b_next_free = h+1;
		h++;	// h 指向下一个新缓冲头位置
		NR_BUFFERS++; // 缓冲块计数
		if (b == (void *) 0x100000) // 如果b 递减到1MB位置, skip 384KB, b = 640KB处
			b = (void *) 0xA0000;
	}
	h--; // h 指向最后一个有效的缓冲头
	free_list = start_buffer; // 让空闲链表头指针指向第一个缓冲块
	free_list->b_prev_free = h; // prev_free指向最后一个缓冲块
	h->b_next_free = free_list; // h即最后一个bh指向头指针, 形成了一个ring chain
	// 初始化hash表
	for (i=0;i<NR_HASH;i++)
		hash_table[i]=NULL;
}	
