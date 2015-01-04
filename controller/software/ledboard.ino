#include "EtherCard.h"
#include "IPAddress.h"
#include "EEPROMAnything.h"
#include <avr/io.h>

#define DEBUG 0

#define SIZE (96*48)

byte calculate_config_checksum(byte* data);
void udpReceive(word port, byte ip[4], const char *data, word len);
void ethernet_setup();
void eeprom_read_config();
void eeprom_write_config();

// ethernet interface default settings
static byte permitted_source_address[4] = { 10, 42, 2, 104 };
static byte default_ip[4] 		= { 10, 42, 3, 12 };
static byte default_gateway[4] 	= { 10, 42, 1, 1 };
static byte default_netmask[4] 	= { 255, 255, 0, 0 };


struct config_t {
	byte ip[4];
	byte gateway[4];
	byte netmask[4];
} config;


// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x70, 0x69, 0x69, 0xCA, 0xFE, 0x00 };

byte Ethernet::buffer[1536]; // tcp/ip send and receive buffer

byte calculate_config_checksum(byte* data)
{
	byte checksum = 0xAA;
	for(int i = 0; i < sizeof(config); i++) checksum ^= *data++;
	return checksum;
}

#define SERIAL_WAIT() while(!(UCSR0A & (1<<UDRE0)))
#define SERIAL_WRITE(x) SERIAL_WAIT(); UDR0 = x

byte pixels_per_byte = 1;

void udpReceive(word port, byte ip[4], const char *chr_data, word len)
{
	byte* data = (byte*)chr_data;

	switch(port)
	{
		case 1337:
		{
			#if DEBUG
			Serial.println("I FAILED YOU MY MASTER");
			#else
			if(pixels_per_byte != 1 && pixels_per_byte != 7)
			{
				SERIAL_WRITE(0x80);
				for(int i = 0; i < pixels_per_byte; i++)
				{
					SERIAL_WRITE(0x7f);
				}
				break;
			}
			for(int i = 0; i < len; i++)
			{
				byte val = *data++;

				if((val & 0x80) == 0)
				{
					val &= 0x7f;
					switch(pixels_per_byte)
					{
						case 0:
						case 1:
							SERIAL_WRITE(val);
							break;
						case 7:
							for(int i = 1; i < 7; i++)
							{
								SERIAL_WRITE((val & 0x01) ? 0x7f : 0x00);
								val >>= 1;
							}
							break;
					}
				}
				else
				{
					if(val != 0x80)
					{
						pixels_per_byte = val & 0x0f;
						continue;
					}
					pixels_per_byte = 1;
					SERIAL_WRITE(val & 0x80);
				}
			}
			#endif
			// Serial.write((const uint8_t*)data, len);
			break;
		}
		case 4242:
		{
			int _size = sizeof(config) + 1;
			if(len == _size)
			{
				byte checksum = *data++;

				byte* cfg = (byte*)&config;
				byte config_checksum = calculate_config_checksum((byte*)data);

				if(checksum == config_checksum)
				{
					memcpy(&config, data, sizeof(config));
					eeprom_write_config();

					ethernet_setup();
				}
			}
			break;
		}
	}
}

void ethernet_setup()
{
  	ether.staticSetup(config.ip, config.gateway, 0, config.netmask);

	ether.udpServerListenOnPort(&udpReceive, 1337);
	ether.udpServerListenOnPort(&udpReceive, 4242);
}

void eeprom_read_config()
{
	byte checksum = EEPROM.read(0);
	EEPROM_readAnything(1, config);
	byte config_checksum = calculate_config_checksum((byte*)&config);
	if(config_checksum != checksum)
	{
		EtherCard::copyIp(config.ip, default_ip);
		EtherCard::copyIp(config.gateway, default_gateway);
		EtherCard::copyIp(config.netmask, default_netmask);
		eeprom_write_config();
	}
}

void eeprom_write_config()
{
	byte checksum = calculate_config_checksum((byte*)&config);
	EEPROM.write(0, checksum);
	EEPROM_writeAnything(1, config);
}

void setup()
{
	#if DEBUG
	Serial.begin(115200);
	Serial.println("DEBUG");
	#else
	Serial.begin(500000);
	#endif

	eeprom_read_config();

	if (ether.begin(sizeof Ethernet::buffer, mymac, 8) == 0)
	{
		// for(int i = 0; i < 5; i++) Serial.write(0x80);
		// for(int i = 0; i < 100; i++)
		// {
		// 	Serial.write(0x7f);
		// 	Serial.write(0x01);
		// }
		while(1);
	}

	ethernet_setup();


	#if DEBUG
	Serial.println("Init completed");
	#else
	delay(3000);
	for(int i = 0; i < 5; i++)
		Serial.write(0x80);
	for(int i = 0; i < 96*48; i++)
	{
		Serial.write(0);
	}
	#endif

}

static bool eth_type_is_ip(byte* packet, uint16_t len)
{
	return 
		packet[ETH_TYPE_H_P] == ETHTYPE_IP_H_V &&
		packet[ETH_TYPE_L_P] == ETHTYPE_IP_L_V &&
		packet[IP_HEADER_LEN_VER_P] == 0x45;
}


bool isMySpecialPacket(byte* header, uint16_t len)
{
    bool is_ip = eth_type_is_ip(header, len);
    bool is_udp = header[IP_PROTO_P] == IP_PROTO_UDP_V;
    uint16_t port = (header[UDP_DST_PORT_H_P] << 8) | (header[UDP_DST_PORT_L_P] & 0xFF);
	return is_ip && is_udp && port == 1337 && memcmp(config.ip, header + IP_DST_P, 4) == 0;
	// && memcmp(permitted_source_address, header + IP_SRC_P, 4) == 0;
}
void specialPacketReceive(byte b)
{
	while(!(UCSR0A & (1<<UDRE0)));
	UDR0 = b;
}

void loop()
{
	//this must be called for ethercard functions to work.

	// packetLoop in tcpip.cpp
	// packetReceive in enc28j60.cpp
	#if DEBUG
	ether.packetLoop(ether.specialPacketReceive(&isMySpecialPacket, &specialPacketReceive));
	#else
	ether.packetLoop(ether.packetReceive());
	#endif
}