/*
 *  linux/kernel/hd.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This is the low-level hd interrupt support. It traverses the
 * request-list, using interrupts to jump between functions. As
 * all the functions are called within interrupts, we may not
 * sleep. Special care is recommended.
 * 
 *  modified by Drew Eckhardt to check nr of hd's from the CMOS.
 * 
 * 硬盘控制器驱动程序:
 * 1. 初始化硬盘和设置硬盘用的数据结构函数 如 sys_setup和hd_init
 * 2. 向硬盘控制器发送命令 hd_out
 * 3. 处理硬盘当前请求项 do_hd_request
 * 4. 硬盘中断处理过程C参数 read_intr/write_intr/bad_rw_intr/recal_intr
 * 5. 辅助函数 controller_ready/drive_busy/win_result/hd_out/reset_controller
 * 
 * DMA Direct Memory Access 外设直接与内存传输数据, 映射内存, 绕过CPU
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/hdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

// 主设备号必须在blk.h 前定义, blk.h 用这个宏确定关联的符号常量和宏
#define MAJOR_NR 3
#include "blk.h"

// 读取CMOS中硬盘信息, 与init/main.c 读取CMOS时钟信息完全一样
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

/* Max read/write errors/sector */
#define MAX_ERRORS	7		// 读写一个扇区允许的最大出错重试数
#define MAX_HD		2		// 系统最多支持2块硬盘

// reset复位操作时条用重新校正函数
static void recal_intr(void);
// 重新校正标记, 置1时调用recal_intr将磁头移动到0柱面/磁道
static int recalibrate = 0;
// 复位标记, 读写错误发生时通过置1复位硬盘控制器
static int reset = 0;

/*
 *  This struct defines the HD's and their types.
 */
struct hd_i_struct {
	int head,sect,cyl,wpcom,lzone,ctl;
	};
#ifdef HD_TYPE
struct hd_i_struct hd_info[] = { HD_TYPE };
#define NR_HD ((sizeof (hd_info))/(sizeof (struct hd_i_struct)))
#else
struct hd_i_struct hd_info[] = { {0,0,0,0,0,0},{0,0,0,0,0,0} };
static int NR_HD = 0;
#endif

// 硬盘分区结构, 每个分区从硬盘0磁道算起起始物理扇区号和分区里的扇区数
// 5的倍数 如hd[0]/hd[5]表示整个硬盘的参数
static struct hd_struct {
	long start_sect;		// 分区在硬盘中起始绝对物理扇区
	long nr_sects;			// 分区中扇区数
} hd[5*MAX_HD]={{0,0},};

// %0 - 读端口 port, %1 - 保存在buf, %2 - 读 nr 字
#define port_read(port,buf,nr) \
__asm__("cld;rep;insw"::"d" (port),"D" (buf),"c" (nr))
// %0 - 写端口 port, %1 - 从buf取数据, %2 - 写 nr 字
#define port_write(port,buf,nr) \
__asm__("cld;rep;outsw"::"d" (port),"S" (buf),"c" (nr))

extern void hd_interrupt(void);		// 硬盘中断处理
extern void rd_load(void);			// ramdisk 创建加载

/* This may be used only once, enforced by 'static int callable' */
// 利用boot/setup.s提供的信息(利用ROM BIOS获取)对系统中硬盘驱动器参数设置, 读取硬盘分区表
// 读取CMOS, 设置硬盘分区结构hd
// 尝试把启动引导盘上initrd复制到ramdisk内存中, 加载ramdisk的根文件系统, 否则继续执行普通根文件系统的加载工作
// 参数 BIOS 由init/main.c init设置为指向硬盘参数表结构, 该硬盘参数表包括2个硬盘参数表内容 = 32字节, 0x90080处
int sys_setup(void * BIOS)
{
	static int callable = 1; // 用静态变量限制只在初始化时被调用一次
	int i,drive;
	unsigned char cmos_disks;
	struct partition *p;
	struct buffer_head * bh;

	if (!callable)
		return -1;
	callable = 0;
	// 如果linux/config.h没有设置HD_TYPE, 读取0x90080开始的硬盘参数表, 设置硬盘参数表hd_info[]
#ifndef HD_TYPE
	for (drive=0 ; drive<2 ; drive++) {
		hd_info[drive].cyl = *(unsigned short *) BIOS;			// 柱面数
		hd_info[drive].head = *(unsigned char *) (2+BIOS);		// 磁头数
		hd_info[drive].wpcom = *(unsigned short *) (5+BIOS);	// 写前预补偿柱面号
		hd_info[drive].ctl = *(unsigned char *) (8+BIOS);		// 控制字节
		hd_info[drive].lzone = *(unsigned short *) (12+BIOS);	// 磁头着陆区柱面号
		hd_info[drive].sect = *(unsigned char *) (14+BIOS);		// 每磁道扇区数
		BIOS += 16;
	}
	if (hd_info[1].cyl)
		NR_HD=2;		// 硬盘数置2
	else
		NR_HD=1;
#endif
	// 设置硬盘分区结构数组hd[], hd[0]/hd[5]分别表示2个硬盘的整体参数, 1 - 4, 6-9两个硬盘的4个分区参数
	for (i=0 ; i<NR_HD ; i++) {
		hd[i*5].start_sect = 0;		// 硬盘起始扇区号
		hd[i*5].nr_sects = hd_info[i].head*
				hd_info[i].sect*hd_info[i].cyl; // 硬盘扇区总数
	}

	/*
		We querry CMOS about hard disks : it could be that 
		we have a SCSI/ESDI/etc controller that is BIOS
		compatable with ST-506, and thus showing up in our
		BIOS table, but not register compatable, and therefore
		not present in CMOS.

		Furthurmore, we will assume that our ST-506 drives
		<if any> are the primary drives in the system, and 
		the ones reflected as drive 1 or 2.

		The first drive is stored in the high nibble of CMOS
		byte 0x12, the second in the low nibble.  This will be
		either a 4 bit drive type or 0xf indicating use byte 0x19 
		for an 8 bit type, drive 1, 0x1a for drive 2 in CMOS.

		Needless to say, a non-zero value means we have 
		an AT controller hard disk for that drive.

	*/
	// CMOS 偏移地址0x12读出硬盘类型字节， 低4位存放第2个硬盘类型值, 到底吃了几碗粉儿
	if ((cmos_disks = CMOS_READ(0x12)) & 0xf0)
		if (cmos_disks & 0x0f)
			NR_HD = 2;
		else
			NR_HD = 1;
	else
		NR_HD = 0;
	for (i = NR_HD ; i < 2 ; i++) {
		hd[i*5].start_sect = 0;
		hd[i*5].nr_sects = 0;
	}
	// 读取每个硬盘上第1个扇区中的分区表信息
	for (drive=0 ; drive<NR_HD ; drive++) {
		// bread 读硬盘第一个数据块, 参数1 0x300/0x305分别表示两个硬盘设备号, 参数2 0表示读硬盘第1个块
		if (!(bh = bread(0x300 + drive*5,0))) {
			printk("Unable to read partition table of drive %d\n\r",
				drive);
			panic("");
		}
		// 读取数据在bh双向链表指向的缓冲区, 根据硬盘第一个扇区最后两个字节0xAA55
		if (bh->b_data[510] != 0x55 || (unsigned char)
		    bh->b_data[511] != 0xAA) {
			printk("Bad partition table on drive %d\n\r",drive);
			panic("");
		}
		// 将0x1BE-0x1FD 64个字节的硬盘分区表信息放入hd[]硬盘分区结构中
		p = 0x1BE + (void *)bh->b_data;
		for (i=1;i<5;i++,p++) {
			hd[i+5*drive].start_sect = p->start_sect;
			hd[i+5*drive].nr_sects = p->nr_sects;
		}
		// 释放缓冲区
		brelse(bh);
	}
	if (NR_HD)
		printk("Partition table%s ok.\n\r",(NR_HD>1)?"s":"");
	// 尝试加载引导启动盘initrd 根文件系统到ramdisk内存文件系统, 把此时的ROOT_DEV修改为ramdisk的设备号
	rd_load();
	// 加载普通根文件系统
	mount_root();
	return (0);
}

// 判断并循环等待硬盘控制器就绪, HD_STATUS(0x1f7) 忙位 7 = 0b1000_0000 是否为1, 返回值 > 0 空闲就绪状态 
static int controller_ready(void)
{
	int retries=100000;

	while (--retries && (inb_p(HD_STATUS)&0x80));
	return (retries);
}

//winchester硬盘, 读取状态寄存器中命令执行结果状态 0 - 正常, 1 - 有错则再读HD_ERROR(1f1)寄存器
static int win_result(void)
{
	int i=inb_p(HD_STATUS);

	if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT))
		== (READY_STAT | SEEK_STAT))
		return(0); /* ok */
	if (i&1) i=inb(HD_ERROR);
	return (1);
}

/// @brief 硬盘控制器操作命令发送
/// @param drive 硬盘号 0-1
/// @param nsect 读写扇区数
/// @param sect  起始扇区
/// @param head 磁头号
/// @param cyl  柱面号
/// @param cmd  命令码
/// @param intr_addr 硬盘中断处理中 do_hd callback 
static void hd_out(unsigned int drive,unsigned int nsect,unsigned int sect,
		unsigned int head,unsigned int cyl,unsigned int cmd,
		void (*intr_addr)(void))
{
	register int port asm("dx"); //定义局部变量放在指定的寄存器dx中

	if (drive>1 || head>15)
		panic("Trying to write bad sector");
	if (!controller_ready())
		panic("HD controller not ready");
	do_hd = intr_addr; // 注册 callback
	// 向0x1f0 - 0x1f7 端口发送7字节命令
	outb_p(hd_info[drive].ctl,HD_CMD); // 先向控制器命令端口0x3f6发送控制字节,建立硬盘控制方式
	port=HD_DATA; // dx = 数据寄存器端口0x1f0
	outb_p(hd_info[drive].wpcom>>2,++port); // 0x1f1 = 写前预补偿柱面号
	outb_p(nsect,++port);					// 0x1f2 = 读写扇区数
	outb_p(sect,++port);					// 0x1f3 = 起始扇区
	outb_p(cyl,++port);						// 0x1f4 = 柱面号低8位
	outb_p(cyl>>8,++port);					// 0x1f5 = 柱面号高8位
	outb_p(0xA0|(drive<<4)|head,++port);	// 0x1f6 = 驱动器号 + 磁头号
	outb(cmd,++port);						// 0x1f7 = 硬盘控制命令
}

// 循环读取HD_STATUS等待硬盘就绪, READY_STAT | SEEK_STAT 则表示就绪, 成功返回0,否则返回1
static int drive_busy(void)
{
	unsigned int i;

	for (i = 0; i < 100000; i++)
		if (READY_STAT == (inb_p(HD_STATUS) & (BUSY_STAT|READY_STAT))) // 等待READY_STAT置1和BUSY_STAT置0
			break;
	// 再次读取状态寄存器, READY_STAT | SEEK_STAT 置1, 表示硬盘就绪
	i = inb(HD_STATUS);
	i &= BUSY_STAT | READY_STAT | SEEK_STAT;
	if (i == (READY_STAT | SEEK_STAT))
		return(0);
	printk("HD controller times out\n\r");
	return(1);
}

// 复位硬盘控制器
static void reset_controller(void)
{
	int	i;

	// 向控制寄存器端口0x3f6发送 4=0b_0000_0100复位控制字节
	outb(4,HD_CMD);
	// 等硬盘控制器执行下复位操作咯
	for(i = 0; i < 100; i++) nop();
	// 再发送正常控制字节 &0x0f = 不禁止重试重读
	outb(hd_info[0].ctl & 0x0f ,HD_CMD);
	if (drive_busy())
		printk("HD-controller still busy\n\r");
	if ((i = inb(HD_ERROR)) != 1)
		printk("HD-controller reset failed: %02x\n\r",i);
}

// 复位第nr块硬盘, WIN_SPECIFY = 建立驱动器参数, recal_intr 中断回调的重新校正
static void reset_hd(int nr)
{
	reset_controller();
	hd_out(nr,hd_info[nr].sect,hd_info[nr].sect,hd_info[nr].head-1,
		hd_info[nr].cyl,WIN_SPECIFY,&recal_intr);
}

void unexpected_hd_interrupt(void)
{
	printk("Unexpected HD interrupt\n\r");
}

// 读写硬盘失败处理
static void bad_rw_intr(void)
{
	// 读写硬盘重试错误数 >=7, 结束请求项, 唤醒等待该请求的进程, 0 没有数据更新
	if (++CURRENT->errors >= MAX_ERRORS)
		end_request(0);
	// 读写扇区重试错误数 > 3, 复位硬盘控制器
	if (CURRENT->errors > MAX_ERRORS/2)
		reset = 1;
}

// 读硬盘命令结束触发中断IRQ -> 执行硬盘中断处理handler -> do_hd = read_intr callback
// 一次读扇区操作完成/出错后调用
static void read_intr(void)
{
	// 读命令执行失败
	if (win_result()) {
		// 对当前请求项重试错误数+1, <=3重新硬盘复位并重试, >=7 结束本请求项, 下一个
		bad_rw_intr();
		// 再执行本次请求项处理
		do_hd_request();
		return;
	}
	// 读命令执行成功, 从数据寄存器把256个字 = 1个扇区的数据读到请求项缓冲区buffer, 记录读取的状态数据
	port_read(HD_DATA,CURRENT->buffer,256);
	CURRENT->errors = 0;	// 重置错误数
	CURRENT->buffer += 512;	// 调整缓冲区指针
	CURRENT->sector++;		// 起始扇区号+1
	// 如果后面还有待读取的扇区, 设置do_hd = read_intr 直接返回, 等待下一次中断的到来
	if (--CURRENT->nr_sectors) {
		do_hd = &read_intr;
		return;
	}
	// 本次请求项的全部扇区数据已经读完, 处理请求项结束收尾工作  1 有数据更新
	end_request(1);
	// 处理其他硬盘请求项, 生产队的驴
	do_hd_request();
}

// 写硬盘命令结束触发中断IRQ -> 执行硬盘中断处理handler -> do_hd = write_intr callback
// 一次写扇区操作完成/出错后调用
static void write_intr(void)
{
	// 写命令执行失败
	if (win_result()) {
		// 重试策略
		bad_rw_intr();
		do_hd_request();
		return;
	}
	// 写命令执行成功
	if (--CURRENT->nr_sectors) {
		CURRENT->sector++;
		CURRENT->buffer += 512;
		do_hd = &write_intr;
		port_write(HD_DATA,CURRENT->buffer,256); // 向数据寄存器端口写 256个字 = 512 = 1扇区
		return;
	}
	end_request(1);
	do_hd_request();
}

// 硬盘calibrate 重新校正
static void recal_intr(void)
{
	if (win_result())
		bad_rw_intr();
	do_hd_request();
}

// 执行硬盘读写请求
void do_hd_request(void)
{
	int i,r = 0;
	unsigned int block,dev;
	unsigned int sec,head,cyl;
	unsigned int nsect;
	// 初始化检查
	INIT_REQUEST;
	// 获取设备号的子设备号 = 对应硬盘上的分区
	dev = MINOR(CURRENT->dev);
	//请求的起始扇区
	block = CURRENT->sector;
	// 检查参数, dev = 0~9, block > 该分区扇区数 - 2 , INIT_REQUEST 的repeat
	if (dev >= 5*NR_HD || block+2 > hd[dev].nr_sects) {
		end_request(0);
		goto repeat;
	}
	// 整个硬盘的LBA扇区号 = 读写的数据块号 + 子设备号对应的分区的起始扇区
	block += hd[dev].start_sect;
	// 硬盘编号 0 / 1
	dev /= 5;

	// 内联汇编将LBA表示法block 转换为CHS 表示法 cyl sec head nsect
	// sector / track_secs = 整数tracks 余数sec
	// tracks / dev_heads = 整数cyl 余数 head˗
	// 在当前磁道上扇区号从1开始, 于是需要把sec++

	// sector = (cyl * dev_heads + head) * track_secs + sec -1
	/*
	* eax = block 扇区号, edx = 0, divl = edx:eax 扇区号 /  hd_info[dev].sect 每磁道扇区数
	* eax = 商 到指定位置的对应总磁道数(所有磁头面), edx = 余数 当前磁道上的扇区号
	*/
	__asm__("divl %4":"=a" (block),"=d" (sec):"0" (block),"1" (0),
		"r" (hd_info[dev].sect));
	/*
	* eax = 对应总磁道数, edx = 0, divl = edx:eax 扇区号 /  hd_info[dev].head 硬盘总磁头数
	* eax = 商 柱面号 cyl, edx = 余数 当前磁头号
	*/
	__asm__("divl %4":"=a" (cyl),"=d" (head):"0" (block),"1" (0),
		"r" (hd_info[dev].head));
	sec++;
	nsect = CURRENT->nr_sectors;

	// 重置硬盘驱动器
	if (reset) {
		reset = 0;
		recalibrate = 1;
		reset_hd(CURRENT_DEV);
		return;
	}
	// 重新校准
	if (recalibrate) {
		recalibrate = 0;
		// 执行寻道操作, 让处于任何地方的磁头移动到0柱面
		hd_out(dev,hd_info[CURRENT_DEV].sect,0,0,0,
			WIN_RESTORE,&recal_intr);
		return;
	}	
	if (CURRENT->cmd == WRITE) {
		// 发送写命令
		hd_out(dev,nsect,sec,head,cyl,WIN_WRITE,&write_intr);
		// DRQ_STAT 请求服务位, 表示ready 在主机和数据端口传输一个字/字节数据
		for(i=0 ; i<3000 && !(r=inb_p(HD_STATUS)&DRQ_STAT) ; i++)
			/* nothing */ ;
		if (!r) { // DRQ_STAT 不为 1, 表示发送的要求写盘命令失败
			bad_rw_intr();
			goto repeat;
		}
		// ready了, 向硬盘控制器数据端口寄存器HD_DATA写入1个扇区数据
		port_write(HD_DATA,CURRENT->buffer,256);
	} else if (CURRENT->cmd == READ) {
		hd_out(dev,nsect,sec,head,cyl,WIN_READ,&read_intr);
	} else
		panic("unknown hd-command");
}

// 设置硬盘控制器中断描述符, 打开端口以允许硬盘控制器发送IRQ
void hd_init(void)
{
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST; // do_hd_request
	set_intr_gate(0x2E,&hd_interrupt);	// 设置硬盘中断描述符 include/asm/system.h, 0x2E(46)对应8259A IRQ13
	outb_p(inb_p(0x21)&0xfb,0x21);	// 重置主8250A int 2
	outb(inb_p(0xA1)&0xbf,0xA1);	// 重置从中断请求mask位
}
