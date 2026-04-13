/**
 *  @filename   :   epd2in9b_V4.cpp
 *  @brief      :   Implements for e-paper library
 *  @author     :   Waveshare
 *
 *  Copyright (C) Waveshare     2023-12-20
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include "epd2in9b_V4.h"
// #include "imagedata.h"

// Partial refresh LUT from GxEPD2 (B/W SSD1680)
const unsigned char lut_partial[] PROGMEM = {
  0x0, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x80, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x40, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0A, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2,
  0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0,
};

Epd::~Epd() {
};

Epd::Epd() {
    reset_pin = RST_PIN;
    dc_pin = DC_PIN;
    cs_pin = CS_PIN;
    busy_pin = BUSY_PIN;
    width = EPD_WIDTH;
    height = EPD_HEIGHT;
};

int Epd::Init(void) {
    if (IfInit() != 0) {
        return -1;
    }
    Reset();

    ReadBusy();
    SendCommand(0x12);
    ReadBusy();

    SendCommand(0x01); //Driver output control      
    SendData((height-1)%256);    
    SendData((height-1)/256);
    SendData(0x00);

    SendCommand(0x11); //data entry mode       
    SendData(0x03);

    SendCommand(0x44); //set Ram-X address start/end position   
    SendData(0x00);
    SendData(width/8-1);   

    SendCommand(0x45); //set Ram-Y address start/end position          
    SendData(0x00);
    SendData(0x00); 
    SendData((height-1)%256);    
    SendData((height-1)/256);

    SendCommand(0x3C); //BorderWavefrom
    SendData(0x05);	

    SendCommand(0x21); //  Display update control
    SendData(0x00);		
    SendData(0x80);	

    SendCommand(0x18); //Read built-in temperature sensor
    SendData(0x80);	

    SendCommand(0x4E);   // set RAM x address count to 0;
    SendData(0x00);
    SendCommand(0x4F);   // set RAM y address count to 0X199;    
    SendData(0x00);    
    SendData(0x00);
    ReadBusy();

    return 0;
}

void Epd::ReinitFast(void) {
    ReadBusy();
    SendCommand(0x12); // SWRESET
    ReadBusy();

    SendCommand(0x18); // Read built-in temperature sensor
    SendData(0x80);
    SendCommand(0x22); // Load temperature value
    SendData(0xB1);
    SendCommand(0x20);
    ReadBusy();

    SendCommand(0x1A); // Write to temperature register
    SendData(0x5A);    // 90°C
    SendData(0x00);
    SendCommand(0x22); // Load temperature value
    SendData(0x91);
    SendCommand(0x20);
    ReadBusy();

    SendCommand(0x01); // Driver output control
    SendData((height - 1) % 256);
    SendData((height - 1) / 256);
    SendData(0x00);
    SendCommand(0x11); // Data entry mode
    SendData(0x03);
    SendCommand(0x44); // RAM X start/end
    SendData(0x00);
    SendData(width / 8 - 1);
    SendCommand(0x45); // RAM Y start/end
    SendData(0x00);
    SendData(0x00);
    SendData((height - 1) % 256);
    SendData((height - 1) / 256);
    SendCommand(0x4E); // RAM X counter
    SendData(0x00);
    SendCommand(0x4F); // RAM Y counter
    SendData(0x00);
    SendData(0x00);
}

int Epd::Init_Fast(void) {
    if (IfInit() != 0) {
        return -1;
    }
    Reset();

    ReadBusy();   
    SendCommand(0x12);  //SWRESET
    ReadBusy();   	

    SendCommand(0x18); //Read built-in temperature sensor
    SendData(0x80);

    SendCommand(0x22); // Load temperature value
    SendData(0xB1);		
    SendCommand(0x20);	
    ReadBusy();   

    SendCommand(0x1A); // Write to temperature register
    SendData(0x5A);		// 90
    SendData(0x00);	
                
    SendCommand(0x22); // Load temperature value
    SendData(0x91);		
    SendCommand(0x20);	
    ReadBusy();  

    SendCommand(0x01); //Driver output control      
    SendData((height-1)%256);    
    SendData((height-1)/256);
    SendData(0x00);

    SendCommand(0x11); //data entry mode       
    SendData(0x03);

    SendCommand(0x44); //set Ram-X address start/end position   
    SendData(0x00);
    SendData(width/8-1);   

    SendCommand(0x45); //set Ram-Y address start/end position          
    SendData(0x00);
    SendData(0x00); 
    SendData((height-1)%256);    
    SendData((height-1)/256);	

    SendCommand(0x4E);   // set RAM x address count to 0;
    SendData(0x00);
    SendCommand(0x4F);   // set RAM y address count to 0X199;
    SendData(0x00);
    SendData(0x00);

    return 0;
}

/**
 *  @brief: basic function for sending commands
 */
void Epd::SendCommand(unsigned char command) {
    DigitalWrite(dc_pin, LOW);
    SpiTransfer(command);
}

/**
 *  @brief: basic function for sending data
 */
void Epd::SendData(unsigned char data) {
    DigitalWrite(dc_pin, HIGH);
    SpiTransfer(data);
}

void Epd::SendDataBulk(const unsigned char* data, unsigned int len) {
    DigitalWrite(dc_pin, HIGH);
    SpiTransferBulk(data, len);
}

/**
 *  @brief: Wait until the busy_pin goes HIGH
 */
void Epd::ReadBusy(void) {
  while(1)
  {
    if(DigitalRead(busy_pin) == 0)
      break;
    DelayMs(10);
  }
  DelayMs(10);
}

/**
 *  @brief: module reset.
 *          often used to awaken the module in deep sleep,
 *          see Epd::Sleep();
 */
void Epd::Reset(void) {
    DigitalWrite(reset_pin, HIGH);
    DelayMs(20);
    DigitalWrite(reset_pin, LOW);                //module reset
    DelayMs(5);
    DigitalWrite(reset_pin, HIGH);
    DelayMs(20);
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
void Epd::TurnOnDisplay(void)
{
	SendCommand(0x22); //Display Update Control
	SendData(0xF7);
	SendCommand(0x20); //Activate Display Update Sequence
	ReadBusy();
}

void Epd::TurnOnDisplay_Base(void)
{
	SendCommand(0x22); //Display Update Control
	SendData(0xF4);
	SendCommand(0x20); //Activate Display Update Sequence
	ReadBusy();
}

void Epd::TurnOnDisplay_Partial(void)
{
	SendCommand(0x22); //Display Update Control
	SendData(0xCC);
	SendCommand(0x20); //Activate Display Update Sequence
	ReadBusy();
}

void Epd::LoadPartialLUT(void)
{
	SendCommand(0x32);
	for (int i = 0; i < 153; i++) {
		SendData(pgm_read_byte(&lut_partial[i]));
	}
}

void Epd::TurnOnDisplay_Fast(void)
{
	SendCommand(0x22); //Display Update Control
	SendData(0xC7);
	SendCommand(0x20); //Activate Display Update Sequence
	ReadBusy();
}

void Epd::Display(const UBYTE *blackimage, const UBYTE *ryimage) {
    UWORD imageSize = width / 8 * height;

    SendCommand(0x24);
    SendDataBulk(blackimage, imageSize);

    SendCommand(0x26);
    const UWORD CHUNK = 256;
    UBYTE buf[CHUNK];
    for (UWORD offset = 0; offset < imageSize; offset += CHUNK) {
        UWORD len = (imageSize - offset < CHUNK) ? imageSize - offset : CHUNK;
        for (UWORD i = 0; i < len; i++) {
            buf[i] = ~ryimage[offset + i];
        }
        SendDataBulk(buf, len);
    }

    TurnOnDisplay();
}

void Epd::Display_Fast(const UBYTE *blackimage, const UBYTE *ryimage) {
    UWORD imageSize = width / 8 * height;

    SendCommand(0x24);
    SendDataBulk(blackimage, imageSize);

    // Red buffer needs inversion — invert in chunks to avoid large temp allocation
    SendCommand(0x26);
    const UWORD CHUNK = 256;
    UBYTE buf[CHUNK];
    for (UWORD offset = 0; offset < imageSize; offset += CHUNK) {
        UWORD len = (imageSize - offset < CHUNK) ? imageSize - offset : CHUNK;
        for (UWORD i = 0; i < len; i++) {
            buf[i] = ~ryimage[offset + i];
        }
        SendDataBulk(buf, len);
    }

    TurnOnDisplay_Fast();
}



void Epd::Clear() {
    //send black data
    SendCommand(0x24);
    for (UWORD j = 0; j < height; j++) {
        for (UWORD i = 0; i < width/8; i++) {
            SendData(0xff);
        }
    }
    //send red data
    SendCommand(0x26);
    for (UWORD j = 0; j < height; j++) {
        for (UWORD i = 0; i < width/8; i++) {
            SendData(0x00);
        }
    }
    TurnOnDisplay_Base();
    SendCommand(0x26);
    for (UWORD j = 0; j < height; j++) {
        for (UWORD i = 0; i < width/8; i++) {
            SendData(0xff);
        }
    }
}

void Epd::Clear_Base() {
    //send black data
    SendCommand(0x10);
    for (UWORD j = 0; j < height; j++) {
        for (UWORD i = 0; i < width/8; i++) {
            SendData(0xff);
        }
    }
    //send red data
    SendCommand(0x26);
    for (UWORD j = 0; j < height; j++) {
        for (UWORD i = 0; i < width/8; i++) {
            SendData(0x00);
        }
    }
    TurnOnDisplay();
}

//Partial refresh display
void Epd::Partial(const UBYTE *Image, UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend)
{
    if((Xstart % 8 + Xend % 8 == 8 && Xstart % 8 > Xend % 8) || Xstart % 8 + Xend % 8 == 0 || (Xend - Xstart)%8 == 0)
    {
        Xstart = Xstart / 8 ;
        Xend = Xend / 8;
    }
    else
    {
        Xstart = Xstart / 8 ;
        Xend = Xend % 8 == 0 ? Xend / 8 : Xend / 8 + 1;
    }
    

    UWORD i, Width;
	Width = Xend -  Xstart;
	UWORD IMAGE_COUNTER = Width * (Yend-Ystart);

	Xend -= 1;
	Yend -= 1;	

    SendCommand(0x44);       // set RAM x address start/end, in page 35
    SendData(Xstart & 0xff);    // RAM x address start at 00h;
    SendData(Xend & 0xff);    // RAM x address end at 0fh(15+1)*8->128 
    SendCommand(0x45);       // set RAM y address start/end, in page 35
    SendData(Ystart & 0xff);    // RAM y address start at 0127h;
    SendData((Ystart>>8) & 0x01);    // RAM y address start at 0127h;
    SendData(Yend & 0xff);    // RAM y address end at 00h;
    SendData((Yend>>8) & 0x01); 

    SendCommand(0x4E);   // set RAM x address count to 0;
    SendData(Xstart & 0xff); 
    SendCommand(0x4F);   // set RAM y address count to 0X127;    
    SendData(Ystart & 0xff);
    SendData((Ystart>>8) & 0x01);


    SendCommand(0x24);   //Write Black and White image to RAM
    for (i = 0; i < IMAGE_COUNTER; i++) {
	    SendData(pgm_read_byte(&Image[i]));
	}
	TurnOnDisplay_Partial();

}
/**
 *  @brief: After this command is transmitted, the chip would enter the 
 *          deep-sleep mode to save power. 
 *          The deep sleep mode would return to standby by hardware reset. 
 *          The only one parameter is a check code, the command would be
 *          You can use EPD_Reset() to awaken
 */
void Epd::Sleep(void) {
    SendCommand(0x10); // DEEP_SLEEP
    SendData(0x01); // check code
}



/* END OF FILE */
