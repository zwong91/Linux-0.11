/*
 *  linux/fs/truncate.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/sched.h>

#include <sys/stat.h>

/// inode 节点 i_zone[0-6]- 直接块号, i_zone[7] - 一次间接块号, i_zone[8] - 二次 间接块号

// 释放所有的一次间接块
static void free_ind(int dev,int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block)	// 逻辑块号为0
		return;
	// 读取一次间接块, 释放上面的使用的所有逻辑块
	if ((bh=bread(dev,block))) {
		p = (unsigned short *) bh->b_data; // 指向一次间接缓冲块数据, 存的是逻辑块的索引
		for (i=0;i<512;i++,p++) // 一次间接块上有512块号, 2个字节表示一个块号 = 1024字节 = 1次间接块
			if (*p)
				free_block(dev,*p); // 依次释放dev的每个块号
		brelse(bh); // 然后释放一次间接占用的缓冲块
	}
	// 最后释放dev上的一次间接块
	free_block(dev,block);
}

// 释放所有的二次间接块
static void free_dind(int dev,int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block) // 逻辑块号为0
		return;
	// 读取二次间接块, 释放上面的使用的所有逻辑块, 再释放该一级块缓冲区
	if ((bh=bread(dev,block))) {
		p = (unsigned short *) bh->b_data; // 指向二次间接缓冲块数据, 存的是一次间接块号
		for (i=0;i<512;i++,p++)
			if (*p)
				free_ind(dev,*p); // 释放所有一次间接块
		brelse(bh); // 释放二次间接占用的缓冲区
	}
	// 最后释放dev上的二次间接块数据
	free_block(dev,block);
}

// 删除某个文件占用的inode ~ metadata和data
void truncate(struct m_inode * inode)
{
	int i;
	// 检查是否为文件或目录
	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
		return;
	// 释放inode节点的7个直接逻辑块并将这7个逻辑块项全置0 (小文件)
	for (i=0;i<7;i++)
		if (inode->i_zone[i]) {
			free_block(inode->i_dev,inode->i_zone[i]);
			inode->i_zone[i]=0;
		}
	// 逻辑块 7即块号, 嵌套
	free_ind(inode->i_dev,inode->i_zone[7]);
	// 逻辑块 8 块号, 再嵌套
	free_dind(inode->i_dev,inode->i_zone[8]);
	inode->i_zone[7] = inode->i_zone[8] = 0;
	inode->i_size = 0;	// 文件大小置0
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}

