#ifndef _CONST_H
#define _CONST_H

#define BUFFER_END      0x200000    // 定义缓存使用内存的末端2M, 代码中未使用

// inode节点中i_mode字段的标志位
#define I_TYPE          0170000     // inode节点类型    
#define I_DIRECTORY	    0040000     // 目录文件
#define I_REGULAR       0100000     // 常规文件
#define I_BLOCK_SPECIAL 0060000     // 块设备文件
#define I_CHAR_SPECIAL  0020000     // 字符设备文件
#define I_NAMED_PIPE	0010000     // 命名管道文件
#define I_SET_UID_BIT   0004000     // 设置有效的用户id类型
#define I_SET_GID_BIT   0002000     // 设置有效的组id类型

#endif
