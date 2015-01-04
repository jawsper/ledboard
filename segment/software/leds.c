/*
Support routine for the 32x16 led-board as sold by Baco Army Goods.
(C) 2008 Jeroen Domburg (jeroen AT spritesmods.com)

This program is free software: you can redistribute it and/or modify
t under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
        
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
                
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// CLK |     A0 | A1 |  A2 | UDAT | NC  | LDAT |  NC
// 1   |      3 |  5 |   7 |    9 |  11 |   13 |  15
// 2   |      4 |  6 |   8 |   10 |  12 |   14 |  16
// GND | STROBE | OE | GND |  GND | GND |  GND | GND

//Pinout of the led-board:
//1: CLK
//2: GND
//3: A0
//4: STROBE
//5: A1
//6: OE
//7: A2
//8: GND
//9: UDAT (upper pixel data)
//10: GND
//11: NC
//12: GND
//13: LDAT (lower pixel data)
//14: GND
//15: NC
//16: GND
//
//J1 is data in (to which the microcontroller must be soldered), J2
//is data out (which is unused here).

#include "leds.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

//IO-ports
//If you change the port itself (e.g. PB0->PD0) you'll have to
//track down these ports in the rest of the code too.
#define CLK PC3
#define PORT_CLK PORTC
#define A0 PC2
#define PORT_A0 PORTC
#define A1 PC1
#define PORT_A1 PORTC
#define A2 PC0
#define PORT_A2 PORTC
#define UDAT PB1
#define PORT_UDAT PORTB
#define LDAT PB2
#define PORT_LDAT PORTB
#define STROBE PC5
#define PORT_STROBE PORTC
#define OE PC4
#define PORT_OE PORTC

//Display memory. The main routine can write here to modify the image on the 
//LED-board. 0 = black = led completely off, 255 = 'white' = led completely on.
unsigned char dispmem[512];

//Initialize led-board-driver
void initleds(void)
{
//Pin direction
    DDRB = (1 << UDAT) | (1 << LDAT);
    DDRC = 0xff;
//Timer for ledboard row advance interrupt.
//Any timer can be used for this, as long as its interrupt routine
//calls the ISR with a speed of at least 13KHz.
    TCCR1A = 0;
    TCCR1B = 9; //CTC, no prescaler
    OCR1A = (F_CPU / (800 * 8)); 
//    TIMSK1=2; //OCIE1A int enable
}

//Timer interrupts. Sends the next row of pixels to the LCD.

//This routine has grayscale-support: it tries to PWM each pixel
//to let the mean of the duty-cycle be the value in the dispmem-array.
//To reduce flickering, the phase of the pwm-signal is inverted every pixel,
//every row and every frame. so each pixel will flicker out of sync with the
//pixels above, beneath, left from and right from it. This'll immensely reduce
//the perception of flickering, so the 'real' framerate can be much lower than
//the one required for flicker-free b/w images.


//GRAYINC will adjust the precision of the routines. If GRAYINC is
//(1<<n), the precision will be (8-n) bits. The advantage of a lower precision
//is that the frame rate can be turned down. (1<<3) is the limit for an
//ATMega88 at 20MHz and gives 5-bit precision.
#define GRAYINC (1 << 2)

void do_leds()
{
    static char rowno; //number of the row we're at now
    static unsigned char *posu,*posd; //position in display memory
    static unsigned char comp; //compare-value for grayscales
    char x;

    //Send next line    
    PORTB &= ~(1 << STROBE); //disable strobe in advance
    for(x=0; x<32; x++) //for each pixel:
    {
        PORT_UDAT &= ~(1 << UDAT);
        PORT_LDAT &= ~(1 << LDAT); //clear data inputs
        PORT_CLK &= ~(1 << CLK); //reset clock in advance
        if(*posu > comp) PORT_UDAT |= (1<<UDAT); //if pixel should be shown, set output
        if(*posd > comp) PORT_LDAT |= (1<<LDAT); //same for the lower 8 rows
        posu++; posd++; //next memory position
        PORT_CLK |= (1 << CLK); //clock in the bits
        comp += 128; //invert phase of pwm every pixel
    }
    comp += 128; //invert phase of pwm every row too


    PORT_OE |= (1 << OE); //disable led output
    PORT_STROBE |= (1 << STROBE); //latch in the led values
    //select correct row
    PORT_A0 &= ~((1 << A0) | (1 << A1) | (1 << A2));
    if(rowno & 1) PORT_A0 |= (1 << A0);
    if(rowno & 2) PORT_A1 |= (1 << A1);
    if(rowno & 4) PORT_A2 |= (1 << A2);
    PORT_OE &= ~(1 << OE); //enable led output
    rowno++; //Ok, this row is done.
    if(rowno > 7) //Frame done?
    {
        rowno = 0; //reset row count
        posu = dispmem;
        posd = dispmem + 256; //reset memory pos
        comp += 128 + GRAYINC; //invert phase of pwm and increase
    }
}


