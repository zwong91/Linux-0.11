/*
 * malloc.c --- a general purpose kernel memory allocator for Linux.
 * 
 * Written by Theodore Ts'o (tytso@mit.edu), 11/29/91
 *
 * This routine is written to be as fast as possible, so that it
 * can be called from the interrupt level.
 *
 * Limitations: maximum size of memory we can allocate using this routine
 *	is 4k, the size of a page in Linux.
 *
 * The general game plan is that each page (called a bucket) will only hold
 * objects of a given size.  When all of the object on a page are released,
 * the page can be returned to the general free pool.  When malloc() is
 * called, it looks for the smallest bucket size which will fulfill its
 * request, and allocate a piece of memory from that bucket pool.
 *
 * Each bucket has as its control block a bucket descriptor which keeps 
 * track of how many objects are in use on that page, and the free list
 * for that page.  Like the buckets themselves, bucket descriptors are
 * stored on pages requested from get_free_page().  However, unlike buckets,
 * pages devoted to bucket descriptor pages are never released back to the
 * system.  Fortunately, a system should probably only need 1 or 2 bucket
 * descriptor pages, since a page can hold 256 bucket descriptors (which
 * corresponds to 1 megabyte worth of bucket pages.)  If the kernel is using 
 * that much allocated memory, it's probably doing something wrong.  :-)
 *
 * Note: malloc() and free() both call get_free_page() and free_page()
 *	in sections of code where interrupts are turned off, to allow
 *	malloc() and free() to be safely called from an interrupt routine.
 *	(We will probably need this functionality when networking code,
 *	particularily things like NFS, is added to Linux.)  However, this
 *	presumes that get_free_page() and free_page() are interrupt-level
 *	safe, which they may not be once paging is added.  If this is the
 *	case, we will need to modify malloc() to keep a few unused pages
 *	"pre-allocated" so that it can safely draw upon those pages if
 * 	it is called from an interrupt routine.
 *
 * 	Another concern is that get_free_page() should not sleep; if it 
 *	does, the code is carefully ordered so as to avoid any race 
 *	conditions.  The catch is that if malloc() is called re-entrantly, 
 *	there is a chance that unecessary pages will be grabbed from the 
 *	system.  Except for the pages for the bucket descriptor page, the 
 *	extra pages will eventually get released back to the system, though,
 *	so it isn't all that bad.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

// kmalloc/kfree 内核通用内存块分配动态管理, 开发库中libc调整进程数据段末端位置 brk, 直到cannary stack -8 环境变量参数
// bucket 桶原理, 建立一页的空闲链表, 对不同请求内存块大小使用存储桶目录分别处理
struct bucket_desc {	/* 16 bytes */
	void			*page;			// 该桶描述符对应的内存页指针
	struct bucket_desc	*next;		// 下一个桶描述符指针
	void			*freeptr;		// 指向该桶的空闲内存地址指针
	unsigned short		refcnt;		// 引用计数
	unsigned short		bucket_size;	// 该桶描述符的大小
};

struct _bucket_dir {	/* 8 bytes */
	int			size;			// 桶的大小
	struct bucket_desc	*chain;	// 链表指针
};

/*
 * The following is the where we store a pointer to the first bucket
 * descriptor for a given size.  
 *
 * If it turns out that the Linux kernel allocates a lot of objects of a
 * specific size, then we may want to add that specific size to this list,
 * since that will allow the memory to be allocated more efficiently.
 * However, since an entire page must be dedicated to each specific size
 * on this list, some amount of temperance must be exercised here.
 *
 * Note that this list *must* be kept in order.
 */
struct _bucket_dir bucket_dir[] = {
	{ 16,	(struct bucket_desc *) 0},		// 16字节长度的内存块
	{ 32,	(struct bucket_desc *) 0},		// 32字节长度的内存块
	{ 64,	(struct bucket_desc *) 0},		// 64字节长度的内存块
	{ 128,	(struct bucket_desc *) 0},		// 128字节长度的内存块
	{ 256,	(struct bucket_desc *) 0},		// 256字节长度的内存块
	{ 512,	(struct bucket_desc *) 0},
	{ 1024,	(struct bucket_desc *) 0},
	{ 2048, (struct bucket_desc *) 0},
	{ 4096, (struct bucket_desc *) 0},		// 4KB长度的内存块
	{ 0,    (struct bucket_desc *) 0}};   /* End of list marker */

/*
 * This contains a linked list of free bucket descriptor blocks
 */
struct bucket_desc *free_bucket_desc = (struct bucket_desc *) 0;

/*
 * This routine initializes a bucket description page. 创建初始化空闲桶描述符链表
 */
static inline void init_bucket_desc()
{
	struct bucket_desc *bdesc, *first;
	int	i;
	
	// 申请一页内存存放bucket desc
	first = bdesc = (struct bucket_desc *) get_free_page();
	if (!bdesc)
		panic("Out of memory in init_bucket_desc()");
	// 计算一页内存可存放的bucket desc数量, 建立单链表
	for (i = PAGE_SIZE/sizeof(struct bucket_desc); i > 1; i--) {
		bdesc->next = bdesc+1;
		bdesc++;
	}
	/*
	 * This is done last, to avoid race conditions in case 
	 * get_free_page() sleeps and this routine gets called again....
	 */
	// 将空闲bucket desc链表最后置为NULL, 设置头指针
	bdesc->next = free_bucket_desc;
	free_bucket_desc = first;
}

void *malloc(unsigned int len)
{
	struct _bucket_dir	*bdir;
	struct bucket_desc	*bdesc;
	void			*retval;

	/*
	 * First we search the bucket_dir to find the right bucket change
	 * for this request. 搜索目录bucket_dir找到合适请求项内存块大小的目录
	 */
	for (bdir = bucket_dir; bdir->size; bdir++)
		if (bdir->size >= len)
			break;
	// 请求内存块超过一页
	if (!bdir->size) {
		printk("malloc called with impossibly large argument (%d)\n",
			len);
		panic("malloc: bad arg");
	}
	/*
	 * Now we search for a bucket descriptor which has free space 再对应的目录项找空闲的桶描述符, 不为NULL找到
	 */
	cli();	/* Avoid race conditions */
	for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) 
		if (bdesc->freeptr)
			break;
	/*
	 * If we didn't find a bucket with free space, then we'll 
	 * allocate a new one. 没找到空闲桶描述符, 新建一个页 free bucket desc
	 */
	if (!bdesc) {
		char	*cp;
		int		i;
		// free bucket 用完了或者第一次调用
		if (!free_bucket_desc)	
			init_bucket_desc();
		// 将该桶描述符空闲指针freeptr赋值给用户, 然后调整freeptr指向下一个空闲 bucket desc
		bdesc = free_bucket_desc;
		free_bucket_desc = bdesc->next;
		// 新bucket desc 对象的引用计数为0
		bdesc->refcnt = 0;
		// 新bucket desc 对象大小等于对应目录指定对象的长度
		bdesc->bucket_size = bdir->size;
		// 申请一页让page指向该内存页, 同时空闲指针freeptr也指向页开始位置, 此时全为空闲
		bdesc->page = bdesc->freeptr = (void *) (cp = (char *) get_free_page());
		if (!cp)
			panic("Out of memory in kernel malloc()");
		/* Set up the chain of free objects 在申请的页空闲内存上建立空闲对象链表*/
		for (i=PAGE_SIZE/bdir->size; i > 1; i--) {
			*((char **) cp) = cp + bdir->size;	// 每个对象的开始4字节设置为指向下一个对象指针
			cp += bdir->size;
		}
		// 最后一个对象存放NULL指针
		*((char **) cp) = 0;
		bdesc->next = bdir->chain; /* OK, link it in! */
		bdir->chain = bdesc;	// 将该新bucket desc 链接到chain头指针
	}
	// 返回bucket desc对应页的当前空闲指针, 调整该空闲指针指向下一个空闲对象
	retval = (void *) bdesc->freeptr;
	bdesc->freeptr = *((void **) retval);	// 这里很骚啊, 取值就是对象blockSize尾部 = next位置
	bdesc->refcnt++;
	sti();	/* OK, we're safe again */
	return(retval);
}

/*
 * Here is the free routine.  If you know the size of the object that you
 * are freeing, then free_s() will use that information to speed up the
 * search for the bucket descriptor.
 * 
 * We will #define a macro so that "free(x)" is becomes "free_s(x, 0)"
 */

// 隐式空闲链表 free 有merge作用, 减少内存碎片
void free_s(void *obj, int size)
{
	void		*page;
	struct _bucket_dir	*bdir;
	struct bucket_desc	*bdesc, *prev;
	bdesc = prev = 0;
	/* Calculate what page this object lives in 计算该内存块地址对应的页地址 取模*/
	page = (void *)  ((unsigned long) obj & 0xfffff000);
	/* Now search the buckets looking for that page 搜索目录找到对应页面描述符*/
	for (bdir = bucket_dir; bdir->size; bdir++) {
		prev = 0;
		/* If size is zero then this conditional is always false */
		if (bdir->size < size)
			continue;
		for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) {
			if (bdesc->page == page) 
				goto found;
			prev = bdesc;
		}
	}
	panic("Bad address passed to kernel free_s()");
found:
	cli(); /* To avoid race conditions */
	// 将释放的内存块链入freeptr头
	*((void **)obj) = bdesc->freeptr;
	bdesc->freeptr = obj;
	bdesc->refcnt--;
	// 该描述符对应的页全部空出了, 释放内存页并将该bucket desc描述符放回空闲bucket链表头
	if (bdesc->refcnt == 0) {
		/*
		 * We need to make sure that prev is still accurate.  It
		 * may not be, if someone rudely interrupted us....
		 */
		if ((prev && (prev->next != bdesc)) ||
		    (!prev && (bdir->chain != bdesc)))
			for (prev = bdir->chain; prev; prev = prev->next)
				if (prev->next == bdesc)
					break;
		if (prev)
			prev->next = bdesc->next;
		else {
			if (bdir->chain != bdesc)
				panic("malloc bucket chains corrupted");
			bdir->chain = bdesc->next;
		}
		free_page((unsigned long) bdesc->page);
		bdesc->next = free_bucket_desc;
		free_bucket_desc = bdesc;
	}
	sti();
	return;
}

