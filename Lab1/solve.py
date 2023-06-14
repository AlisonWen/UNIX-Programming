#!/usr/bin/env python3
from pwn import *
import pow as pw 
import base64
import hashlib
import time

r = remote('up23.zoolab.org', 10363)
pw.solve_pow(r)
total_num = int(0)

for i in range(7):
	buf = r.recvline().decode()
	print(i,"buf", buf)
	if i == 3:
		total_num = int(buf.split(" ")[3])
		print("total num", total_num)

for cnt in range(total_num):
	buf = r.recvuntil(b'?').decode().split()
	num1 = buf[2]
	op = buf[3]
	num2 = buf[4]
	exp = num1 + " " + op + " " + num2
	ans = eval(exp)
	print(num1, op, num2, "=", ans)
	print("----------------------")
	encode_ans = (ans.to_bytes(ans.bit_length(), 'big'))
	encode_ans = encode_ans.lstrip(b'\x00')
	little_endian = encode_ans[::-1]
	formal_ans = base64.b64encode(little_endian)
	
	r.sendline(formal_ans)
result = r.recvline().decode()
print(result)
