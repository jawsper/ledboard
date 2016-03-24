#include "LedBoard.h"
#include "font7x5.h"
#include "defines.h"
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>


/*

Protocol is called LMCP (LedMatrixControlProtocol)

Works via UDP port 1337

Command format:
	uint8_t command:	the command
	[uint8_t data ...]:	the data (can be non-existant)
	
Multiple commands can be sent in 1 datagram (optional).

Commands:
	0x01: write buffer, writes the current framebuffer to the screem
		no arguments
	0x02: clear, clears the matrix and writes the current framebuffer to the screen
		no arguments
	0x10: draw rows
		* uint y:
			y position of the row to draw (0-5)
		* uint8_t data[96*8]:
			send the data for 8 rows at a time, position is y * 8
	0x11: draw image rectangle
		* uint8_t x:
			top left x position of pixel data
		* uint8_t y:
			top left y position of pixel data
		* uint8_t width:
			width of pixel data
		* uint8_t height:
			height of pixel data
		* uint8_t data[width * height]:
			pixel data
	0x20: write text line based
		chars are 5x7 -> 6x8 including space and line separation
		* uint8_t x:
			top left x position in chars (0-15)
		* uint8_t y:
			top left y position in chars (0-5)
		* uint8_t brightness:
			brightness of text (0x00-0xFF)
		* [uint8_t text[...]]:
			text (ascii)
		* 0x00:
			terminator
	0x21: write text absolute
		* uint8_t x:
			top left x position in pixels (0-95)
		* uint8_t y:
			top left y position in pixels (0-47)
		* uint8_t brightness:
			brightness of text (0x00-0xFF)
		* [uint8_t text[...]]:
			text (ascii)
		* 0x00:
			terminator
	
*/

// serial baudrate
#define BAUDRATE 500000

// panel layout
#define PANEL_COUNT 9
int LedBoard::panel_layout[PANEL_COUNT][2] = {
	{2, 0}, {2, 1}, {2, 2},
	{1, 2}, {1, 1}, {1, 0},
	{0, 0}, {0, 1}, {0, 2},
};
// board width, segment width
#define X_SIZE 96
#define SEGMENT_X_SIZE 32
// board height, segment height
#define Y_SIZE 48
#define SEGMENT_Y_SIZE 16

// text char width, height, column count, row count
#define TEXT_CHAR_WIDTH 5
#define TEXT_CHAR_HEIGHT 7
#define TEXT_COLUMNS 16
#define TEXT_LINES 6

// size of board for buffers
#define TOTAL_SIZE (X_SIZE * Y_SIZE)

int LedBoard::width = X_SIZE;
int LedBoard::height = Y_SIZE;
// buffer that can be written to the matrix
uint8_t LedBoard::buffer[TOTAL_SIZE];
// buffer, set at init, keeps track of pixel location to pixel order to send to panels
uint16_t LedBoard::pixel_map[TOTAL_SIZE];

// initialize the pixel map and the serial port
void LedBoard::init()
{
	int pos = 0;
	for(int i = 0; i < PANEL_COUNT; i++)
	{
		int x_offset = panel_layout[i][0] * SEGMENT_X_SIZE;
		int y_offset = panel_layout[i][1] * SEGMENT_Y_SIZE;

		for(int y = 0; y < SEGMENT_Y_SIZE; y++)
		{
			int y_index = (y + y_offset) * X_SIZE;
			for(int x = 0; x < SEGMENT_X_SIZE; x++)
			{
				int x_index = x + x_offset;
				pixel_map[pos++] = y_index + x_index;
			}
		}
	}

	fd = fopen("/dev/ttyAMA0", "w");
	if(!fd)
	{
		printf("Error opening serial port!");
		exit(1);
	}

	int fd2 = fileno(fd);
	struct termios options;
	tcgetattr(fd2, &options);
	cfsetispeed(&options, B500000);
	cfsetospeed(&options, B500000);
	tcsetattr(fd2, TCSANOW, &options);

//	Serial1.begin(BAUDRATE);
}

// clear the whole board
void LedBoard::clear()
{
	memset(buffer, 0, width * height);
	outputStart();
	writeBuffer();
}

// processes the incoming packets
bool LedBoard::processPacket(const uint8_t* data, uint16_t packet_len)
{
	uint16_t packet_position = 0;
	// as long as there is data still...
	while(packet_position < packet_len)
	{
		// first byte is command
		uint8_t cmd = data[packet_position++];
		#ifdef DEBUG
		Serial.print("Packet: ");
		Serial.print(cmd, HEX);
		Serial.print(" " + String(packet_len));
		Serial.println();
		#endif
		switch(cmd)
		{
			// write buffer
			case 0x01:
				writeBuffer();
				break;

			// clear
			case 0x02:
				clear();
				break;

			// draw rows
			case 0x10:
			{
				// need 1 byte for y and 96 * 8 for pixel data
				if(packet_len - packet_position < 1 + (96 * 8))
					return false;

				uint8_t y = data[packet_position++];

				packet_position += drawImage(0, y * 8, 96, 8, (uint8_t*)data + packet_position);

				break;
			}
			// draw image rectangle
			case 0x11:
			{
				// 4 bytes for header
				if(packet_len - packet_position < 4)
					return false;
				uint8_t x = data[packet_position++];
				uint8_t y = data[packet_position++];
				uint8_t width = data[packet_position++];
				uint8_t height = data[packet_position++];

				// need enough bytes 
				if(packet_len - packet_position < width * height)
					return false;

				packet_position += drawImage(x, y, width, height, (uint8_t*)(data + packet_position));
				
				break;
			}

			// write text line based
			case 0x20:
			// write text absolute
			case 0x21:
			{
				bool absolute = cmd == 0x21;
				uint8_t x = data[packet_position++];
				uint8_t y = data[packet_position++];
				uint8_t brightness = data[packet_position++];
				int16_t str_size = strnlen((char*)(data + packet_position), packet_len - packet_position);
				// string error
				if(str_size < 0)
					return false;
				packet_position += drawString(
					(char*)(data + packet_position), 
					str_size, 
					x, y, 
					brightness,
					absolute
				);
				break;
			}
			// unknown command -> ignore this packet
			default:
				return false;
		}
	}
	return true;
}


uint16_t LedBoard::drawStringNoLen(char* text, uint8_t x_pos, uint8_t y_pos, uint8_t brightness, bool absolute)
{
	return drawString(text, strlen(text), x_pos, y_pos, brightness, absolute);
}


// draw a string at x_pos, y_pos, optionally absolute position
uint16_t LedBoard::drawString(char* text, uint16_t len, uint8_t x_pos, uint8_t y_pos, uint8_t brightness, bool absolute)
{
	if(!absolute)
	{
		x_pos *= TEXT_CHAR_WIDTH + 1;
		y_pos *= TEXT_CHAR_HEIGHT + 1;
	}
	if(x_pos + TEXT_CHAR_WIDTH >= width) goto writeText_exit;
	if(y_pos + TEXT_CHAR_HEIGHT >= height) goto writeText_exit;

	for(int i = 0; i < len; i++)
	{
		int char_pos = (text[i] - 0x20);
		if(char_pos < 0 || char_pos > (0x7f - 0x20)) char_pos = 0;
		char_pos *= TEXT_CHAR_WIDTH; // 5 byte per char

		// draw 1 extra for the space, it's ok if it overflows there because setPixel will catch that
		for(int char_x = 0; char_x < TEXT_CHAR_WIDTH + 1; char_x++)
		{
			// this draws a 1 pixel empty column after each character
			uint8_t c = char_x == TEXT_CHAR_WIDTH ? 0 : Font5x7[char_pos + char_x];

			// draw 1 extra to make sure the pixels under each char are cleared too
			for(int k = 0; k < TEXT_CHAR_HEIGHT + 1; k++)
			{
				bool on = (c & (1 << k)) != 0;
				setPixel(
					on ? brightness : 0x00,
					x_pos + char_x + ((TEXT_CHAR_WIDTH + 1) * i), 
					y_pos + k
				);
			}
		}
	}
writeText_exit:
	return len + 1;
}


// draws an image in the specified region
uint16_t LedBoard::drawImage(uint8_t x, uint8_t y, uint16_t width, uint16_t height, uint8_t* data)
{
	for(int y_pos = 0; y_pos < height; y_pos++)
	{
		for(int x_pos = 0; x_pos < width; x_pos++)
		{
			setPixel(*data++, x + x_pos, y + y_pos);
		}
	}
	return width * height;
}

// render a WIDTH * HEIGHT XBM header
void LedBoard::drawXBM(const uint8_t* data, uint16_t len)
{
	int buffer_pos = 0;
	for(uint16_t i = 0; i < len; i++)
	{
		for(uint8_t j = 0; j < 8; j++)
		{
			setPixel((data[i] & (1 << j)) ? 0 : 0x7f, buffer_pos++);
		}
	}
}


// draw the curent framebuffer on the screen
void LedBoard::writeBuffer()
{
	outputStart();
	for(int i = 0; i < TOTAL_SIZE; i++)
	{
		outputWrite(buffer[pixel_map[i]] >> 1);
	}
}


// set pixel by index
void LedBoard::setPixel(uint8_t val, int pos)
{
	if(pos < 0 || pos >= TOTAL_SIZE) return;
	buffer[pos] = val;
}

// set pixel by x/y position
void LedBoard::setPixel(uint8_t val, uint8_t x, uint8_t y)
{
	if(x >= width || y >= height) return;
	setPixel(val, y * width + x);
}


// send a reset / start byte to the segments
void LedBoard::outputStart()
{
	outputWrite(0x80);
}

// actually write the pixel to the serial interface
void LedBoard::outputWrite(uint8_t val)
{
	int val2 = fputc(val, fd);
	if(val2 == EOF)
	{
		printf("TX ERROR\n");
	}
	fflush(fd);
//	Serial1.write(val);
}
