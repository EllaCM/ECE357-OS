.text
    mov $1, %rax;         // syscall number sys_write
    mov $1, %rdi;         // first argument, fd = 1 (stdout)
    mov $message, %rsi;   // second argument, buf =  message
    mov $11, %rdx;        // third argument, count = 11
    syscall
    mov %rax, %rdi;       // error_code = 0
    mov $60, %rax;        // sys_exit
    syscall

.data
message:
    .ascii "hello world"

