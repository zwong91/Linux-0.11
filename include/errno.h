#ifndef _ERRNO_H
#define _ERRNO_H

/*
 * ok, as I hadn't got any other source of information about
 * possible error numbers, I was forced to use the same numbers
 * as minix.
 * Hopefully these are posix or something. I wouldn't know (and posix
 * isn't telling me - they want $$$ for their f***ing standard).
 *
 * We don't use the _SIGN cludge of minix, so kernel returns must
 * see to the sign by themselves.
 *
 * NOTE! Remember to change strerror() if you change this file!
 */

// ANSI C X3J11 & POSIX != SCO UNIX v6/ minix fuck 混蛋标准要钱 
extern int errno;

#define ERROR		99      // 一般性错误
#define EPERM		 1      // 操作没有许可
#define ENOENT		 2      // 文件或目录不存在
#define ESRCH		 3      // 指定的进程不存在
#define EINTR		 4      // 中断的系统调用
#define EIO		     5      // 输入输出出错
#define ENXIO		 6      // 指定设备或地址不存在
#define E2BIG		 7      // 参数列表太长
#define ENOEXEC		 8      // 执行程序格式错误
#define EBADF		 9      // 文件句柄描述符fd错误
#define ECHILD		10      // 子进程不存在
#define EAGAIN		11      // 资源暂时不可用
#define ENOMEM		12      // 内存不足
#define EACCES		13      // 没有许可权限
#define EFAULT		14      // 地址错误
#define ENOTBLK		15      // 不是块设备文件
#define EBUSY		16      // 资源正忙
#define EEXIST		17      // 文件已存在
#define EXDEV		18      // 非法连接
#define ENODEV		19      // 设备不存在
#define ENOTDIR		20      // 不是目录文件
#define EISDIR		21      // 是目录文件
#define EINVAL		22      // 参数无效
#define ENFILE		23      // 系统打开文件数过多
#define EMFILE		24      // 打开文件数太多
#define ENOTTY		25      // 没有tty终端
#define ETXTBSY		26      // 不再使用
#define EFBIG		27      // 文件太大
#define ENOSPC		28      // 设备已满, 没有空闲空间
#define ESPIPE		29      // 无效文件重定位
#define EROFS		30      // 文件系统只读
#define EMLINK		31      // 连接太多
#define EPIPE		32      // 管道错误
#define EDOM		33      // domain 错误
#define ERANGE		34      // 结果太大
#define EDEADLK		35      // 避免资源死锁
#define ENAMETOOLONG	36  // 文件名太长
#define ENOLCK		37      // 没有锁可用
#define ENOSYS		38      // 功能还没有实现
#define ENOTEMPTY	39      // 目录不为空

#endif
