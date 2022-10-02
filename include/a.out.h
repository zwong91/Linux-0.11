#ifndef _A_OUT_H
#define _A_OUT_H

#define __GNU_EXEC_MACROS__

// Assembly out格式, 7 section组成
// exec header 执行头部
// text segment 只读指令和数据
// data segment 可读写初始化后的数据
// text relocations 链接程序ld使用定位代码段位置
// data relocations 链接程序ld使用定位数据段位置
// symbols table 变量和函数与地址交叉引用映射
// string table 符号名对应的字符串
struct exec {
  unsigned long a_magic;	/* Use macros N_MAGIC, etc for access 可执行文件魔数 */
  unsigned a_text;		/* length of text, in bytes 代码段长度,字节数*/
  unsigned a_data;		/* length of data, in bytes 数据段长度,字节数*/
  unsigned a_bss;		/* length of uninitialized data area for file, in bytes 缓冲区长度,字节数, break(brk)*/
  unsigned a_syms;		/* length of symbol table data in file, in bytes 符号表长度,字节数*/
  unsigned a_entry;		/* start address 执行入口地址*/
  unsigned a_trsize;		/* length of relocation info for text, in bytes 代码段重定位长度,字节数*/
  unsigned a_drsize;		/* length of relocation info for data, in bytes 数据段重定位长度,字节数*/
};

// 取自上面exec结构的魔数
#ifndef N_MAGIC
#define N_MAGIC(exec) ((exec).a_magic)
#endif
// Old Magic PDP-11的跳转指令0407=0x107, 跳转随后的7个字节代码开始处
#ifndef OMAGIC
/* Code indicating object file or impure executable.  */
#define OMAGIC 0407
/* Code indicating pure executable. 纯可执行文件代号 New Magic 1975后使用, 0410 = 0x108 */
#define NMAGIC 0410
/* Code indicating demand-paged executable. 为分页处理的可执行文件, 头结构占文件开始1K大小, 0413-0x10b */
#define ZMAGIC 0413
#endif /* not OMAGIC */

// 判断魔数字段的正确性, 不能识别魔数返回true
#ifndef N_BADMAG
#define N_BADMAG(x)					\
 (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC		\
  && N_MAGIC(x) != ZMAGIC)
#endif

#define _N_BADMAG(x)					\
 (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC		\
  && N_MAGIC(x) != ZMAGIC)

// 可执行文件头结构尾到4096 字节内容长度
#define _N_HDROFF(x) (SEGMENT_SIZE - sizeof (struct exec))

// 下列的宏操作.o文件.out文件内容
// 文件为ZMAGIC类型可执行文件, 代码部分从可执行文件1024字节偏移处开始
// 文件为OMAGIC类型模块文件, 代码部分紧随可执行头exec头结构后32字节偏移处开始
#ifndef N_TXTOFF
#define N_TXTOFF(x) \
 (N_MAGIC(x) == ZMAGIC ? _N_HDROFF((x)) + sizeof (struct exec) : sizeof (struct exec))
#endif

// 数据部分起始偏移值, 从代码部分尾开始
#ifndef N_DATOFF
#define N_DATOFF(x) (N_TXTOFF(x) + (x).a_text)
#endif

// 代码重定位信息从数据部分尾偏移开始
#ifndef N_TRELOFF
#define N_TRELOFF(x) (N_DATOFF(x) + (x).a_data)
#endif

// 数据重定位信息从代码重定位信息尾部偏移开始
#ifndef N_DRELOFF
#define N_DRELOFF(x) (N_TRELOFF(x) + (x).a_trsize)
#endif

// 符号表偏移值从上面数据段重定位末端开始
#ifndef N_SYMOFF
#define N_SYMOFF(x) (N_DRELOFF(x) + (x).a_drsize)
#endif

// 字符串信息偏移值在紧随符号表之后
#ifndef N_STROFF
#define N_STROFF(x) (N_SYMOFF(x) + (x).a_syms)
#endif


// 下面对可执行文件被加载进内存后的位置情况操作

/* Address of text segment in memory after it is loaded.  */
// 代码段加载到内存0地址开始执行
#ifndef N_TXTADDR
#define N_TXTADDR(x) 0
#endif

// 数据段加载到内存后的地址, 对于下面没列出机器cpu得自定义
/* Address of data segment in memory after it is loaded.
   Note that it is up to you to define SEGMENT_SIZE
   on machines not listed here.  */
#if defined(vax) || defined(hp300) || defined(pyr)
#define SEGMENT_SIZE PAGE_SIZE
#endif
#ifdef	hp300
#define	PAGE_SIZE	4096
#endif
#ifdef	sony
#define	SEGMENT_SIZE	0x2000
#endif	/* Sony.  */
#ifdef is68k
#define SEGMENT_SIZE 0x20000
#endif
#if defined(m68k) && defined(PORTAR)
#define PAGE_SIZE 0x400
#define SEGMENT_SIZE PAGE_SIZE
#endif

// Linux 内核内存页定义为4KB, 段大小定义为1KB, 没有使用hp300/sony/is/motorola等定义
#define PAGE_SIZE 4096
#define SEGMENT_SIZE 1024

// 以段大小为分界进位
#define _N_SEGMENT_ROUND(x) (((x) + SEGMENT_SIZE - 1) & ~(SEGMENT_SIZE - 1))
// 代码段尾部地址
#define _N_TXTENDADDR(x) (N_TXTADDR(x)+(x).a_text)

// 数据段开始地址, 如果文件为OMAGIC类型，数据段紧随代码段后面, ZMAGIC文件数据段地址以代码段后1KB边界对齐
#ifndef N_DATADDR
#define N_DATADDR(x) \
    (N_MAGIC(x)==OMAGIC? (_N_TXTENDADDR(x)) \
     : (_N_SEGMENT_ROUND (_N_TXTENDADDR(x))))
#endif
// bss 缓冲区段加载到内存的地址, 紧随数据段后面
/* Address of bss segment in memory after it is loaded.  */
#ifndef N_BSSADDR
#define N_BSSADDR(x) (N_DATADDR(x) + (x).a_data)
#endif


// a.out 可执行文件中符号表项结构, 符号将名称映射为地址
#ifndef N_NLIST_DECLARED
struct nlist {
  union {
    char *n_name;
    struct nlist *n_next;
    long n_strx;
  } n_un;
  unsigned char n_type; // type 分为3个字段
  char n_other;
  short n_desc;
  unsigned long n_value;
};
#endif

// 下面定义nlist结构的n_type字段值常量
#ifndef N_UNDF
#define N_UNDF 0
#endif
#ifndef N_ABS
#define N_ABS 2
#endif
#ifndef N_TEXT
#define N_TEXT 4
#endif
#ifndef N_DATA
#define N_DATA 6
#endif
#ifndef N_BSS
#define N_BSS 8
#endif
#ifndef N_COMM
#define N_COMM 18
#endif
#ifndef N_FN
#define N_FN 15
#endif

// 以下三常量时nlist结构的n_type字段掩码 八进制形式
#ifndef N_EXT
#define N_EXT 1         /* 0x01 = 0b0000_0001 符号是否时外部的,全局的*/
#endif
#ifndef N_TYPE
#define N_TYPE 036      /* 0x1e = 0b0001_1110 符号额类型位 */
#endif
#ifndef N_STAB
#define N_STAB 0340     /* 0xe0 = 0b1110_0000  STAB Symbol table types 符号调试器*/
#endif

/* The following type indicates the definition of a symbol as being
   an indirect reference to another symbol.  The other symbol
   appears as an undefined reference, immediately following this symbol.

   Indirection is asymmetrical.  The other symbol's value will be used
   to satisfy requests for the indirect symbol, but not vice versa.
   If the other symbol does not have a definition, libraries will
   be searched to find a definition. 符号的间接引用 */
#define N_INDR 0xa

/* The following symbols refer to set elements.
   All the N_SET[ATDB] symbols with the same name form one set.
   Space is allocated for the set in the text section, and each set
   element's value is stored into one word of the space.
   The first word of the space is the length of the set (number of elements).

   The address of the set is made into an N_SETV symbol
   whose name is the same as the name of the set.
   This symbol acts like a N_DATA global symbol
   in that it can satisfy undefined external references.  */

/* These appear as input to LD, in a .o file.  */
// 以下符号在.o文件中作为链接程序ld的输入
#define	N_SETA	0x14		/* Absolute set element symbol */
#define	N_SETT	0x16		/* Text set element symbol */
#define	N_SETD	0x18		/* Data set element symbol */
#define	N_SETB	0x1A		/* Bss set element symbol */

/* This is output from LD.  */
// 下面时ld的输出
#define N_SETV	0x1C		/* Pointer to set vector in data area.  */

#ifndef N_RELOCATION_INFO_DECLARED

/* This structure describes a single relocation to be performed.
   The text-relocation section of the file is a vector of these structures,
   all of which apply to the text section.
   Likewise, the data-relocation section applies to the data section.  */

// a.out 可执行文件中代码和数据重定位信息结构
struct relocation_info
{
  /* Address (within segment) to be relocated.  */
  int r_address;  // 段内需要重定位的地址
  /* The meaning of r_symbolnum depends on r_extern.  */
  unsigned int r_symbolnum:24; // 指定符号表中一个符号或者一个段, 与r_extern关联
  /* Nonzero means value is a pc-relative offset
     and it should be relocated for changes in its own address
     as well as for changes in the symbol or section specified.  */
  unsigned int r_pcrel:1;  // PC 相关标识
  /* Length (as exponent of 2) of the field to be relocated.
     Thus, a value of 2 indicates 1<<2 bytes.  */
  unsigned int r_length:2; // 要被重定位字段长度 2的次方
  /* 1 => relocate with value of symbol.
          r_symbolnum is the index of the symbol
	  in file's the symbol table.
     0 => relocate with the address of a segment.
          r_symbolnum is N_TEXT, N_DATA, N_BSS or N_ABS
	  (the N_EXT bit may be set also, but signifies nothing).  */
  unsigned int r_extern:1; // 1 - 以符号的值重定位, 0 - 以段的地址重定位
  /* Four bits that aren't used, but when writing an object file
     it is desirable to clear them.  */
  unsigned int r_pad:4;  // 置0 padding
};
#endif /* no N_RELOCATION_INFO_DECLARED.  */


#endif /* __A_OUT_GNU_H__ */
