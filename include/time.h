#ifndef _TIME_H
#define _TIME_H

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#define CLOCKS_PER_SEC 100

typedef long clock_t;

struct tm {
	int tm_sec;		//秒数 [0,59]
	int tm_min;		//分钟数[0,59]
	int tm_hour;	//小时数[0,23]
	int tm_mday;	//月的天数[0,31]
	int tm_mon;		//年的月数[0,11]
	int tm_year;	//1900开始的年数
	int tm_wday;	//星期中的某天[0,6], 0 = sunday
	int tm_yday;	//年中的某天[0,365]
	int tm_isdst;	//夏令时标记
};

clock_t clock(void);
time_t time(time_t * tp);
double difftime(time_t time2, time_t time1);
time_t mktime(struct tm * tp);

char * asctime(const struct tm * tp);
char * ctime(const time_t * tp);
struct tm * gmtime(const time_t *tp);
struct tm *localtime(const time_t * tp);
size_t strftime(char * s, size_t smax, const char * fmt, const struct tm * tp);
void tzset(void);

#endif
