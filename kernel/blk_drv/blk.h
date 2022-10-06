#ifndef _BLK_H
#define _BLK_H

// 7个块设备
#define NR_BLK_DEV	7
/*
 * NR_REQUEST is the number of entries in the request-queue.
 * NOTE that writes may use only the low 2/3 of these: reads
 * take precedence.
 *
 * 32 seems to be a reasonable number: enough to get some benefit
 * from the elevator-mechanism, but not so much as to lock a lot of
 * buffers when they are in the queue. 64 seems to be too many (easily
 * long pauses in reading when heavy writing/syncing is going on)
 */
#define NR_REQUEST	32	// 请求队列中包含的项数 中低2/3用于写操作, 读操作优先

/*
 * Ok, this is an expanded form so that we can use the same
 * request for paging requests when that is implemented. In
 * paging, 'bh' is NULL, and 'waiting' is used to wait for
 * read/write completion.
 */
// 块设备请求项队列, Elevator Algorithm 电梯调度算法 磁头移动最小距离原则, 新建请求项插入链表合适位置
// I/O 调度程序, 当前版本仅对请求项进行排序, 2.6.x 后还包含访问相邻磁盘扇区的多个请求项merge
struct request {
	int dev;		/* 使用的设备号 -1 空闲if no request */
	int cmd;		/* 命令 READ or WRITE */
	int errors;		// 读写产生的错误次数
	unsigned long sector;	// 起始扇区
	unsigned long nr_sectors;	// 读写扇区数
	char * buffer;	// 数据缓冲区
	struct task_struct * waiting;	// 等待I/O操作完成的进程
	struct buffer_head * bh;	// 缓冲区头指针
	struct request * next;		// 指向下一项请求
};

/*
 * This is used in the elevator algorithm: Note that
 * reads always go before writes. This is natural: reads
 * are much more time-critical than writes.
 * 这个宏实现电梯调度算法, 读操作总是在写操作之前进行, 因为读操作对时间的要求更严格
 * 这个宏在blk_drv/ll_rw_block add_request 调用, 部分实现了I/O调度功能即实现了对请求项排序(另一个是请求项合并功能)
 */
#define IN_ORDER(s1,s2) \
((s1)->cmd<(s2)->cmd || ((s1)->cmd==(s2)->cmd && \
((s1)->dev < (s2)->dev || ((s1)->dev == (s2)->dev && \
(s1)->sector < (s2)->sector))))

// 请求项
struct blk_dev_struct {
	void (*request_fn)(void);	// 请求项操作的函数指针如do_hd_request
	struct request * current_request;	// 当前请求项指针
};
// 块设备表, 一种设备占一项
extern struct blk_dev_struct blk_dev[NR_BLK_DEV];
// 请求项队列数组
extern struct request request[NR_REQUEST];
// 等待空闲请求项进程队列头指针
extern struct task_struct * wait_for_request;

#ifdef MAJOR_NR

/*
 * Add entries as needed. Currently the only block devices
 * supported are hard-disks and floppies.
 */

#if (MAJOR_NR == 1)	// ramdisk主设备号
/* ram disk */
#define DEVICE_NAME "ramdisk"
#define DEVICE_REQUEST do_rd_request
#define DEVICE_NR(device) ((device) & 7)
#define DEVICE_ON(device) 
#define DEVICE_OFF(device)

#elif (MAJOR_NR == 2)
/* floppy */
#define DEVICE_NAME "floppy"
#define DEVICE_INTR do_floppy
#define DEVICE_REQUEST do_fd_request
#define DEVICE_NR(device) ((device) & 3)
#define DEVICE_ON(device) floppy_on(DEVICE_NR(device))
#define DEVICE_OFF(device) floppy_off(DEVICE_NR(device))

#elif (MAJOR_NR == 3)	// 硬盘主设备号
/* harddisk */
#define DEVICE_NAME "harddisk"
#define DEVICE_INTR do_hd
#define DEVICE_REQUEST do_hd_request
#define DEVICE_NR(device) (MINOR(device)/5)
#define DEVICE_ON(device)	// 硬盘一直在工作无需开/关
#define DEVICE_OFF(device)

#else
/* unknown blk device */
#error "unknown blk device"

#endif

#define CURRENT (blk_dev[MAJOR_NR].current_request)
#define CURRENT_DEV DEVICE_NR(CURRENT->dev)

#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif
static void (DEVICE_REQUEST)(void);

// 解锁指定的缓冲区块并唤醒等待该缓冲区的进程
static inline void unlock_buffer(struct buffer_head * bh)
{
	if (!bh->b_lock)
		printk(DEVICE_NAME ": free buffer being unlocked\n");
	bh->b_lock=0;
	wake_up(&bh->b_wait);
}

static inline void end_request(int uptodate)
{
	// 关闭指定块设备
	DEVICE_OFF(CURRENT->dev);
	// 如果此次读写缓冲区有效则根据参数设置缓冲区数据更新标记并解锁该缓冲区
	if (CURRENT->bh) {
		CURRENT->bh->b_uptodate = uptodate;
		unlock_buffer(CURRENT->bh);
	}
	// 此次请求项操作失败, 显示相关块设备错误信息
	if (!uptodate) {
		printk(DEVICE_NAME " I/O error\n\r");
		printk("dev %04x, block %d\n\r",CURRENT->dev,
			CURRENT->bh->b_blocknr);
	}
	// 唤醒等待该请求项的进程
	wake_up(&CURRENT->waiting);
	// 唤醒等待空闲请求项出现的进程
	wake_up(&wait_for_request);
	// 当前操作设备置为无效, 从请求项链表拿掉本请求项
	CURRENT->dev = -1;
	// 当前请求指针指向下一请求项
	CURRENT = CURRENT->next;
}

// 块设备统一初始化宏
#define INIT_REQUEST \
repeat: \
	if (!CURRENT) \
		return; \
	if (MAJOR(CURRENT->dev) != MAJOR_NR) \
		panic(DEVICE_NAME ": request list destroyed"); \
	if (CURRENT->bh) { \
		if (!CURRENT->bh->b_lock) \
			panic(DEVICE_NAME ": block not locked"); \
	}

#endif

#endif
