	.code16
# rewrite with AT&T syntax by falcon <wuzhangjin@gmail.com> at 081012
#
#	setup.s		(C) 1991 Linus Torvalds
#
# setup.s is responsible for getting the system data from the BIOS,
# and putting them into the appropriate places in system memory.
# both setup.s and system has been loaded by the bootblock.
#
# This code asks the bios for memory/disk/other parameters, and
# puts them in a "safe" place: 0x90000-0x901FF, ie where the
# boot-block used to be. It is then up to the protected mode
# system to read them from there before the area is overwritten
# for buffer-blocks.
#

# NOTE! These had better be the same as in bootsect.s!

	.equ INITSEG, 0x9000	# we move boot here - out of the way 原来bootsect所处的段
	.equ SYSSEG, 0x1000	# system loaded at 0x10000 (65536).	system在0x10000=64KB处
	.equ SETUPSEG, 0x9020	# this is the current segment 本程序所在的段地址

	.global _start, begtext, begdata, begbss, endtext, enddata, endbss
	.text
	begtext:
	.data
	begdata:
	.bss
	begbss:
	.text

	ljmp $SETUPSEG, $_start	
_start:
	mov %cs,%ax
	mov %ax,%ds
	mov %ax,%es
#
##print some message
#
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10

	mov $29, %cx
	mov $0x000b,%bx
	mov $msg2,%bp
	mov $0x1301, %ax
	int $0x10
# ok, the read went well so we get current cursor position and save it for
# posterity. 整个读磁盘过程都正常, 现在将光标位置保存备用
	mov	$INITSEG, %ax	# this is done in bootsect already, but...
	mov	%ax, %ds # ds = 0x9000, 这已经在bootsect程序中设置过了, 现在是setup程序, Linux觉得有必要再重新设置一下
	mov	$0x03, %ah	# read cursor pos BIOS 0x10 ah=0x03 读光标功能, dh = 行号, dl = 列号
	xor	%bh, %bh
	int	$0x10		# save it in known place, con_init fetches
	mov	%dx, %ds:0	# it from 0x90000. 将光标信息放在0x90000处, 控制台初始化会来取的。
# Get memory size (extended mem, kB)

	mov	$0x88, %ah 
	int	$0x15
	mov	%ax, %ds:2 # 调用中断0x15, ah =0x88 功能号, ax = 0x100000=1M开始的扩展内存大小放在0x90002

# Get video-card data:

	mov	$0x0f, %ah
	int	$0x10
	mov	%bx, %ds:4	# bh = display page 当前显示页在0x90004
	mov	%ax, %ds:6	# al = video mode, ah = window width 显示模式0x90006, 字符列数0x90007

# check for EGA/VGA and some config parameters

	mov	$0x12, %ah
	mov	$0x10, %bl
	int	$0x10
	mov	%ax, %ds:8		# 0x90008 = ??
	mov	%bx, %ds:10		# 0x9000A = 显示内存大小, 0x9000B = 显示样式色彩
	mov	%cx, %ds:12		# 0x9000C = 显卡特性参数

# Get hd0 data

	mov	$0x0000, %ax
	mov	%ax, %ds
	lds	%ds:4*0x41, %si	# 取中断向量表0x41值即hd0参数表地址ds:si
	mov	$INITSEG, %ax
	mov	%ax, %es
	mov	$0x0080, %di	# es:di 0x9000:0x0080, 0x90080放第一个硬盘参数表数据
	mov	$0x10, %cx
	rep
	movsb

# Get hd1 data

	mov	$0x0000, %ax
	mov	%ax, %ds
	lds	%ds:4*0x46, %si	# 取中断向量表0x46值即hd1参数表地址ds:si
	mov	$INITSEG, %ax
	mov	%ax, %es
	mov	$0x0090, %di	# es:di 0x9000:0x0090, 0x90080放第二个硬盘参数表数据
	mov	$0x10, %cx
	rep
	movsb

## modify ds
	mov $INITSEG,%ax
	mov %ax,%ds
	mov $SETUPSEG,%ax
	mov %ax,%es

##show cursor pos:
	mov $0x03, %ah 
	xor %bh,%bh
	int $0x10
	mov $11,%cx
	mov $0x000c,%bx
	mov $cur,%bp
	mov $0x1301,%ax
	int $0x10
##show detail
	mov %ds:0 ,%ax
	call print_hex
	call print_nl

##show memory size
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $12, %cx
	mov $0x000a, %bx
	mov $mem, %bp
	mov $0x1301, %ax
	int $0x10

##show detail
	mov %ds:2 , %ax
	call print_hex

##show 
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $25, %cx
	mov $0x000d, %bx
	mov $cyl, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:0x80, %ax
	call print_hex
	call print_nl

##show 
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $8, %cx
	mov $0x000e, %bx
	mov $head, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:0x82, %ax
	call print_hex
	call print_nl

##show 
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $8, %cx
	mov $0x000f, %bx
	mov $sect, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:0x8e, %ax
	call print_hex
	call print_nl
#l:
#	jmp l
##
# Check that there IS a hd1 :-) 检查是否有第2个硬盘, 不存在则第2个硬盘表数据清零

	mov	$0x01500, %ax	# int 0x13 功能号 ah = 0x15
	mov	$0x81, %dl		# 驱动器号 0x8X为硬盘, 0x80第一个硬盘, 0x81第二个硬盘
	int	$0x13
	jc	no_disk1
	cmp	$3, %ah			# type = 3, 是硬盘吗?
	je	is_disk1
no_disk1:
	mov	$INITSEG, %ax	# 对第二个硬盘表清零
	mov	%ax, %es
	mov	$0x0090, %di
	mov	$0x10, %cx
	mov	$0x00, %ax
	rep
	stosb
is_disk1:

# now we want to move to protected mode ...  我们现在要准备进入保护模式了!!!

	cli			# no interrupts allowed ! 

# first we move the system to it's rightful place
# 把整个system模块从0x10000-0x8ffff移动到0x00000-80000位置, 总共512KB, 整体的向低端地址移动了0x10000=64KB位置

	mov	$0x0000, %ax
	cld			# 'direction'=0, movs moves forward
do_move:
	mov	%ax, %es	# destination segment es:di为0x0000:0x0
	add	$0x1000, %ax
	cmp	$0x9000, %ax # 最后一段0x8000开始的64KB移动完了?
	jz	end_move
	mov	%ax, %ds	# source segment ds:si 0x1000:0x0
	sub	%di, %di
	sub	%si, %si
	mov 	$0x8000, %cx # 移动0x8000字=64KB
	rep
	movsw
	jmp	do_move

# then we load the segment descriptors

end_move:
	mov	$SETUPSEG, %ax	# right, forgot this at first. didn't work :-)
	mov	%ax, %ds		# ds指向本程序setup段
	lidt	idt_48		# load idt with 0,0 加载中断描述符表寄存器
	lgdt	gdt_48		# load gdt with whatever appropriate 加载全局描述符表寄存器

# that was painless, now we enable A20 开启A20地址线

	#call	empty_8042	# 8042 is the keyboard controller
	#mov	$0xD1, %al	# command write
	#out	%al, $0x64
	#call	empty_8042
	#mov	$0xDF, %al	# A20 on
	#out	%al, $0x60
	#call	empty_8042
	inb     $0x92, %al	# open A20 line(Fast Gate A20).
	orb     $0b00000010, %al
	outb    %al, $0x92

# well, that went ok, I hope. Now we have to reprogram the interrupts :-(
# we put them right after the intel-reserved hardware interrupts, at
# int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
# messed this up with the original PC, and they haven't been able to
# rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
# which is used for the internal hardware interrupts as well. We just
# have to reprogram the 8259's, and it isn't fun.

	mov	$0x11, %al		# initialization sequence(ICW1)
					# ICW4 needed(1),CASCADE mode,Level-triggered
	out	%al, $0x20		# send it to 8259A-1
	.word	0x00eb,0x00eb		# jmp $+2, jmp $+2
	out	%al, $0xA0		# and to 8259A-2
	.word	0x00eb,0x00eb
	mov	$0x20, %al		# start of hardware int's (0x20)(ICW2)
	out	%al, $0x21		# from 0x20-0x27
	.word	0x00eb,0x00eb
	mov	$0x28, %al		# start of hardware int's 2 (0x28)
	out	%al, $0xA1		# from 0x28-0x2F
	.word	0x00eb,0x00eb		#               IR 7654 3210
	mov	$0x04, %al		# 8259-1 is master(0000 0100) --\
	out	%al, $0x21		#				|
	.word	0x00eb,0x00eb		#			 INT	/
	mov	$0x02, %al		# 8259-2 is slave(       010 --> 2)
	out	%al, $0xA1
	.word	0x00eb,0x00eb
	mov	$0x01, %al		# 8086 mode for both
	out	%al, $0x21
	.word	0x00eb,0x00eb
	out	%al, $0xA1
	.word	0x00eb,0x00eb
	mov	$0xFF, %al		# mask off all interrupts for now
	out	%al, $0x21
	.word	0x00eb,0x00eb
	out	%al, $0xA1

# well, that certainly wasn't fun :-(. Hopefully it works, and we don't
# need no steenking BIOS anyway (except for the initial loading :-).
# The BIOS-routine wants lots of unnecessary data, and it's less
# "interesting" anyway. This is how REAL programmers do it.
#
# Well, now's the time to actually move into protected mode. To make
# things as simple as possible, we do no register set-up or anything,
# we let the gnu-compiled 32-bit programs do that. We just jump to
# absolute address 0x00000, in 32-bit protected mode.
	#mov	$0x0001, %ax	# protected mode (PE) bit
	#lmsw	%ax		# This is it! lmsw-Load Machine Status Word = cr0
	mov	%cr0, %eax	# get machine status(cr0|MSW)	
	bts	$0, %eax	# turn on the PE-bit 
	mov	%eax, %cr0	# protection enabled
	
	# 我们以及进入保护模式了,system模块也移动到了0x00000开始的地方, 8 = 0b0000_0000_0000_1:0 :00
	# 8 段描述符了, 寻址方式为请求特权级0, 使用全局描述符表中第1项, 该项代码段基址为0, 跳转到执行system的代码
	# segment-descriptor (INDEX:TI:RPL)
	.equ	sel_cs0, 0x0008 # select for code segment 0 ( 0b0000_0000_0000_1:0 :00) 
	ljmp	$sel_cs0, $0	# jmp offset 0 of code segment 0 in gdt

## 下面子程序检查键盘命令队列是否为空?
# This routine checks that the keyboard command queue is empty
# No timeout is used - if this hangs there is something wrong with
# the machine, and we probably couldn't proceed anyway.
empty_8042:
	.word	0x00eb,0x00eb # jmp $+2 jmp $+2 一点点延时
	in	$0x64, %al	# 8042 status port 读AT键盘控制器的状态寄存器
	test	$2, %al		# is input buffer full? 测试位2, 输入缓冲器满?
	jnz	empty_8042	# yes - loop
	ret
# 全局描述符表开始, 这里给出了3个描述符项
gdt:
	# placeholder项
	.word	0,0,0,0		# dummy 第一个描述符项 8字节 为空无用的 place holder

	# 系统代码段描述符, 在gdt表的偏移是0x08, 当加载cs代码段寄存器=段描述符使用这个值
	.word	0x07FF		# 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		# base address=0
	.word	0x9A00		# code read/exec 代码只读或者可执行
	.word	0x00C0		# granularity=4096, 386
	# 系统数据段描述符, 在gdt表的偏移是0x10, 当加载ds等数据段寄存器使用这个值
	.word	0x07FF		# 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		# base address=0
	.word	0x9200		# data read/write, 数据可读写
	.word	0x00C0		# granularity=4096, 386

idt_48:
	.word	0			# idt limit=0
	.word	0,0			# idt base=0L

gdt_48:
	.word	0x800			# gdt limit=2048, 256 GDT entries, 2KB / 8 = 256
	.word   512+gdt, 0x9	# gdt base = 0X9xxxx = 0x90200 + gdt = 0x0009 << 16 + 0x200 + gdt
	# 512+gdt is the real gdt after setup is moved to 0x9020 * 0x10, 4个字节组成的liner adr,高16位0x9,低16位512+gdt
print_hex:
	mov $4,%cx
	mov %ax,%dx

print_digit:
	rol $4,%dx	#循环以使低4位用上，高4位移至低4位
	mov $0xe0f,%ax #ah ＝ 请求的功能值，al = 半个字节的掩码
	and %dl,%al
	add $0x30,%al
	cmp $0x3a,%al
	jl outp
	add $0x07,%al

outp:
	int $0x10
	loop print_digit
	ret
#打印回车换行
print_nl:
	mov $0xe0d,%ax
	int $0x10
	mov $0xa,%al
	int $0x10
	ret

msg2:
	.byte 13,10
	.ascii "Now we are in setup ..."
	.byte 13,10,13,10
cur:
	.ascii "Cursor POS:"
mem:
	.ascii "Memory SIZE:"
cyl:
	.ascii "KB"
	.byte 13,10,13,10
	.ascii "HD Info"
	.byte 13,10
	.ascii "Cylinders:"
head:
	.ascii "Headers:"
sect:
	.ascii "Secotrs:"
.text
endtext:
.data
enddata:
.bss
endbss:
