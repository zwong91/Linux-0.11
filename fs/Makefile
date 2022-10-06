include ../Makefile.header

LDFLAGS += -r
CFLAGS	+= -I../include
CPP	+= -I../include

.c.s:
	@$(CC) $(CFLAGS) \
	-S -o $*.s $<
.c.o:
	@$(CC) $(CFLAGS) \
	-c -o $*.o $<
.s.o:
	$(AS) -o $*.o $<

# MINIX 文件系统
## 0x0000-0x03ff = 1KB 第0个块引导块内容
## 0x0400-0x07ff = 1KB 第1个块超级块内容
## 0x0800-0x0bff = 1KB 第2个块inode索引位图信息
## 0x0c00-0x0x0ff = 1KB 第3个块 逻辑块位图
## 0x1000-0x1fff = 4KB 第4-7块inode 节点结构信息
## 0x2000-0x23ff = 1B 第8个块 1号root inode数据
## ...开始存块数据

OBJS=	open.o read_write.o inode.o file_table.o buffer.o super.o \
	block_dev.o char_dev.o file_dev.o stat.o exec.o pipe.o namei.o \
	bitmap.o fcntl.o ioctl.o truncate.o

fs.o: $(OBJS)
	@$(LD) $(LDFLAGS) -o fs.o $(OBJS)

clean:
	@rm -f core *.o *.a tmp_make
	@for i in *.c;do rm -f `basename $$i .c`.s;done

dep:
	@sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	@(for i in *.c;do $(CPP) -M $$i;done) >> tmp_make
	@cp tmp_make Makefile

### Dependencies:
bitmap.o: bitmap.c ../include/string.h ../include/linux/sched.h \
  ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
  ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h
block_dev.o: block_dev.c ../include/errno.h ../include/linux/sched.h \
  ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
  ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
  ../include/asm/segment.h ../include/asm/system.h
buffer.o: buffer.c ../include/stdarg.h ../include/linux/config.h \
  ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
  ../include/sys/types.h ../include/linux/mm.h ../include/signal.h \
  ../include/linux/kernel.h ../include/asm/system.h ../include/asm/io.h
char_dev.o: char_dev.c ../include/errno.h ../include/sys/types.h \
  ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
  ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
  ../include/asm/segment.h ../include/asm/io.h
exec.o: exec.c ../include/errno.h ../include/string.h \
  ../include/sys/stat.h ../include/sys/types.h ../include/a.out.h \
  ../include/linux/fs.h ../include/linux/sched.h ../include/linux/head.h \
  ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
  ../include/asm/segment.h
fcntl.o: fcntl.c ../include/string.h ../include/errno.h \
  ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
  ../include/sys/types.h ../include/linux/mm.h ../include/signal.h \
  ../include/linux/kernel.h ../include/asm/segment.h ../include/fcntl.h \
  ../include/sys/stat.h
file_dev.o: file_dev.c ../include/errno.h ../include/fcntl.h \
  ../include/sys/types.h ../include/linux/sched.h ../include/linux/head.h \
  ../include/linux/fs.h ../include/linux/mm.h ../include/signal.h \
  ../include/linux/kernel.h ../include/asm/segment.h
file_table.o: file_table.c ../include/linux/fs.h ../include/sys/types.h
inode.o: inode.c ../include/string.h ../include/sys/stat.h \
  ../include/sys/types.h ../include/linux/sched.h ../include/linux/head.h \
  ../include/linux/fs.h ../include/linux/mm.h ../include/signal.h \
  ../include/linux/kernel.h ../include/asm/system.h
ioctl.o: ioctl.c ../include/string.h ../include/errno.h \
  ../include/sys/stat.h ../include/sys/types.h ../include/linux/sched.h \
  ../include/linux/head.h ../include/linux/fs.h ../include/linux/mm.h \
  ../include/signal.h
namei.o: namei.c ../include/linux/sched.h ../include/linux/head.h \
  ../include/linux/fs.h ../include/sys/types.h ../include/linux/mm.h \
  ../include/signal.h ../include/linux/kernel.h ../include/asm/segment.h \
  ../include/string.h ../include/fcntl.h ../include/errno.h \
  ../include/const.h ../include/sys/stat.h
open.o: open.c ../include/string.h ../include/errno.h ../include/fcntl.h \
  ../include/sys/types.h ../include/utime.h ../include/sys/stat.h \
  ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
  ../include/linux/mm.h ../include/signal.h ../include/linux/tty.h \
  ../include/termios.h ../include/linux/kernel.h ../include/asm/segment.h
pipe.o: pipe.c ../include/signal.h ../include/sys/types.h \
  ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
  ../include/linux/mm.h ../include/asm/segment.h
read_write.o: read_write.c ../include/sys/stat.h ../include/sys/types.h \
  ../include/errno.h ../include/linux/kernel.h ../include/linux/sched.h \
  ../include/linux/head.h ../include/linux/fs.h ../include/linux/mm.h \
  ../include/signal.h ../include/asm/segment.h
stat.o: stat.c ../include/errno.h ../include/sys/stat.h \
  ../include/sys/types.h ../include/linux/fs.h ../include/linux/sched.h \
  ../include/linux/head.h ../include/linux/mm.h ../include/signal.h \
  ../include/linux/kernel.h ../include/asm/segment.h
super.o: super.c ../include/linux/config.h ../include/linux/sched.h \
  ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
  ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
  ../include/asm/system.h ../include/errno.h ../include/sys/stat.h
truncate.o: truncate.c ../include/linux/sched.h ../include/linux/head.h \
  ../include/linux/fs.h ../include/sys/types.h ../include/linux/mm.h \
  ../include/signal.h ../include/sys/stat.h
