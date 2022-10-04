// 内核中所有系统调用函数原型
extern int sys_setup();     // 系统启动初始化设置函数
extern int sys_exit();      // 程序退出
extern int sys_fork();      // 创建进程
extern int sys_read();      // 读文件
extern int sys_write();     // 写文件
extern int sys_open();      // 打开文件
extern int sys_close();     // 关闭文件
extern int sys_waitpid();   // 等待进程退出
extern int sys_creat();     // 创建文件
extern int sys_link();      // 创建一个文件的硬链接
extern int sys_unlink();    // 删除一个文件的链接
extern int sys_execve();    // 执行程序
extern int sys_chdir();     // 改变当前目录
extern int sys_time();      // 获取当前时间
extern int sys_mknod();     // 创建字符/块设备文件
extern int sys_chmod();     // 修改文件属性
extern int sys_chown();     // 修改文件所有者和所在组
extern int sys_break();     // brk 堆内存
extern int sys_stat();      // 使用路径名获取文件信息
extern int sys_lseek();     // 重定位文件读写偏移量
extern int sys_getpid();    // 获取进程id
extern int sys_mount();     // 挂载文件系统
extern int sys_umount();    // 卸载文件系统
extern int sys_setuid();    // 设置进程用户id
extern int sys_getuid();    // 获取进程用户id
extern int sys_stime();     // 设置系统日期时间
extern int sys_ptrace();    // 程序调试
extern int sys_alarm();     // 设置报警
extern int sys_fstat();     // 使用fd获取文件信息
extern int sys_pause();     // 暂停挂起程序
extern int sys_utime();     // 修改文件访问和修改时间
extern int sys_stty();      // 修改终端行设置
extern int sys_gtty();      // 获取终端行设置
extern int sys_access();    // 检查用户对文件的访问权限
extern int sys_nice();      // 设置进程执行优先级
extern int sys_ftime();     // 获取日期和时间
extern int sys_sync();      // 同步高速缓冲与设备的数据
extern int sys_kill();      // 终止一个进程
extern int sys_rename();    // 更改文件名
extern int sys_mkdir();     // 创建目录
extern int sys_rmdir();     // 删除目录
extern int sys_dup();       // 复制文件描述符
extern int sys_pipe();      // 创建管道
extern int sys_times();     // 获取运行时间
extern int sys_prof();      // 程序执行时间区域
extern int sys_brk();       // 修改数据段长度 buffer
extern int sys_setgid();    // 设置进程组id
extern int sys_getgid();    // 获取进程组id
extern int sys_signal();    // 信号处理
extern int sys_geteuid();   // 获取进程有效用户id
extern int sys_getegid();   // 获取进程有效组id
extern int sys_acct();      // 进程记账
extern int sys_phys();      // 
extern int sys_lock();
extern int sys_ioctl();     // 设备控制
extern int sys_fcntl();     // 文件操作
extern int sys_mpx();      
extern int sys_setpgid();   // 设置进程组id
extern int sys_ulimit();
extern int sys_uname();     // 显示系统信息
extern int sys_umask();     // 获取默认文件创建属性标识
extern int sys_chroot();    // 改变根目录
extern int sys_ustat();     // 获取文件系统信息
extern int sys_dup2();      // 复制文件句柄
extern int sys_getppid();   // 获取父进程id
extern int sys_getpgrp();   // 获取进程组id, == getpgid(0)
extern int sys_setsid();    // 在新会话中运行程序
extern int sys_sigaction(); // 修改信号处理handler
extern int sys_sgetmask();  // 获取信号掩码
extern int sys_ssetmask();  // 设置信号掩码
extern int sys_setreuid();  // 设置真实/有效的用户id
extern int sys_setregid();  // 设置真实/有效的组id
extern int sys_iam();
extern int sys_whoami();
extern int sys_sethostname();

// 系统调用函数指针表, 用于系统调用中断处理程序0x80
fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read,
sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,
sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,
sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, sys_alarm,
sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
sys_nice, sys_ftime, sys_sync, sys_kill, sys_rename, sys_mkdir,
sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, sys_setgid,
sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, sys_phys,
sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,
sys_uname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,
sys_getpgrp, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
sys_setreuid,sys_setregid, sys_iam, sys_whoami, sys_sethostname };
