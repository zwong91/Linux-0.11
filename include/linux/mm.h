#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096      // 4KB 一页

// 从mem_map[]取空闲页面, 返回页面地址
extern unsigned long get_free_page(void);
// 线性地址映射一页内存, 在页目录表和页表设置该页面信息, 返回该页面物理地址
extern unsigned long put_page(unsigned long page,unsigned long address);
// 释放物理地址addr开始的一页内存, 修改mem_map[]中引用次数
extern void free_page(unsigned long addr);

#endif
