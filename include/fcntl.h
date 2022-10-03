#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>

/* open/fcntl - NOCTTY, NDELAY isn't implemented yet */
#define O_ACCMODE	00003	// 文件访问模式
#define O_RDONLY	   00	// 只读打开文件
#define O_WRONLY	   01	// 只写打开文件
#define O_RDWR		   02	// 读写打开文件
// 文件创建和操作标志
#define O_CREAT		00100	/* not fcntl 如果文件不存在就创建*/
#define O_EXCL		00200	/* not fcntl 独占使用文件标志*/
#define O_NOCTTY	00400	/* not fcntl 不分配控制终端*/
#define O_TRUNC		01000	/* not fcntl 若文件已存在且写, 长度截为0*/
#define O_APPEND	02000	// 追加方式, 文件指针指向尾部
#define O_NONBLOCK	04000	/* not fcntl 非阻塞方式打开和操作文件*/
#define O_NDELAY	O_NONBLOCK

/* Defines for fcntl-commands. Note that currently
 * locking isn't supported, and other things aren't really
 * tested.
 */
#define F_DUPFD		0	/* dup */
#define F_GETFD		1	/* get f_flags */
#define F_SETFD		2	/* set f_flags */
#define F_GETFL		3	/* more flags (cloexec) */
#define F_SETFL		4
#define F_GETLK		5	/* not implemented */
#define F_SETLK		6
#define F_SETLKW	7

/* for F_[GET|SET]FL */
#define FD_CLOEXEC	1	/* actually anything with low bit set goes */

/* Ok, these are locking features, and aren't implemented at any
 * level. POSIX wants them.
 */
#define F_RDLCK		0	// 共享或读文件锁定
#define F_WRLCK		1	// 独占或写锁定
#define F_UNLCK		2	// 文件解锁

/* Once again - not implemented, but ... */
struct flock {
	short l_type;		// 文件锁定类型 F_RDLCK/F_WRLCK/F_UNLCK
	short l_whence;		// 开始偏移处SEEK_SET/SEEK_CUR/SEEK_END
	off_t l_start;		// 阻塞锁定开始处, 相对偏移字节数
	off_t l_len;		// 阻塞锁定大小, 如果为0则为文件尾
	pid_t l_pid;		// 锁定的进程id
};

// 创建或重写一个已存在文件
extern int creat(const char * filename,mode_t mode);
// 文件fd操作, 会影响打开的文件
extern int fcntl(int fildes,int cmd, ...);
// 打开文件在文件和fd关联
extern int open(const char * filename, int flags, ...);

#endif
