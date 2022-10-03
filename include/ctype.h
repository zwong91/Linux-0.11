#ifndef _CTYPE_H
#define _CTYPE_H

#define _U	0x01	/* upper [A-Z]*/
#define _L	0x02	/* lower [a-z]*/
#define _D	0x04	/* digit [0-9]*/
#define _C	0x08	/* cntrl 控制字符*/
#define _P	0x10	/* punct 标点符号*/
#define _S	0x20	/* white space (space/lf/tab) 空白字符*/
#define _X	0x40	/* hex digit 十六进制*/
#define _SP	0x80	/* hard space (0x20) 空格字符*/

extern unsigned char _ctype[];  // 字符特性数组, 定义各个字符对应上面的属性
extern char _ctmp;  // 临时字符变量

// 字符类型判断和转换
#define isalnum(c) ((_ctype+1)[c]&(_U|_L|_D))
#define isalpha(c) ((_ctype+1)[c]&(_U|_L))
#define iscntrl(c) ((_ctype+1)[c]&(_C))
#define isdigit(c) ((_ctype+1)[c]&(_D))
#define isgraph(c) ((_ctype+1)[c]&(_P|_U|_L|_D))
#define islower(c) ((_ctype+1)[c]&(_L))
#define isprint(c) ((_ctype+1)[c]&(_P|_U|_L|_D|_SP))
#define ispunct(c) ((_ctype+1)[c]&(_P))
#define isspace(c) ((_ctype+1)[c]&(_S))
#define isupper(c) ((_ctype+1)[c]&(_U))
#define isxdigit(c) ((_ctype+1)[c]&(_D|_X))

// 127 = 7f = 0111_1111
#define isascii(c) (((unsigned) c)<=0x7f)
#define toascii(c) (((unsigned) c)&0x7f)

// note: _ctmp参数只能使用一次, 这个公共临时变量unsafe, Linux 2.2.X后使用两个函数替代
#define tolower(c) (_ctmp=c,isupper(_ctmp)?_ctmp-('A'-'a'):_ctmp)
#define toupper(c) (_ctmp=c,islower(_ctmp)?_ctmp-('a'-'A'):_ctmp)

#endif
