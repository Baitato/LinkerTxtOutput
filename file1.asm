.section .text
.global _start

_start:
    mov $1, %eax    # System call number for write
    mov $1, %edi    # File descriptor for stdout
    mov $message, %rsi    # Address of the message
    mov $12, %edx    # Length of the message
    syscall    # Invoke the system call

    mov $60, %eax    # System call number for exit
    xor %edi, %edi    # Exit status
    syscall    # Invoke the system call

.section .data
message:
    .string "Hello World"
