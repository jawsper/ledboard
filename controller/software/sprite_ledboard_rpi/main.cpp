#include <stdint.h>
#include "LedBoard.h"
#include "tkkrlab_96x48.xbm"
#include "defines.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

static LedBoard board;

int sock;

void udpReceive(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len)
{
	switch(dest_port)
	{
		case 1337:
		{
			bool success = board.processPacket((const uint8_t*)data, len);
			if(!success)
			{
				#ifdef DEBUG
				Serial.println("Packet error!");
				#endif
				board.clear();
				board.drawString((char*)"Packet error!", 0, 0, 0xFF);
				board.writeBuffer();
			}
			else
			{
				#ifdef DEBUG
				Serial.println("No packet error!");
				#endif
			}
			break;
		}
	}
}

void setup()
{
	board.init();
	board.drawXBM((const uint8_t*)&tkkrlab_96x48_bits, sizeof tkkrlab_96x48_bits);
	board.drawStringNoLen((char*)"TkkrLab Ledboard", 0, 0);
	board.drawStringNoLen((char*)"Loading...", 0, 5);
	board.writeBuffer();

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(1337);
	bind(sock, (struct sockaddr*)&addr, sizeof addr);
}

int main(int argc, char* argv[])
{
	setup();
	char buffer[65535];
	while(1)
	{
		int c = recvfrom(sock, buffer, 65535, 0, 0, 0);
		board.processPacket((const uint8_t*)buffer, c);
		printf("Recv: %d\n", c);
	}
	return 0;
}
