#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>
// 文件状态结构
struct stat {
	dev_t	st_dev;		// 文件设备号
	ino_t	st_ino;		// 文件inode节点号
	umode_t	st_mode;	// 文件属性, 如下
	nlink_t	st_nlink;	// 指定文件的链接数
	uid_t	st_uid;		// 文件用户号
	gid_t	st_gid;		// 文件组id
	dev_t	st_rdev;	// 设备号 chr/blk文件
	off_t	st_size;	// 文件大小/地址
	time_t	st_atime;	// 最后访问时间
	time_t	st_mtime;	// 最后修改时间
	time_t	st_ctime;	// 最后inode修改时间
};

// 定义st_mode值的常量, 八进制表示, S -State, I -Inode, F - File, M - Mask, T - Type
#define S_IFMT  00170000
#define S_IFREG  0100000	// Regular 常规文件
#define S_IFBLK  0060000	// 块设备文件 如/dev/sda
#define S_IFDIR  0040000	// 目录文件
#define S_IFCHR  0020000	// 字符设备文件
#define S_IFIFO  0010000	// FIFO文件

#define S_ISUID  0004000	// 执行文件时设置用户ID
#define S_ISGID  0002000	// 指针文件时设置组ID
#define S_ISVTX  0001000	// 针对目录, 限制删除标识

#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)

#define S_IRWXU 00700 	//User- RWX
#define S_IRUSR 00400	//User- R
#define S_IWUSR 00200	//User- W
#define S_IXUSR 00100	//User- X

#define S_IRWXG 00070	// Group member - RWX
#define S_IRGRP 00040	// Group member - R
#define S_IWGRP 00020	// Group member - W
#define S_IXGRP 00010	// Group member - X

#define S_IRWXO 00007	// Other - RWX
#define S_IROTH 00004	// Other - R
#define S_IWOTH 00002	// Other - W
#define S_IXOTH 00001	// Other - X

extern int chmod(const char *_path, mode_t mode);
extern int fstat(int fildes, struct stat *stat_buf);
extern int mkdir(const char *_path, mode_t mode);
extern int mkfifo(const char *_path, mode_t mode);
extern int stat(const char *filename, struct stat *stat_buf);
extern mode_t umask(mode_t mask);

#endif
