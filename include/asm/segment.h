// Linux中, 当用户程序通过系统调用执行内核代码时, ds/es加载全局描述符表GDT中内核数据段描述符0x10, ds/es用于访问
// 内核数据段,  而fs中加载了局部描述符表LDT中任务的数据段描述符0x17, fs用于访问用户数据段
// 下面get/put_fs_xx函数专门用来访问用户程序中的数据

/*
* 读取fs段中指定地址处的字节
* 参数: addr 指定内存地址
* %0 - 返回的字节_v, %1 - 内存地址addr
* return: 返回内存fs:[addr]处的字节 
*/
static inline unsigned char get_fs_byte(const char * addr)
{
	// 定义寄存器变量_v, 该变量保存在寄存器中高效
	unsigned register char _v;

	__asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

/*
* 读取fs段中指定地址处的2个字节=字
* 参数: addr 指定内存地址
* %0 - 返回的字_v, %1 - 内存地址addr
* return: 返回内存fs:[addr]处的2个字节=字
*/
static inline unsigned short get_fs_word(const unsigned short *addr)
{
	unsigned short _v;

	__asm__ ("movw %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

/*
* 读取fs段中指定地址处的4个字节=长字
* 参数: addr 指定内存地址
* %0 - 返回的长字_v, %1 - 内存地址addr
* return: 返回内存fs:[addr]处的4个字节=长字
*/
static inline unsigned long get_fs_long(const unsigned long *addr)
{
	unsigned long _v;

	__asm__ ("movl %%fs:%1,%0":"=r" (_v):"m" (*addr)); \
	return _v;
}

/*
* 写入fs段中指定地址处的1个字节
* 参数: val - 字节值, addr 指定内存地址
* %0 - 寄存器字节值val, %1 - 内存地址addr
*/
static inline void put_fs_byte(char val,char *addr)
{
__asm__ ("movb %0,%%fs:%1"::"r" (val),"m" (*addr));
}

/*
* 写入fs段中指定地址处的2个字节 = 字
* 参数: val - 字值, addr 指定内存地址
* %0 - 寄存器字值val, %1 - 内存地址addr
*/
static inline void put_fs_word(short val,short * addr)
{
__asm__ ("movw %0,%%fs:%1"::"r" (val),"m" (*addr));
}

/*
* 写入fs段中指定地址处的4个字节 = 长字
* 参数: val - 长字值, addr 指定内存地址
* %0 - 寄存器长字值val, %1 - 内存地址addr
*/
static inline void put_fs_long(unsigned long val,unsigned long * addr)
{
__asm__ ("movl %0,%%fs:%1"::"r" (val),"m" (*addr));
}

/*
 * Someone who knows GNU asm better than I should double check the followig.
 * It seems to work, but I don't know if I'm doing something subtly wrong.
 * --- TYT, 11/24/91
 * [ nothing wrong here, Linus ]
 */

/// @brief  取fs段寄存器的值(选择符)
/// @return fs寄存器的值
static inline unsigned long get_fs() 
{
	unsigned short _v;
	__asm__("mov %%fs,%%ax":"=a" (_v):);
	return _v;
}

/// @brief 取ds寄存器的值(选择符)
/// @return  ds寄存器值
static inline unsigned long get_ds() 
{
	unsigned short _v;
	__asm__("mov %%ds,%%ax":"=a" (_v):);
	return _v;
}

/// @brief 设置fs段寄存器选择符
/// @param val -段选择符
static inline void set_fs(unsigned long val)
{
	__asm__("mov %0,%%fs"::"a" ((unsigned short) val));
}

