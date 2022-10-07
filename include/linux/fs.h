/*
 * This file has definitions for some important file table
 * structures etc.
 */

#ifndef _FS_H
#define _FS_H

#include <sys/types.h>

/* devices are as follows: (same as minix, so we can use the minix
 * file system. These are major numbers.)
 *
 * 0 - unused (nodev)
 * 1 - /dev/mem
 * 2 - /dev/fd
 * 3 - /dev/hd		硬盘设备
 * 4 - /dev/ttyx
 * 5 - /dev/tty		tty 终端设备
 * 6 - /dev/lp
 * 7 - unnamed pipes
 */

#define IS_SEEKABLE(x) ((x)>=1 && (x)<=3)

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a))>>8)	// 取高字节为主设备号
#define MINOR(a) ((a)&0xff)				// 取低字节次设备号

#define NAME_LEN 14		// minix 名字长度
#define ROOT_INO 1		// 根 inode

#define I_MAP_SLOTS 8			// inode slots数
#define Z_MAP_SLOTS 8			// 逻辑块 slots数
#define SUPER_MAGIC 0x137F		// 文件系统魔数

#define NR_OPEN 20
#define NR_INODE 32
#define NR_FILE 64
#define NR_SUPER 8
#define NR_HASH 307				// 缓冲区hash表数组项
#define NR_BUFFERS nr_buffers	// 缓冲区块个数
#define BLOCK_SIZE 1024			// 1K
#define BLOCK_SIZE_BITS 10
#ifndef NULL
#define NULL ((void *) 0)
#endif

// 每个逻辑块可存放的inode数
#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct d_inode)))
// 每个逻辑块可存放的目录项数
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct dir_entry)))

#define PIPE_HEAD(inode) ((inode).i_zone[0])
#define PIPE_TAIL(inode) ((inode).i_zone[1])
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode))&(PAGE_SIZE-1))
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode)==PIPE_TAIL(inode))
#define PIPE_FULL(inode) (PIPE_SIZE(inode)==(PAGE_SIZE-1))
#define INC_PIPE(head) \
__asm__("incl %0\n\tandl $4095,%0"::"m" (head))

// 块缓冲区
typedef char buffer_block[BLOCK_SIZE];

// 双向链表缓冲区头bh
struct buffer_head {
	char * b_data;			/* 指向对应缓冲块数据 pointer to data block (1024 bytes) */
	unsigned long b_blocknr;	/* 块号 block number */
	unsigned short b_dev;		/* 使用该缓冲块的设备号, device (0 = free) */
	unsigned char b_uptodate;	// 数据是否最新或直接可使用
	unsigned char b_dirt;		/* 是否修改过内容 0-clean,1-dirty */
	unsigned char b_count;		/* 引用计数 users using this block */
	unsigned char b_lock;		/* 是否锁定 0 - ok, 1 -locked */
	struct task_struct * b_wait;	// 等待该缓冲区上的进程队列
	struct buffer_head * b_prev;	// hash_table 中同一个hash值的拉链法前一个缓冲头
	struct buffer_head * b_next;	// hash_table 中同一个hash值的拉链法后一个缓冲头
	struct buffer_head * b_prev_free; // 前一个缓冲头
	struct buffer_head * b_next_free;// 后一个缓冲头
};

// 磁盘inode索引, d_inode 7项
struct d_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_time;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];	// 0-6直接索引, 7间接 8 二级逻辑块
};

struct m_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_mtime;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
/* these are in memory also 以下字段在内存中*/
	struct task_struct * i_wait;
	unsigned long i_atime;
	unsigned long i_ctime;
	unsigned short i_dev;
	unsigned short i_num;
	unsigned short i_count;
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe;
	unsigned char i_mount;
	unsigned char i_seek;
	unsigned char i_update;
};

// 文件结构, fd和inode关联
struct file {
	unsigned short f_mode;		// 文件操作模式 RW 位
	unsigned short f_flags;		// 文件打开/控制标记
	unsigned short f_count;		// 对应文件引用数
	struct m_inode * f_inode;	// 指向的inode
	off_t f_pos;				// 文件读写偏移量
};

// 超级块, d_super_block 磁盘8项
struct super_block {
	unsigned short s_ninodes;		// 节点数
	unsigned short s_nzones;		// 逻辑块数
	unsigned short s_imap_blocks;	// inode位图占用的数据块数
	unsigned short s_zmap_blocks;	// 逻辑块位图占用的数据块数
	unsigned short s_firstdatazone;	// 第一个数据逻辑块号
	unsigned short s_log_zone_size;	// log2为底对数的数据块数/逻辑块
	unsigned long s_max_size;		// 文件最大长度
	unsigned short s_magic;			// 文件系统魔数
/* These are only in memory */
	struct buffer_head * s_imap[8];	// 占用8块， 表示64M
	struct buffer_head * s_zmap[8];
	unsigned short s_dev;
	struct m_inode * s_isup;		// 文件系统根目录inode
	struct m_inode * s_imount;
	unsigned long s_time;
	struct task_struct * s_wait;
	unsigned char s_lock;
	unsigned char s_rd_only;
	unsigned char s_dirt;
};

struct d_super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
};

// 目录项结构
struct dir_entry {
	unsigned short inode;	// inode节点
	char name[NAME_LEN];	// 文件名
};

extern struct m_inode inode_table[NR_INODE];// inode数组 32项
extern struct file file_table[NR_FILE];		// 文件表数组64项
extern struct super_block super_block[NR_SUPER];	// 超级块数组
extern struct buffer_head * start_buffer;	// 缓冲区起始内存位置
extern int nr_buffers;		// 缓存块数


extern void check_disk_change(int dev);
extern int floppy_change(unsigned int nr);
extern int ticks_to_floppy_on(unsigned int dev);
extern void floppy_on(unsigned int dev);
extern void floppy_off(unsigned int dev);

// 文件系统操作管理函数原型
// 将inode指向的文件截为0
extern void truncate(struct m_inode * inode);
// 刷新inode信息
extern void sync_inodes(void);
// 等待指定的inode
extern void wait_on(struct m_inode * inode);
// 逻辑块位图操作, 取数据块block在设备上对应的逻辑块号
extern int bmap(struct m_inode * inode,int block);
// 创建数据块block在设备上对应的逻辑块, 返回block
extern int create_block(struct m_inode * inode,int block);
// 获取指定的路径名的inode
extern struct m_inode * namei(const char * pathname);
// 根据路径名为打开文件作准备
extern int open_namei(const char * pathname, int flag, int mode,
	struct m_inode ** res_inode);
// 释放一个inode回写入设备
extern void iput(struct m_inode * inode);
// 从设备中读取指定设备号的inode
extern struct m_inode * iget(int dev,int nr);
// 从inode_table中获取一个空闲的inode
extern struct m_inode * get_empty_inode(void);
// 获取一个pipe node
extern struct m_inode * get_pipe_inode(void);
// hash表中查找指定数据块, 返回找到的块缓存头指针
extern struct buffer_head * get_hash_table(int dev, int block);
// 优先从hash表查找读取设备指定块
extern struct buffer_head * getblk(int dev, int block);
// 读写数据块
extern void ll_rw_block(int rw, struct buffer_head * bh);
// 释放指定的数据块
extern void brelse(struct buffer_head * buf);
// 读取指定的数据块
extern struct buffer_head * bread(int dev,int block);
// 读取1页缓冲区到指定地址的内存中
extern void bread_page(unsigned long addr,int dev,int b[4]);
// 读取头一个指定的数据库, 并标记后续要读的块
extern struct buffer_head * breada(int dev,int block,...);
// 向dev申请一个逻辑块, 返回block
extern int new_block(int dev);
// 释放block
extern void free_block(int dev, int block);
// 为设备dev创建一个新的inode
extern struct m_inode * new_inode(int dev);
// 删除文件时释放一个inode
extern void free_inode(struct m_inode * inode);
// 刷新指定设备缓冲区
extern int sync_dev(int dev);
// 读取指定设备的超级块
extern struct super_block * get_super(int dev);
extern int ROOT_DEV;

// 挂载根文件系统
extern void mount_root(void);

#endif
