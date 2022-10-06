/*
 *  linux/kernel/blk_dev/ll_rw.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * This handles all read/write requests to block devices
 */
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include "blk.h"

/*
 * The request-struct contains all necessary data
 * to load a nr of sectors into memory
 */
struct request request[NR_REQUEST]; // 请求项数组队列

/*
 * used to wait on when there are no free requests
 */
struct task_struct * wait_for_request = NULL; // 在请求项数组满了等待进程链表

/* blk_dev_struct is:
 *	do_request-address
 *	next-request
 * 如blk[3].do_request-address = do_hd_request
 */
struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
	{ NULL, NULL },		/* no_dev */
	{ NULL, NULL },		/* dev mem ramdisk*/
	{ NULL, NULL },		/* dev fd */
	{ NULL, NULL },		/* dev hd 硬盘设备*/
	{ NULL, NULL },		/* dev ttyx 虚拟/串口终端*/
	{ NULL, NULL },		/* dev tty tty控制台*/
	{ NULL, NULL }		/* dev lp 打印机*/
};

// 加锁指定的缓冲块
static inline void lock_buffer(struct buffer_head * bh)
{
	cli();	// 关中断
	while (bh->b_lock)
		sleep_on(&bh->b_wait); // 如果缓冲区已被其他进程上锁, 则使自己睡眠(不可中断等待状态), 直到缓冲区解锁明确唤醒
	bh->b_lock=1; // 加锁
	sti(); // 开中断
}

// 解锁指定的缓冲区, 与blk.h同名函数完全一样
static inline void unlock_buffer(struct buffer_head * bh)
{
	if (!bh->b_lock)
		printk("ll_rw_block.c: buffer not locked\n\r");
	bh->b_lock = 0;
	wake_up(&bh->b_wait); // 唤醒等待该缓冲区上的进程
}

/*
 * add-request adds a request to the linked list.
 * It disables interrupts so that it can muck with the
 * request-lists in peace.
 * 
 * 向链表中添加请求项
 */
static void add_request(struct blk_dev_struct * dev, struct request * req)
{
	struct request * tmp;
	// 请求项next置为NULL
	req->next = NULL;
	// 关中断
	cli();
	// 清除dirt 标记
	if (req->bh)
		req->bh->b_dirt = 0;
	// 查看指定设备上是否有当前请求项即查看设备是否在忙
	if (!(tmp = dev->current_request)) {
		// 目前该设备上没有请求项, 本次是第一个请求也是唯一一个, 将块设备当前请求指针指向自己
		dev->current_request = req;
		// 开中断
		sti();
		// 立即指向如do_hd_request
		(dev->request_fn)();
		return;
	}
	// 当前有请求项, 利用电梯调度算法插入适合位置
	for ( ; tmp->next ; tmp=tmp->next)
		if ((IN_ORDER(tmp,req) || 
		    !IN_ORDER(tmp,tmp->next)) &&
		    IN_ORDER(req,tmp->next))
			break;
	// 将当前请求插入队列正确位置处
	req->next=tmp->next;
	tmp->next=req;
	// 开中断
	sti();
}

static void make_request(int major,int rw, struct buffer_head * bh)
{
	struct request * req;
	int rw_ahead;

/* WRITEA/READA is special case - it is not really needed, so if the */
/* buffer is locked, we just forget about it, else it's a normal read */
	// Ahead 表示提前预读/写数据块, 当指定的缓冲区上锁了,就放弃预读/写, 转为普通读/写操作
	if ((rw_ahead = (rw == READA || rw == WRITEA))) {
		if (bh->b_lock)
			return;
		if (rw == READA)
			rw = READ;
		else
			rw = WRITE;
	}
	if (rw!=READ && rw!=WRITE)
		panic("Bad block dev command, must be R/W/RA/WA");
	// 写操作请求,但缓冲区中的数据读入后没有修改b_dirt则没有必要添加请求项
	// 读操作请求,但缓冲区的数据已经uptodate的即与块设备上完全一样则页没必要添加请求项
	// 加锁缓冲区进行检查, 如果缓冲区已锁则当前进程睡眠直到被唤醒
	lock_buffer(bh);
	if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate)) {
		// 高速缓冲区可用, 数据可靠无需指向读写硬盘, 直接使用内存数据, 直接解锁缓冲区并返回
		unlock_buffer(bh);
		return;
	}
repeat:
/* we don't allow the write-requests to fill up the queue completely:
 * we want some room for reads: they take precedence. The last third
 * of the requests are only for reads.
 */
	// 从后往前添加读/写请求项 slots, 后1/3用于读, 队列2/3处到队列头用于写
	if (rw == READ)
		req = request+NR_REQUEST;	// 读请求指向队列尾部
	else
		req = request+((NR_REQUEST*2)/3); // 写请求指向队列2/3处
/* find an empty request 搜索一个空请求项*/
	while (--req >= request)
		if (req->dev<0)
			break;
/* if none found, sleep on new requests: check for rw_ahead */
	if (req < request) { // 已经搜索到队列头了
		if (rw_ahead) { // 提前预读/写, 直接解锁缓冲区并返回
			unlock_buffer(bh);
			return;
		}
		// 进入睡眠不可中断等待状态, 阻塞在这, 这个睡得真有技巧
		sleep_on(&wait_for_request);
		goto repeat;
	}
/* fill up the request-info, and add it to the queue */
// 向空闲请求项填充请求信息, 并加入到队列中
	req->dev = bh->b_dev;			// 设备号
	req->cmd = rw;					// 命令READ/WRITE
	req->errors=0;		 			// 操作时重试错误数
	req->sector = bh->b_blocknr<<1; // 请求项读写操作的起始扇区号 = 块号转换扇区号 / 2
	req->nr_sectors = 2;			// 请求项需要读写的扇区数
	req->buffer = bh->b_data;		// 请求项存放数据buffer
	req->waiting = NULL;			// 等待I/O操作完成唤醒的进程
	req->bh = bh;					// 缓冲块头指针
	req->next = NULL;				// 指向下一个请求项
	add_request(major+blk_dev,req);	// 添加到队列
}

// 块设备驱动程序与系统其他部分的接口层函数, 读写块设备中数据, 通常在fs/buffer.c中调用
// 创建块设备请求项并插入到指定块设备请求队列中, 实际读写则是设备的request_fn 完成, 如硬盘 do_hd_request
// param1:rw - READ/READA/WRITE/WRITEA
// param2: 读/写块设备信息放在缓冲块头结构bh, 有设备号, 块号
void ll_rw_block(int rw, struct buffer_head * bh)
{
	// 主设备号, 如硬盘 3
	unsigned int major;

	if ((major=MAJOR(bh->b_dev)) >= NR_BLK_DEV ||
	!(blk_dev[major].request_fn)) {
		printk("Trying to read nonexistent block-device\n\r");
		return;
	}
	make_request(major,rw,bh);
}

// 块设备初始化 init/main.c调用
void blk_dev_init(void)
{
	int i;

	for (i=0 ; i<NR_REQUEST ; i++) {
		request[i].dev = -1;
		request[i].next = NULL;
	}
}
