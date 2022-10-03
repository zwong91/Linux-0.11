/*
 * This file contains some defines for the AT-hd-controller.
 * Various sources. Check out some definitions (see comments with
 * a ques).
 */
#ifndef _HDREG_H
#define _HDREG_H

/* Hd controller regs. Ref: IBM AT Bios-listing */
// 硬盘控制器寄存器端口
#define HD_DATA		0x1f0	/* _CTL when writing */
#define HD_ERROR	0x1f1	/* see err-bits */
#define HD_NSECTOR	0x1f2	/* nr of sectors to read/write */
#define HD_SECTOR	0x1f3	/* starting sector */
#define HD_LCYL		0x1f4	/* starting cylinder */
#define HD_HCYL		0x1f5	/* high byte of starting cyl */
#define HD_CURRENT	0x1f6	/* 101dhhhh , d=drive, hhhh=head */
#define HD_STATUS	0x1f7	/* see status-bits */
#define HD_PRECOMP HD_ERROR	/* same io address, read=error, write=precomp */
#define HD_COMMAND HD_STATUS	/* same io address, read=status, write=cmd */

#define HD_CMD		0x3f6	// 控制寄存器端口

/* Bits of HD_STATUS */
#define ERR_STAT	0x01
#define INDEX_STAT	0x02
#define ECC_STAT	0x04	/* Corrected error */
#define DRQ_STAT	0x08
#define SEEK_STAT	0x10
#define WRERR_STAT	0x20
#define READY_STAT	0x40
#define BUSY_STAT	0x80

/* Values for HD_COMMAND */
#define WIN_RESTORE		0x10
#define WIN_READ		0x20
#define WIN_WRITE		0x30
#define WIN_VERIFY		0x40
#define WIN_FORMAT		0x50
#define WIN_INIT		0x60
#define WIN_SEEK 		0x70
#define WIN_DIAGNOSE	0x90
#define WIN_SPECIFY		0x91

/* Bits for HD_ERROR */
#define MARK_ERR	0x01	/* Bad address mark */
#define TRK0_ERR	0x02	/* couldn't find track 0 */
#define ABRT_ERR	0x04	/* abort */
#define ID_ERR		0x10	/* ID not found */
#define ECC_ERR		0x40	/* ECC */
#define	BBD_ERR		0x80	/* 坏扇区 */

// 硬盘分区表结构 16字节, 存放在硬盘的0柱面0磁头1扇区的0x1BE-0x1FD
struct partition {
	unsigned char boot_ind;		/*0x00 0x80 - active (unused) 0x80表示可引导操作系统*/
	unsigned char head;			/*0x01 起始磁头号 */
	unsigned char sector;		/*0x02 起始扇区号0-5位, 起始柱面号6-7位 */
	unsigned char cyl;			/*0x03 起始柱面号低8位 */
	unsigned char sys_ind;		/*0x04 0x0b-DOS,0x80-Minix,0x83-Linux */
	unsigned char end_head;		/*0x05 结束磁头号 */
	unsigned char end_sector;	/*0x06 结束扇区号0-5位, 结束柱面号6-7位 */
	unsigned char end_cyl;		/*0x07 结束柱面号低8位 */
	unsigned int start_sect;	/*0x08-0x0b starting sector counting from 0 */
	unsigned int nr_sects;		/*0x0c-0x0f nr of sectors in partition */
};

#endif
