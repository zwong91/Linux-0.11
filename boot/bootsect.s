	.code16
# rewrite with AT&T syntax by falcon <wuzhangjin@gmail.com> at 081012
#
# SYS_SIZE is the number of clicks (16 bytes) to be loaded.
# 0x3000 is 0x30000 bytes = 192kB, more than enough for current
# versions of linux
#
	.equ SYSSIZE, 0x3000 # 编译链接后system模块默认的最大值 192KB
#
#	bootsect.s		(C) 1991 Linus Torvalds
#
# bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
# iself out of the way to address 0x90000, and jumps there.
#
# It then loads 'setup' directly after itself (0x90200), and the system
# at 0x10000, using BIOS interrupts. 
#
# NOTE! currently system is at most 8*65536 bytes long. This should be no
# problem, even in the future. I want to keep it simple. This 512 kB
# kernel size should be enough, especially as this doesn't contain the
# buffer cache as in minix
#
# The loader has been made as simple as possible, and continuos
# read errors will result in a unbreakable loop. Reboot by hand. It
# loads pretty fast by getting whole sectors at a time whenever possible.

	.global _start, begtext, begdata, begbss, endtext, enddata, endbss  # 6个全局描述符
	.text	# 代码段
	begtext:
	.data	# 数据段
	begdata:
	.bss	# Block Started by Symbol未初始化数据段 buffer
	begbss:
	.text

	.equ SETUPLEN, 4		# nr of setup-sectors setup程序的扇区数
	.equ BOOTSEG, 0x07c0		# original address of boot-sector 31KB处, bootsect的起始地址(段地址,下同)
	.equ INITSEG, 0x9000		# we move boot here - out of the way 576KB处, 将bootsect移动到这里, 滚开
	.equ SETUPSEG, 0x9020		# setup starts here 4个扇区 576.5KB处, setup程序开始地址
	.equ SYSSEG, 0x1000		# system loaded at 0x10000 (65536 64KB处). 确保system不会超过0x80000 512KB 大小
	.equ ENDSEG, SYSSEG + SYSSIZE	# where to stop loading

# ROOT_DEV:	0x000 - same type of floppy as boot.
# dev_no = (major << 8) + minor
# major: 1 -内存, 2 -磁盘, 3 -硬盘, 4 -ttyx, 5 -tty, 6 -串口, 7 - non-named pipe
#		0x300 - /dev/hd0 代表整个第1个硬盘
#		0x301 - first partition on first drive etc
#       ...
#       0x304 /dev/hd4
#       0x305 /dev/hd5 代表整个第2块硬盘
#       0x306 /dev/hd6 第2块硬盘的第一个分区
#       ...
#       0x309 /dev/hd9 第2个盘第4个分区


##和源码不同，源码中是0x306, start 将自身bootsect从目前的0x07c0移动到0x9000, 共256word=512KB, 然后跳转到移动后的go标号
#
	.equ ROOT_DEV, 0x301
	ljmp    $BOOTSEG, $_start #让链接程序知道, 程序从start标号开始执行
_start:
	mov	$BOOTSEG, %ax	
	mov	%ax, %ds		#将ds段寄存器设置为0x07C0
	mov	$INITSEG, %ax	
	mov	%ax, %es		#将es段寄存器设置为0x9000
	mov	$256, %cx		#设置移动计数值256字
	sub	%si, %si		#源地址	ds:si = 0x07C0:0x0000
	sub	%di, %di		#目标地址 es:si = 0x9000:0x0000
	rep					#重复执行并递减cx的值到0为止
	movsw				#从内存[si]处移动cx个字到[di]处
	ljmp	$INITSEG, $go	#段间跳转，这里INITSEG指出跳转到的段地址，解释了cs的值为0x9000
go:	mov	%cs, %ax		#将ds，es，ss都设置成移动后代码所在的段处(0x9000)
	mov	%ax, %ds
	mov	%ax, %es
# 程序有堆栈操作push/pop/call, 必须设置堆栈, sp > 0x200 + 0x200 * 4 + 堆栈大小, 高高的
# put stack at 0x9ff00. 系统初始化时临时使用的堆栈ss:esp 0x9000:0xff00
	mov	%ax, %ss
	mov	$0xFF00, %sp		# arbitrary value >>512  0x90200

# load the setup-sectors directly after the bootblock.
# Note that 'es' is already set up.

# 利用BIOS中断0x13将setup 模块从磁盘第2-5扇区读到0x90200开始处
##ah=0x02 读磁盘扇区到内存	al＝需要读出的扇区数量
##ch=磁道(柱面)号的低八位   cl＝开始扇区(位0-5),磁道号高2位(位6－7)
##dh=磁头号					dl=驱动器号(硬盘则7要置位)
##es:bx ->指向数据缓冲区；如果出错则CF标志置位,ah中是出错码
#
load_setup:
	mov	$0x0000, %dx		# drive 0, head 0
	mov	$0x0002, %cx		# sector 2, track 0
	mov	$0x0200, %bx		# address = 512, in INITSEG
	.equ    AX, 0x0200+SETUPLEN
	mov     $AX, %ax		# service 2, nr of sectors
	int	$0x13			# read it
	jnc	ok_load_setup		# ok - continue
	mov	$0x0000, %dx
	mov	$0x0000, %ax		# reset the diskette
	int	$0x13
	jmp	load_setup

ok_load_setup:

# Get disk drive parameters, specifically nr of sectors/track 获取磁盘驱动器参数表, 特别时磁盘扇区数量

	mov	$0x00, %dl
	mov	$0x0800, %ax		# AH=8 is get drive parameters
	int	$0x13
	mov	$0x00, %ch
	#seg cs
	mov	%cx, %cs:sectors+0	# %cs means sectors is in %cs
	mov	$INITSEG, %ax
	mov	%ax, %es	  		#上面取磁盘参数中断改变了es的值, 这里改回来

# Print some inane message  打印输出一些信息('Loading system ...' , 13, 10, 0 )

	mov	$0x03, %ah		# read cursor pos 读取光标位置
	xor	%bh, %bh
	int	$0x10
	
	mov	$24, %cx		# 24个字符
	mov	$0x0007, %bx		# page 0, attribute 7 (normal)
	#lea	msg1, %bp
	mov     $msg1, %bp		# 指向要显示的字符串
	mov	$0x1301, %ax		# write string, move cursor 写字符串并移动光标
	int	$0x10

# ok, we've written the message, now we want to load the system (at 0x10000) 现在开始加载system模块到0x10000

	mov	$SYSSEG, %ax
	mov	%ax, %es		# segment of 0x010000
	call	read_it
	call	kill_motor # 关闭驱动器马达, 这样可以知道驱动器的状态了

# After that we check which root-device to use. If the device is
# defined (#= 0), nothing is done and the given device is used.
# Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
# on the number of sectors that the BIOS reports currently.

	#seg cs
	mov	%cs:root_dev+0, %ax
	cmp	$0, %ax
	jne	root_defined
	#seg cs
	mov	%cs:sectors+0, %bx  # 取上面保存的每磁道扇区数 eqlual 15, 引导驱动器的设备号为0x021c
	mov	$0x0208, %ax		# /dev/ps0 - 1.2Mb
	cmp	$15, %bx
	je	root_defined
	mov	$0x021c, %ax		# /dev/PS0 - 1.44Mb
	cmp	$18, %bx
	je	root_defined
undef_root:
	jmp undef_root  	 	# 如果都不一样, 去死吧
root_defined:
	#seg cs
	mov	%ax, %cs:root_dev+0 #将检查过的设备号保存起来

# after that (everyting loaded), we jump to
# the setup-routine loaded directly after
# the bootblock:

	ljmp	$SETUPSEG, $0  # 跳转到0x9020:0000的setup.s程序开始处, 本程序到此结束了!!! -)


## read_it 子程序, 将system模块加载到0x10000, 尽可能每次加载整个磁道数据
# This routine loads the system at address 0x10000, making sure
# no 64kB boundaries are crossed. We try to load it as fast as
# possible, loading whole tracks whenever we can.
#
# in:	es - starting address segment (normally 0x1000)
#
sread:	.word 1+ SETUPLEN	# sectors read of current track 当前磁道已读扇区数, 1 引导扇区, bootsect/setup的SETUPLEN
head:	.word 0			# current head 当前磁头号
track:	.word 0			# current track 当前磁道号

read_it:
	mov	%es, %ax
	test	$0x0fff, %ax  	# 测试es段, 磁盘读入的数据必须放在位于内存地址64KB边界开始处，否则死循环了
die:	jne 	die			# es must be at 64kB boundary
	xor 	%bx, %bx		# bx is starting address within segment 清空bx寄存器,用于表示当前段内存放数据开始位置
rp_read:
	mov 	%es, %ax
 	cmp 	$ENDSEG, %ax		# have we loaded all yet? 是否已经加载了全部数据, 否则就跳转ok1_read继续读数据
	jb	ok1_read
	ret
# ax: 计算和验证当前需要读取的扇区数
# 根据ax以及段内数据偏移开始位置, 计算全部读取这些未读扇区是否超过64KB段长度限制
# 超过则一次最多扇区 = 最多读取字节数 64KB - 段内偏移地址
ok1_read:
	#seg cs
	mov	%cs:sectors+0, %ax # 取每磁道扇区数
	sub	sread, %ax 		# 减去当前磁道已读扇区数
	mov	%ax, %cx		# cx = ax = 当前磁道未读扇区数
	shl	$9, %cx			# cx = cx * 512 bytes
	add	%bx, %cx		# cx = cx + 段内当前偏移值bx = 此次读操作后,段内共读入的字节数
	jnc 	ok2_read	# 若没有超过64KB跳转到ok2_read
	je 	ok2_read
	xor 	%ax, %ax	# 若加上此次读磁盘上未读扇区数超过64KB, 计算出实际实际能读的扇区数
	sub 	%bx, %ax
	shr 	$9, %ax
ok2_read:
	call 	read_track
	mov 	%ax, %cx	# cx = 该次操作已读取的扇区数
	add 	sread, %ax	# 当前磁道上已读取的扇区数
	#seg cs
	cmp 	%cs:sectors+0, %ax  # 如果当前磁盘上还有未读扇区, 跳转到ok3_read
	jne 	ok3_read
	mov 	$1, %ax 	# 读该磁道的1号磁头反面上的数据, 如果已经完成了, 读下一个磁道
	sub 	head, %ax	# 判断当前磁头号
	jne 	ok4_read	# 若0磁头号, 继续读1磁头号上的扇区数据
	incw    track 		# 否则读下一个磁道
ok4_read:
	mov	%ax, head		# 保存当前磁头号
	xor	%ax, %ax		# 清空当前磁道已读扇区数
ok3_read:
	mov	%ax, sread		# 保存当前磁道已读扇区数
	shl	$9, %cx			# 上次已读扇区数 * 512
	add	%cx, %bx		# 调整当前段内数据开始位置
	jnc	rp_read			# 若小于64KB,跳转到rp_raed继续读, 否则调整当前段,为读下一段数据做准备
	mov	%es, %ax
	add	$0x1000, %ax	# 将段基址指向下一个64KB内存开始处
	mov	%ax, %es	
	xor	%bx, %bx		# 清空段内数据开始偏移值
	jmp	rp_read			# 跳转到rp_read继续读数据

# 读取当前磁道指定的开始扇区和需要读取扇区数到es:bx开始处
# BIOS int 0x13, ah =2, al - 需要读取扇区数, es:bx -缓冲区开始位置
read_track:
	push	%ax
	push	%bx
	push	%cx
	push	%dx
	mov	track, %dx		# 取当前磁道号
	mov	sread, %cx		# 取当前磁道上已读扇区数
	inc	%cx				# cl = 开始读扇区
	mov	%dl, %ch		# ch = 当前磁道号
	mov	head, %dx		# 取当前磁头号
	mov	%dl, %dh		# dh = 磁头号
	mov	$0, %dl			# dl = 驱动器号, 0表示A驱
	and	$0x0100, %dx	# 磁头号<=1
	mov	$2, %ah			# ah = 2, 取磁盘扇区功能编号
	int	$0x13
	jc	bad_rt			# 若出错, 跳转到bad_rt
	pop	%dx
	pop	%cx
	pop	%bx
	pop	%ax
	ret
# 执行驱动器复位操作, ax = 0功能编号, 再跳转到read_track重试
bad_rt:	mov	$0, %ax
	mov	$0, %dx
	int	$0x13
	pop	%dx
	pop	%cx
	pop	%bx
	pop	%ax
	jmp	read_track

#/* 这个子程序用于关闭软驱的马达, 这样我们进入内核后它处于禁用状态, 不用关心了。
# * This procedure turns off the floppy drive motor, so
# * that we enter the kernel in a known state, and
# * don't have to worry about it later.
# */
kill_motor:
	push	%dx
	mov	$0x3f2, %dx
	mov	$0, %al
	outsb
	pop	%dx
	ret

sectors:
	.word 0


msg1:
	.byte 13,10		# \r\n的ASCII码
	.ascii "Loading system ..."
	.byte 13,10,13,10	# 总共24个ASCII码字符

	.org 508	# 表示下面的语句从地址508=0x1FC开始
root_dev:
	.word ROOT_DEV # 保存根文件系统设备号于0x508处的两个字节
boot_flag:
	.word 0xAA55	# boot引导扇区有效标识
	
	.text
	endtext:
	.data
	enddata:
	.bss
	endbss:
