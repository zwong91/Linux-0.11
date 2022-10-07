/*
 *  linux/fs/bitmap.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* bitmap.c contains the code that handles the inode and block bitmaps */
#include <string.h>

#include <linux/sched.h>
#include <linux/kernel.h>

// 将指定地址addr处的一块=1024B内存清零
// %0 - eax = 0, %1 - ecx = 256, %2 - edi = addr
#define clear_block(addr) \
__asm__ __volatile__ ("cld\n\t" \
	"rep\n\t" \
	"stosl" \
	::"a" (0),"c" (BLOCK_SIZE/4),"D" ((long) (addr)))

/// 将指定地址addr开始的第nr位 置1, nr 可大于 32, 返回原bits值
// %0 - eax返回值, %1 - eax = 0, %2 - nr, %3 - addr的内容
// res局部寄存器变量用于内联汇编, btsl Bit Test and Set 指令测试并设置bits
// addr地址和bit位偏移值指定的bits位值先保存在CF 进位标志中; 然后设置该bits = 1
// setb指令根据CF设置%al, 若CF = 1则 %al = 1, 否则 %al = 0
#define set_bit(nr,addr) ({\
register int res ; \
__asm__ __volatile__("btsl %2,%3\n\tsetb %%al": \
"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res;})

/// 将指定地址addr开始的第nr位 置0复位, nr 可大于 32, 返回原bits值反码
// %0 - eax返回值, %1 - eax = 0, %2 - nr 位偏移值, %3 - addr的内容
// res局部寄存器变量用于内联汇编, btrl Bit Test and Reset 指令测试并设置bits
// addr地址和bit位偏移值指定的bits位值先保存在CF 进位标志中; 然后设置该bits = 0 重置
// setnb指令根据CF设置%al, 若CF = 1则 %al = 0, 否则 %al = 1
#define clear_bit(nr,addr) ({\
register int res ; \
__asm__ __volatile__("btrl %2,%3\n\tsetnb %%al": \
"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res;})

/// 从指定地址addr开始寻找第1个为0的bits, addr是缓冲块数据区的起始地址占8192bits=1024字节
// %0 - ecx 返回值, %1 - ecx =0, %2 - esi = addr
// #define find_first_zero(addr) ({ \
// int __res; \
// __asm__ __volatile__ ("cld\n" \	// 清方向位 esi/edi++ 方向自动生长
// 	"1:\tlodsl\n\t" \				// eax = [esi]
// 	"notl %%eax\n\t" \				// eax取反
// 	"bsfl %%eax,%%edx\n\t" \		// 从位 0 开始遍历eax中为1的第一个1的位, 其偏移值放入edx
// 	"je 2f\n\t" \					// 如果eax里全为0, 则向前跳转标号2
// 	"addl %%edx,%%ecx\n\t" \		// 偏移值加入ecx
// 	"jmp 3f\n" \					// 向前跳转到标号3
// 	"2:\taddl $32,%%ecx\n\t" \		// 未找到0值的位, 则ecx加偏移量32
// 	"cmpl $8192,%%ecx\n\t" \		// 已经遍历完8192 bits即1024字节了吗?
// 	"jl 1b\n" \						// 若没有遍历完1块数据, 则向前跳转到标号1
// 	"3:" \							// 结束, 此时ecx是位的偏移量
// 	:"=c" (__res):"c" (0),"S" (addr)); \
// __res;})

#define find_first_zero(addr) ({ \
int __res; \
__asm__ __volatile__ ("cld\n" \
	"1:\tlodsl\n\t" \
	"notl %%eax\n\t" \
	"bsfl %%eax,%%edx\n\t" \
	"je 2f\n\t" \
	"addl %%edx,%%ecx\n\t" \
	"jmp 3f\n" \
	"2:\taddl $32,%%ecx\n\t" \
	"cmpl $8192,%%ecx\n\t" \
	"jl 1b\n" \
	"3:" \
	:"=c" (__res):"c" (0),"S" (addr)); \
__res;})

void free_block(int dev, int block)
{
	struct super_block * sb;
	struct buffer_head * bh;
	// 获取设备dev上文件系统的超级块信息
	if (!(sb = get_super(dev)))
		panic("trying to free block on nonexistent device");
	// 检查逻辑块号block 小于dev上数据区第一个逻辑块号或者 大于dev上总逻辑块数
	if (block < sb->s_firstdatazone || block >= sb->s_nzones)
		panic("trying to free block not in datazone");
	// 从hash表中查找该数据块, 存在则验证是否有效, 清除dirt和uptodate标记, 释放该数据块
	bh = get_hash_table(dev,block);
	if (bh) {
		if (bh->b_count > 1) { // 如果引用计数大于1, 则调用brelse
			printk("block busy, trying to free block (%04x:%d), count=%d\n",
				dev,block,bh->b_count);
			brelse(); // 该块还有人用, b_count--后直接退出
			return;
		}
		bh->b_dirt=0;
		bh->b_uptodate=0;
		if (bh->b_count /*b_count为1*/)
			brelse(bh);
	}
	// 计算block在数据区开始算起的逻辑块号, 从 1 开始计数, 然后对逻辑块bitmap操作
	block -= sb->s_firstdatazone - 1 ;
	// 由于1个缓冲块为1024字节=8192bits, block/8192 = blockmap的哪个块, block&8191 = blockmap的哪个块的bits位即偏移量
	// 如果原来的bits为0, panic
	if (clear_bit(block&8191,sb->s_zmap[block/8192]->b_data)) {
		printk("block (%04x:%d) ",dev,block+sb->s_firstdatazone-1);
		panic("free_block: bit already cleared");
	}
	// 最后逻辑块位图z_map dirt 置1, 你不纯洁了
	sb->s_zmap[block/8192]->b_dirt = 1;
}

int new_block(int dev)
{
	struct buffer_head * bh;
	struct super_block * sb;
	int i,j;
	// 获取设备dev的超级块
	if (!(sb = get_super(dev)))
		panic("trying to get new block from nonexistant device");
	// 遍历文件系统的8个逻辑块位图s_zmap, 开始寻找缓冲块中第1个为0的bits,8192bits=1024字节(0表示的是空闲逻辑块)
	// 获取放置该空闲逻辑块上的块号
	j = 8192;
	for (i=0 ; i<8 ; i++)
		if ((bh=sb->s_zmap[i]))
			if ((j=find_first_zero(bh->b_data))<8192)
				break;
	// 全部遍历完8块逻辑块位图的所有的bits位, 还没有 0 值的bit位或者bh无效, 文件系统硬盘满了
	if (i>=8 || !bh || j>=8192)
		return 0;
	// 设置找到的新逻辑块 j 对应的逻辑块位图中bits位
	if (set_bit(j,bh->b_data))
		panic("new_block: bit already set");
	bh->b_dirt = 1;
	// 逻辑块位图仅仅表示硬盘上数据区中逻辑块占用情况即s_zmap中bits位偏移值表示从数据区开始算起的块号
	// 把 j 转换为逻辑块号
	j += i*8192 + sb->s_firstdatazone-1;
	if (j >= sb->s_nzones) // 新逻辑块号 > 硬盘上总逻辑块数
		return 0;
	// 在高速缓存区为 j 获取一个缓冲块
	if (!(bh=getblk(dev,j))) // b_count = 1
		panic("new_block: cannot get block");
	if (bh->b_count != 1)
		panic("new block: count is != 1");
	clear_block(bh->b_data); // 将逻辑块数据清0
	bh->b_uptodate = 1;
	bh->b_dirt = 1;
	brelse(bh);		// b_count--
	return j;
}

void free_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;

	// inode 参数幂等性检查
	if (!inode) // inode 为 NULL
		return;
	if (!inode->i_dev) { // dev 为 0 说明inode没有使用, inode内存块清0
		memset(inode,0,sizeof(*inode));
		return;
	}
	if (inode->i_count>1) { // 引用计数大于1, 还有其他程序使用呢
		printk("trying to free inode with count=%d\n",inode->i_count);
		panic("free_inode");
	}
	if (inode->i_nlinks) // 文件连接数不为0, 还有其他文件目录使用该inode
		panic("trying to free inode with links");
	// 获取inode所在dev的超级块
	if (!(sb = get_super(inode->i_dev)))
		panic("trying to free inode on nonexistent device");
	// inode 节点为 0 或者大于该dev上inode节点总数 (0号inode保留place_holder)
	if (inode->i_num < 1 || inode->i_num > sb->s_ninodes)
		panic("trying to free inode 0 or nonexistant inode");
	// 一个缓冲块inode有8192 bits, i_num >> 13 = i_num / 8192 在哪个inode块上
	if (!(bh=sb->s_imap[inode->i_num>>13]))
		panic("nonexistent imap in superblock");
	// inode对应的imap中bits位 置0, 原来的bits位为1出错了
	if (clear_bit(inode->i_num&8191,bh->b_data))
		printk("free_inode: bit already cleared.\n\r");
	bh->b_dirt = 1;
	memset(inode,0,sizeof(*inode));
}

struct m_inode * new_inode(int dev)
{
	struct m_inode * inode;
	struct super_block * sb;
	struct buffer_head * bh;
	int i,j;
	// 从inode_table中获取一个空闲的inode
	if (!(inode=get_empty_inode()))
		return NULL;
	// 获取dev的超级块信息
	if (!(sb = get_super(dev)))
		panic("new_inode with unknown device");
	// 遍历文件系统的8个inode位图s_imap, 开始寻找缓冲块中第1个为0的bits,8192bits=1024字节(0表示的是空闲inode)
	// 获取放置该空闲inode上的inode号
	j = 8192;
	for (i=0 ; i<8 ; i++)
		if ((bh=sb->s_imap[i]))
			if ((j=find_first_zero(bh->b_data))<8192)
				break;
	// 全部遍历完8块逻辑inode位图的所有的bits位, 还没有 0 值的bit位或者bh无效
	if (!bh || j >= 8192 || j+i*8192 > sb->s_ninodes) {
		iput(inode);
		return NULL;
	}
	// 找到还未使用的inode号 j, i节点 j 对应的 i map 的bits位 置1
	if (set_bit(j,bh->b_data))
		panic("new_inode: bit already set");
	bh->b_dirt = 1;
	// 初始化该inode
	inode->i_count=1;
	inode->i_nlinks=1;
	inode->i_dev=dev;
	inode->i_uid=current->euid;
	inode->i_gid=current->egid;
	inode->i_dirt=1;
	inode->i_num = j + i*8192;	// 对应设备中的inode号
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	return inode;
}
