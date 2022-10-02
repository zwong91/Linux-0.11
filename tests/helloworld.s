.text
_entry:
    movl $4, %eax
    movl $1, %ebx
    movl $message, %ecx
    movl $12, %edx
    int $0x80
    movl $1, %eax
    int $0x80
message:
    .ascii  "Hello, World!\n"