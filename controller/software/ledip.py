import socket
import struct

IP = (10, 42, 2, 201)
NEW_IP = (10, 42, 3, 12)
GW = (10, 42, 1, 1)
NM = (255, 255, 0, 0)

PORT = 4242

def ip_pack(ip):
	return struct.pack("!BBBB", *ip)

def ip_str(ip):
	return '{}.{}.{}.{}'.format(*ip)

def make_packet(ip, gw, nm):
	packet = ip_pack(ip) + ip_pack(gw) + ip_pack(nm)
	checksum = 0xAA
	for b in packet:
		checksum ^= b
	packet = struct.pack("!B", checksum) + packet
	return packet

packet = make_packet(NEW_IP, GW, NM)
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
print(packet)
s.sendto(packet, (ip_str(IP), PORT))