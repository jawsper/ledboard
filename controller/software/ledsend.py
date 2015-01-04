import socket
import struct
import time
import math
import sys

MAX_LEN = 1400
PANELS_X = 1
PANELS_Y = 1
PANEL_WIDTH = 32
PANEL_HEIGHT = 16
WIDTH = PANEL_WIDTH * PANELS_X
HEIGHT  = PANEL_HEIGHT * PANELS_Y
SIZE = WIDTH * HEIGHT
TARGET = ('ledboard', 1337)
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def compress(input):
	# return input
	out = [0x87] # 7 pixels per byte
	val = 0
	n = 0
	for i in range(len(input)):
		if input[i] > 0:
			val |= 1 << n
		n += 1
		if n == 7:
			if val > 0x7f:
				print(val)
				sys.exit(val)
			out.append(val)
			val = 0
			n = 0
	if n > 0:
		out.append(val)
	# new_len = math.ceil(len(input)/7)
	# for i in range(0, new_len * 7, 7):
	# 	val = 0
	# 	for j in range(7):
	# 		if i+j >= len(input):
	# 			break
	# 		if input[i+j] > 0:
	# 			val |= 1 << j
	# 	# print(val)
	# 	# sys.exit(0)
	# 	out += bytes([val])
	# 	# i += 7
	return bytes(out)


def send(data):
	# print(data)
	# sys.exit(1)
	# print(len(data))
	data = b'\x80' + compress(data)
	total_len = len(data)
	print(total_len)
	while(len(data) > 0):
		buff = data[:MAX_LEN]
		data = data[MAX_LEN:]
		send_packet(buff)
		if total_len > MAX_LEN:
			print('zzz')
			time.sleep(0.03)
	time.sleep(0.05)

def send_packet(packet):
	s.sendto(packet, TARGET)

PANELS = (
	(2, 0), (2, 1), (2, 2),
	(1, 2), (1, 1), (1, 0),
	(0, 0), (0, 1), (0, 2)
)

def image2d(image1d):
	image = []
	for x in range(WIDTH):
		pass

def make_image(image):
	data = []
	for panel in PANELS:
		x_offset = panel[0] * 32
		y_offset = panel[1] * 16
		for y in range(y_offset, y_offset + 16):
			for x in range(x_offset, x_offset + 32):
				data.append(image[x + y * WIDTH])
	return bytes(data)

def print_image(image):
	i = 0
	for y in range(HEIGHT):
		for x in range(WIDTH):
			print('o' if image[x + y * WIDTH] > 0 else 'x', end='')
		print()


count = 32*16
try:
	# send(b'\x7f' * SIZE)
	# sys.exit(0x7f)
	while True:
		data = b'\x7f' * count
		data += (b'\x00' * (SIZE - len(data)))
		# data = make_image(data)
		# print_image(data)
		# break
		# time.sleep(0.01)
		# import os
		# os.system("clear")
		send(data)#make_image(data))
		print(count)
		for block in (data, compress(data)):
			print(len(block))
			for i in range(16):
				print(block[i*PANEL_WIDTH:(i+1)*PANEL_WIDTH])
		row = 0
		n = 0
		print('0  ', end=' ')
		for pixel in compress(data):
			if pixel & 0x80 == 0:
				for subpixel in range(7):
					print('{:02X}'.format(0x7f if (pixel & 0x01) > 0 else 0), end=' ')
					pixel >>= 1
					n += 1
					if n >= PANEL_WIDTH:
						print()
						row += 1
						print('{: <3}'.format(row), end=' ')
						n = 0
		print()
		# print(compress(data))
		break
		count += 1
		if count >= SIZE:
			count = 0
except KeyboardInterrupt:
	pass
	#send(b'\x00' * SIZE)
