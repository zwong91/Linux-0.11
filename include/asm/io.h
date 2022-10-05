// outb/inb 与outb_p/inb_p 区别仅仅后面jmp 进行了一点点延迟

// 外设端口写指定字节
#define outb(value,port) \
__asm__ ("outb %%al,%%dx"::"a" (value),"d" (port))

// 读外设端口返回字节数
#define inb(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al":"=a" (_v):"d" (port)); \
_v; \
})

// 带延迟外设端口写指定字节
#define outb_p(value,port) \
__asm__ ("outb %%al,%%dx\n" \
		"\tjmp 1f\n" \
		"1:\tjmp 1f\n" \
		"1:"::"a" (value),"d" (port))

// 带延迟读外设端口返回字节数
#define inb_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" (port)); \
_v; \
})
