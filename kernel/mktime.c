/*
 *  linux/kernel/mktime.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <time.h>

/*
 * This isn't the library routine, it is only used in the kernel.
 * as such, we don't care about years<1970 etc, but assume everything
 * is ok. Similarly, TZ etc is happily ignored. We just do everything
 * as easily as possible. Let's find something public for the library
 * routines (although I think minix times is public).
 */
/*
 * PS. I hate whoever though up the year 1970 - couldn't they have gotten
 * a leap-year instead? I also hate Gregorius, pope or no. I'm grumpy.
 * 
 * 这不是库函数， 仅供内核使用, 我恨设置1970年开始的人, 我恨格里高利历
 */
#define MINUTE 60		// 一分钟的秒数
#define HOUR (60*MINUTE) // 1小时的秒数
#define DAY (24*HOUR)	// 1天的秒数
#define YEAR (365*DAY)	// 1年的秒数

/* interestingly, we assume leap-years 考虑闰年 = y 能被4整除不能被100整除, 或者 y 能被400整除*/
// 以年为界限, 定义了每个月开始时的秒数时间
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

// 计算从1970-01-01 00:00:00 起到开机经过的秒数, 作为开机时间, init/main.c 赋值了，信息取自CMOS
long kernel_mktime(struct tm * tm)
{
	long res;
	int year;
	if (tm->tm_year >= 70)
	  year = tm->tm_year - 70;
	else
	  year = tm->tm_year + 100 -70; /* Y2K bug fix by hellotigercn 20110803  2位表示方式, 2000千年虫问题*/
/* magic offsets (y+1) needed to get leapyears right. 70-73为第一个闰年周期*/
	res = YEAR*year + DAY*((year+1)/4);	// 这些年经过的秒数 + 每个闰年多一天的秒数时间
	res += month[tm->tm_mon];			// 当年到当月的秒数
/* and (y+2) here. If it wasn't a leap-year, we have to adjust */
// month数组2月份的天数默认包含闰年时天数即多算了一天, 不是闰年(70-72年， y+ 2)并且当前月份 >= 2月
	if (tm->tm_mon>1 && ((year+2)%4))
		res -= DAY;
	res += DAY*(tm->tm_mday-1);			// 本月过去的天数的秒数时间
	res += HOUR*tm->tm_hour;			// 当天过去的小时数的秒数时间
	res += MINUTE*tm->tm_min;			// 1小时内过去的分钟数的秒数时间
	res += tm->tm_sec;					// 1分钟已经过去的秒数
	return res;							// 1970年以来经过的秒数时间
}
