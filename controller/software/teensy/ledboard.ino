#include <stdint.h>
#include <EtherCard.h>
#include "LedBoard.h"
#include "tkkrlab_96x48.xbm"

static LedBoard board;

#define ETHERNET_BUFFER_SIZE 2000
#define PIN_LED 13
#define PIN_CS 10

void udpReceive(word port, byte ip[4], const char *data, word len);

// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x70, 0x69, 0x69, 0xCA, 0xFE, 0x00 };

uint8_t Ethernet::buffer[ETHERNET_BUFFER_SIZE]; // tcp/ip send and receive buffer

void udpReceive(uint16_t dest_port, uint8_t src_ip[4], uint16_t src_port, const char *data, uint16_t len)
{
	switch(dest_port)
	{
		case 1337:
		{
			board.processPacket((const uint8_t*)data, len);
			break;
		}
	}
}

void setup()
{
	delay(1000);
	board.init();
	board.drawXBM((const uint8_t*)&tkkrlab_96x48_bits, sizeof tkkrlab_96x48_bits);
	board.drawString("Loading...", 0, 0);
	board.writeBuffer();

	if(ether.begin(sizeof Ethernet::buffer, mymac, PIN_CS) == 0)
	{
		board.clear();
		board.drawString("Ethernet error!", 0, 0);
		board.writeBuffer();
		while(1);
	}
	if(!ether.dhcpSetup("ledboard"))
	{
		board.clear();
		board.drawString("DHCP error!", 0, 0);
		board.writeBuffer();
		while(1);
	}
	ether.udpServerListenOnPort(&udpReceive, 1337);
 
	board.drawString("TkkrLab Ledboard", 0, 0);
	static char sprintf_buffer[256];
	sprintf(sprintf_buffer, "IP: %d.%d.%d.%d", ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3]);
	board.drawString(sprintf_buffer, 0, 5);
	board.writeBuffer();
	delay(5000);
	board.clear();
}

void loop()
{
	//this must be called for ethercard functions to work.
	ether.packetLoop(ether.packetReceive());
}