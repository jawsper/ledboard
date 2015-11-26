#ifndef _LEDBOARD_H_
#define _LEDBOARD_H_

#include <stdint.h>

class LedBoard
{
public:
	LedBoard() {};
	~LedBoard() {};

	void init();
	void clear();

	bool processPacket(const uint8_t*, uint16_t);

	// draw functions
	uint16_t drawString(char*, uint8_t, uint8_t, bool absolute=false);
	uint16_t drawString(char*, uint16_t, uint8_t, uint8_t, bool absolute=false);
	uint16_t drawImage(uint8_t x, uint8_t y, uint16_t width, uint16_t height, uint8_t* data);
	void drawXBM(const uint8_t*, uint16_t);

	// output the buffer
	void writeBuffer();

private:
	static int panel_layout[][2];
	static int width;
	static int height;
	static uint8_t buffer[];
	static uint16_t pixel_map[];

	void outputStart();
	void outputWrite(uint8_t);
	
	void setPixel(uint8_t val, int pos);
	void setPixel(uint8_t val, uint8_t x, uint8_t y);
};

#endif //_IMAGE_GEN_H