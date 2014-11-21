/*
    This file is part of Repetier-Firmware.

    Repetier-Firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Repetier-Firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Repetier-Firmware.  If not, see <http://www.gnu.org/licenses/>.

*/

#define UI_MAIN
#include "Repetier.h"
extern const int8_t encoder_table[16] PROGMEM ;
#include <math.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>

char uipagedialog[4][MAX_COLS+1];

#if BEEPER_TYPE==2 && defined(UI_HAS_I2C_KEYS) && UI_I2C_KEY_ADDRESS!=BEEPER_ADDRESS
#error Beeper address and i2c key address must be identical
#else
#if BEEPER_TYPE==2
#define UI_I2C_KEY_ADDRESS BEEPER_ADDRESS
#endif
#endif

#if UI_AUTORETURN_TO_MENU_AFTER!=0
long ui_autoreturn_time=0;
bool benable_autoreturn=true;
#endif

#if FEATURE_BEEPER
bool enablesound = true;
#endif

#if UI_AUTOLIGHTOFF_AFTER!=0
millis_t UIDisplay::ui_autolightoff_time=-1;
#endif

uint8_t UIDisplay::display_mode=ADVANCED_MODE;

void playsound(int tone,int duration)
{
#if FEATURE_BEEPER
if (!HAL::enablesound)return;
HAL::tone(BEEPER_PIN, tone);
HAL::delayMilliseconds(duration);
HAL::noTone(BEEPER_PIN);
#endif
}
float Z_probe[3];
void beep(uint8_t duration,uint8_t count)
{
#if FEATURE_BEEPER
if (!HAL::enablesound)return;
#if BEEPER_TYPE!=0
#if BEEPER_TYPE==1 && defined(BEEPER_PIN) && BEEPER_PIN>=0
    SET_OUTPUT(BEEPER_PIN);
#endif
#if BEEPER_TYPE==2
    HAL::i2cStartWait(BEEPER_ADDRESS+I2C_WRITE);
#if UI_DISPLAY_I2C_CHIPTYPE==1
    HAL::i2cWrite( 0x14); // Start at port a
#endif
#endif
    for(uint8_t i=0; i<count; i++)
    {
#if BEEPER_TYPE==1 && defined(BEEPER_PIN) && BEEPER_PIN>=0
#if defined(BEEPER_TYPE_INVERTING) && BEEPER_TYPE_INVERTING
        WRITE(BEEPER_PIN,LOW);
#else
        WRITE(BEEPER_PIN,HIGH);
#endif
#else
#if UI_DISPLAY_I2C_CHIPTYPE==0
#if BEEPER_ADDRESS == UI_DISPLAY_I2C_ADDRESS
        HAL::i2cWrite(uid.outputMask & ~BEEPER_PIN);
#else
        HAL::i2cWrite(~BEEPER_PIN);
#endif
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==1
        HAL::i2cWrite((BEEPER_PIN) | uid.outputMask);
        HAL::i2cWrite(((BEEPER_PIN) | uid.outputMask)>>8);
#endif
#endif
        HAL::delayMilliseconds(duration);
#if BEEPER_TYPE==1 && defined(BEEPER_PIN) && BEEPER_PIN>=0
#if defined(BEEPER_TYPE_INVERTING) && BEEPER_TYPE_INVERTING
        WRITE(BEEPER_PIN,HIGH);
#else
        WRITE(BEEPER_PIN,LOW);
#endif
#else
#if UI_DISPLAY_I2C_CHIPTYPE==0

#if BEEPER_ADDRESS == UI_DISPLAY_I2C_ADDRESS
        HAL::i2cWrite((BEEPER_PIN) | uid.outputMask);
#else
        HAL::i2cWrite(255);
#endif
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==1
        HAL::i2cWrite( uid.outputMask);
        HAL::i2cWrite(uid.outputMask>>8);
#endif
#endif
        HAL::delayMilliseconds(duration);
    }
#if BEEPER_TYPE==2
    HAL::i2cStop();
#endif
#endif
#endif
}

bool UIMenuEntry::showEntry() const
{
    bool ret = true;
    uint8_t f,f2,f3;
	//check what mode is targeted
	f3 = HAL::readFlashByte((PGM_P)&display_mode);
	//if not for current mode not need to continue
	if (!(f3 & UIDisplay::display_mode) ) return false;
    f = HAL::readFlashByte((PGM_P)&filter);
    if(f!=0)
        ret = (f & Printer::menuMode) != 0;
    f2 = HAL::readFlashByte((PGM_P)&nofilter);
    if(ret && f2!=0)
    {
        ret = (f2 & Printer::menuMode) == 0;
    }
    return ret;
}

#if UI_DISPLAY_TYPE!=0
UIDisplay uid;
char displayCache[UI_ROWS][MAX_COLS+1];

// Menu up sign - code 1
// ..*.. 4
// .***. 14
// *.*.* 21
// ..*.. 4
// ***.. 28
// ..... 0
// ..... 0
// ..... 0
const uint8_t character_back[8] PROGMEM = {4,14,21,4,28,0,0,0};
// Degrees sign - code 2
// ..*.. 4
// .*.*. 10
// ..*.. 4
// ..... 0
// ..... 0
// ..... 0
// ..... 0
// ..... 0
const uint8_t character_degree[8] PROGMEM = {4,10,4,0,0,0,0,0};
// selected - code 3
// ..... 0
// ***** 31
// ***** 31
// ***** 31
// ***** 31
// ***** 31
// ***** 31
// ..... 0
// ..... 0
const uint8_t character_selected[8] PROGMEM = {0,31,31,31,31,31,0,0};
// unselected - code 4
// ..... 0
// ***** 31
// *...* 17
// *...* 17
// *...* 17
// *...* 17
// ***** 31
// ..... 0
// ..... 0
const uint8_t character_unselected[8] PROGMEM = {0,31,17,17,17,31,0,0};
// unselected - code 5
// ..*.. 4
// .*.*. 10
// .*.*. 10
// .*.*. 10
// .*.*. 10
// .***. 14
// ***** 31
// ***** 31
// .***. 14
const uint8_t character_temperature[8] PROGMEM = {4,10,10,10,14,31,31,14};
// unselected - code 6
// ..... 0
// ***.. 28
// ***** 31
// *...* 17
// *...* 17
// ***** 31
// ..... 0
// ..... 0
const uint8_t character_folder[8] PROGMEM = {0,28,31,17,17,31,0,0};

// printer ready - code 7
// *...* 17
// .*.*. 10
// ..*.. 4
// *...* 17
// ..*.. 4
// .*.*. 10
// *...* 17
// *...* 17
const byte character_ready[8] PROGMEM = {17,10,4,17,4,10,17,17};

// Bed - code 2
// ..... 0
// ***** 31
// *.*.* 21
// *...* 17
// *.*.* 21
// ***** 31
// ..... 0
// ..... 0
const byte character_bed[8] PROGMEM = {0,31,21,17,21,31,0,0};

const long baudrates[] PROGMEM = {9600,14400,19200,28800,38400,56000,57600,76800,111112,115200,128000,230400,250000,256000,
                                  460800,500000,921600,1000000,1500000,0
                                 };

#define LCD_ENTRYMODE			0x04			/**< Set entrymode */

/** @name GENERAL COMMANDS */
/*@{*/
#define LCD_CLEAR			0x01	/**< Clear screen */
#define LCD_HOME			0x02	/**< Cursor move to first digit */
/*@}*/

/** @name ENTRYMODES */
/*@{*/
#define LCD_ENTRYMODE			0x04			/**< Set entrymode */
#define LCD_INCREASE		LCD_ENTRYMODE | 0x02	/**<	Set cursor move direction -- Increase */
#define LCD_DECREASE		LCD_ENTRYMODE | 0x00	/**<	Set cursor move direction -- Decrease */
#define LCD_DISPLAYSHIFTON	LCD_ENTRYMODE | 0x01	/**<	Display is shifted */
#define LCD_DISPLAYSHIFTOFF	LCD_ENTRYMODE | 0x00	/**<	Display is not shifted */
/*@}*/

/** @name DISPLAYMODES */
/*@{*/
#define LCD_DISPLAYMODE			0x08			/**< Set displaymode */
#define LCD_DISPLAYON		LCD_DISPLAYMODE | 0x04	/**<	Display on */
#define LCD_DISPLAYOFF		LCD_DISPLAYMODE | 0x00	/**<	Display off */
#define LCD_CURSORON		LCD_DISPLAYMODE | 0x02	/**<	Cursor on */
#define LCD_CURSOROFF		LCD_DISPLAYMODE | 0x00	/**<	Cursor off */
#define LCD_BLINKINGON		LCD_DISPLAYMODE | 0x01	/**<	Blinking on */
#define LCD_BLINKINGOFF		LCD_DISPLAYMODE | 0x00	/**<	Blinking off */
/*@}*/

/** @name SHIFTMODES */
/*@{*/
#define LCD_SHIFTMODE			0x10			/**< Set shiftmode */
#define LCD_DISPLAYSHIFT	LCD_SHIFTMODE | 0x08	/**<	Display shift */
#define LCD_CURSORMOVE		LCD_SHIFTMODE | 0x00	/**<	Cursor move */
#define LCD_RIGHT		LCD_SHIFTMODE | 0x04	/**<	Right shift */
#define LCD_LEFT		LCD_SHIFTMODE | 0x00	/**<	Left shift */
/*@}*/

/** @name DISPLAY_CONFIGURATION */
/*@{*/
#define LCD_CONFIGURATION		0x20				/**< Set function */
#define LCD_8BIT		LCD_CONFIGURATION | 0x10	/**<	8 bits interface */
#define LCD_4BIT		LCD_CONFIGURATION | 0x00	/**<	4 bits interface */
#define LCD_2LINE		LCD_CONFIGURATION | 0x08	/**<	2 line display */
#define LCD_1LINE		LCD_CONFIGURATION | 0x00	/**<	1 line display */
#define LCD_5X10		LCD_CONFIGURATION | 0x04	/**<	5 X 10 dots */
#define LCD_5X7			LCD_CONFIGURATION | 0x00	/**<	5 X 7 dots */

#define LCD_SETCGRAMADDR 0x40

#define lcdPutChar(value) lcdWriteByte(value,1)
#define lcdCommand(value) lcdWriteByte(value,0)

static const uint8_t LCDLineOffsets[] PROGMEM = UI_LINE_OFFSETS;
static const char versionString[] PROGMEM = UI_VERSION_STRING;


#if UI_DISPLAY_TYPE==3

// ============= I2C LCD Display driver ================
inline void lcdStartWrite()
{
    HAL::i2cStartWait(UI_DISPLAY_I2C_ADDRESS+I2C_WRITE);
#if UI_DISPLAY_I2C_CHIPTYPE==1
    HAL::i2cWrite( 0x14); // Start at port a
#endif
}
inline void lcdStopWrite()
{
    HAL::i2cStop();
}
void lcdWriteNibble(uint8_t value)
{
#if UI_DISPLAY_I2C_CHIPTYPE==0
    value|=uid.outputMask;
#if UI_DISPLAY_D4_PIN==1 && UI_DISPLAY_D5_PIN==2 && UI_DISPLAY_D6_PIN==4 && UI_DISPLAY_D7_PIN==8
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
#else
    uint8_t v=(value & 1?UI_DISPLAY_D4_PIN:0)|(value & 2?UI_DISPLAY_D5_PIN:0)|(value & 4?UI_DISPLAY_D6_PIN:0)|(value & 8?UI_DISPLAY_D7_PIN:0);
    HAL::i2cWrite((v) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(v);
#
#endif
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==1
    unsigned int v=(value & 1?UI_DISPLAY_D4_PIN:0)|(value & 2?UI_DISPLAY_D5_PIN:0)|(value & 4?UI_DISPLAY_D6_PIN:0)|(value & 8?UI_DISPLAY_D7_PIN:0) | uid.outputMask;
    unsigned int v2 = v | UI_DISPLAY_ENABLE_PIN;
    HAL::i2cWrite(v2 & 255);
    HAL::i2cWrite(v2 >> 8);
    HAL::i2cWrite(v & 255);
    HAL::i2cWrite(v >> 8);
#endif
}
void lcdWriteByte(uint8_t c,uint8_t rs)
{
#if UI_DISPLAY_I2C_CHIPTYPE==0
    uint8_t mod = (rs?UI_DISPLAY_RS_PIN:0) | uid.outputMask; // | (UI_DISPLAY_RW_PIN);
#if UI_DISPLAY_D4_PIN==1 && UI_DISPLAY_D5_PIN==2 && UI_DISPLAY_D6_PIN==4 && UI_DISPLAY_D7_PIN==8
    uint8_t value = (c >> 4) | mod;
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
    value = (c & 15) | mod;
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
#else
    uint8_t value = (c & 16?UI_DISPLAY_D4_PIN:0)|(c & 32?UI_DISPLAY_D5_PIN:0)|(c & 64?UI_DISPLAY_D6_PIN:0)|(c & 128?UI_DISPLAY_D7_PIN:0) | mod;
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
    value = (c & 1?UI_DISPLAY_D4_PIN:0)|(c & 2?UI_DISPLAY_D5_PIN:0)|(c & 4?UI_DISPLAY_D6_PIN:0)|(c & 8?UI_DISPLAY_D7_PIN:0) | mod;
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
#endif
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==1
    unsigned int mod = (rs?UI_DISPLAY_RS_PIN:0) | uid.outputMask; // | (UI_DISPLAY_RW_PIN);
    unsigned int value = (c & 16?UI_DISPLAY_D4_PIN:0)|(c & 32?UI_DISPLAY_D5_PIN:0)|(c & 64?UI_DISPLAY_D6_PIN:0)|(c & 128?UI_DISPLAY_D7_PIN:0) | mod;
    unsigned int value2 = (value) | UI_DISPLAY_ENABLE_PIN;
    HAL::i2cWrite(value2 & 255);
    HAL::i2cWrite(value2 >>8);
    HAL::i2cWrite(value & 255);
    HAL::i2cWrite(value>>8);
    value = (c & 1?UI_DISPLAY_D4_PIN:0)|(c & 2?UI_DISPLAY_D5_PIN:0)|(c & 4?UI_DISPLAY_D6_PIN:0)|(c & 8?UI_DISPLAY_D7_PIN:0) | mod;
    value2 = (value) | UI_DISPLAY_ENABLE_PIN;
    HAL::i2cWrite(value2 & 255);
    HAL::i2cWrite(value2 >>8);
    HAL::i2cWrite(value & 255);
    HAL::i2cWrite(value>>8);
#endif
}
void initializeLCD()
{
    HAL::delayMilliseconds(400);
    lcdStartWrite();
    HAL::i2cWrite(uid.outputMask & 255);
#if UI_DISPLAY_I2C_CHIPTYPE==1
    HAL::i2cWrite(uid.outputMask >> 8);
#endif
    HAL::delayMicroseconds(20);
    lcdWriteNibble(0x03);
    HAL::delayMicroseconds(6000); // I have one LCD for which 4500 here was not long enough.
    // second try
    lcdWriteNibble(0x03);
    HAL::delayMicroseconds(180); // wait
    // third go!
    lcdWriteNibble(0x03);
    HAL::delayMicroseconds(180);
    // finally, set to 4-bit interface
    lcdWriteNibble(0x02);
    HAL::delayMicroseconds(180);
    // finally, set # lines, font size, etc.
    lcdCommand(LCD_4BIT | LCD_2LINE | LCD_5X7);
    lcdCommand(LCD_CLEAR);					//-	Clear Screen
    HAL::delayMilliseconds(2); // clear is slow operation
    lcdCommand(LCD_INCREASE | LCD_DISPLAYSHIFTOFF);	//-	Entrymode (Display Shift: off, Increment Address Counter)
    lcdCommand(LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKINGOFF);	//-	Display on
    uid.lastSwitch = uid.lastRefresh = HAL::timeInMilliseconds();
    uid.createChar(1,character_back);
    uid.createChar(2,character_degree);
    uid.createChar(3,character_selected);
    uid.createChar(4,character_unselected);
    uid.createChar(5,character_temperature);
    uid.createChar(6,character_folder);
    //uid.createChar(7,character_ready);
    uid.createChar(7,character_bed);
    lcdStopWrite();
}
#endif
#if UI_DISPLAY_TYPE==1 || UI_DISPLAY_TYPE==2

void lcdWriteNibble(uint8_t value)
{
    WRITE(UI_DISPLAY_D4_PIN,value & 1);
    WRITE(UI_DISPLAY_D5_PIN,value & 2);
    WRITE(UI_DISPLAY_D6_PIN,value & 4);
    WRITE(UI_DISPLAY_D7_PIN,value & 8);
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);// enable pulse must be >450ns
#if CPU_ARCH == ARCH_AVR
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
    HAL::delayMicroseconds(1);
#endif
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
#if CPU_ARCH == ARCH_AVR
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
    HAL::delayMicroseconds(1);
#endif
}
void lcdWriteByte(uint8_t c,uint8_t rs)
{
#if UI_DISPLAY_RW_PIN<0
    HAL::delayMicroseconds(UI_DELAYPERCHAR);
#else
    SET_INPUT(UI_DISPLAY_D4_PIN);
    SET_INPUT(UI_DISPLAY_D5_PIN);
    SET_INPUT(UI_DISPLAY_D6_PIN);
    SET_INPUT(UI_DISPLAY_D7_PIN);
    WRITE(UI_DISPLAY_RW_PIN, HIGH);
    WRITE(UI_DISPLAY_RS_PIN, LOW);
    uint8_t busy;
    do
    {
        WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);
#if CPU_ARCH == ARCH_AVR
        __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
    HAL::delayMicroseconds(1);
#endif
        busy = READ(UI_DISPLAY_D7_PIN);
        WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
#if CPU_ARCH == ARCH_AVR
        __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
        HAL::delayMicroseconds(1);
#endif
        WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);
#if CPU_ARCH == ARCH_AVR
        __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
    HAL::delayMicroseconds(1);
#endif
        WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
#if CPU_ARCH == ARCH_AVR
        __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
        HAL::delayMicroseconds(1);
#endif
    }
    while (busy);
    SET_OUTPUT(UI_DISPLAY_D4_PIN);
    SET_OUTPUT(UI_DISPLAY_D5_PIN);
    SET_OUTPUT(UI_DISPLAY_D6_PIN);
    SET_OUTPUT(UI_DISPLAY_D7_PIN);
    WRITE(UI_DISPLAY_RW_PIN, LOW);
#endif
    WRITE(UI_DISPLAY_RS_PIN, rs);

    WRITE(UI_DISPLAY_D4_PIN, c & 0x10);
    WRITE(UI_DISPLAY_D5_PIN, c & 0x20);
    WRITE(UI_DISPLAY_D6_PIN, c & 0x40);
    WRITE(UI_DISPLAY_D7_PIN, c & 0x80);
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);   // enable pulse must be >450ns
#if CPU_ARCH == ARCH_AVR
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
    HAL::delayMicroseconds(1);
#endif
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
#if CPU_ARCH == ARCH_AVR
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
    HAL::delayMicroseconds(1);
#endif

    WRITE(UI_DISPLAY_D4_PIN, c & 0x01);
    WRITE(UI_DISPLAY_D5_PIN, c & 0x02);
    WRITE(UI_DISPLAY_D6_PIN, c & 0x04);
    WRITE(UI_DISPLAY_D7_PIN, c & 0x08);
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);   // enable pulse must be >450ns
#if CPU_ARCH == ARCH_AVR
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
    HAL::delayMicroseconds(1);
#endif
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
#if CPU_ARCH == ARCH_AVR
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
#else
    HAL::delayMicroseconds(1);
#endif
}
void initializeLCD()
{
    playsound(5000,240);
    playsound(3000,240);
    // SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
    // according to datasheet, we need at least 40ms after power rises above 2.7V
    // before sending commands. Arduino can turn on way before 4.5V.
    // is this delay long enough for all cases??
    HAL::delayMilliseconds(400);
    SET_OUTPUT(UI_DISPLAY_D4_PIN);
    SET_OUTPUT(UI_DISPLAY_D5_PIN);
    SET_OUTPUT(UI_DISPLAY_D6_PIN);
    SET_OUTPUT(UI_DISPLAY_D7_PIN);
    SET_OUTPUT(UI_DISPLAY_RS_PIN);
#if UI_DISPLAY_RW_PIN>-1
    SET_OUTPUT(UI_DISPLAY_RW_PIN);
#endif
    SET_OUTPUT(UI_DISPLAY_ENABLE_PIN);

    // Now we pull both RS and R/W low to begin commands
    WRITE(UI_DISPLAY_RS_PIN, LOW);
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
     HAL::delayMilliseconds(40);
    //put the LCD into 4 bit mode
    // this is according to the hitachi HD44780 datasheet
    // figure 24, pg 46

    // we start in 8bit mode, try to set 4 bit mode
    // at this point we are in 8 bit mode but of course in this
    // interface 4 pins are dangling unconnected and the values
    // on them don't matter for these instructions.
     WRITE(UI_DISPLAY_RS_PIN, LOW);
    HAL::delayMilliseconds(40);
    lcdWriteNibble(0x03);
   HAL::delayMilliseconds(40); // I have one LCD for which 4500 here was not long enough.
    // second try
    lcdWriteNibble(0x03);
   HAL::delayMilliseconds(40); // wait
    // third go!
    lcdWriteNibble(0x03);
    HAL::delayMilliseconds(40);
    // finally, set to 4-bit interface
    lcdWriteNibble(0x02);
    HAL::delayMilliseconds(40);
    // finally, set # lines, font size, etc.
    lcdCommand(LCD_4BIT | LCD_2LINE | LCD_5X7);
	HAL::delayMilliseconds(40);
    lcdCommand(LCD_CLEAR);					//-	Clear Screen
    HAL::delayMilliseconds(40); // clear is slow operation
    lcdCommand(LCD_INCREASE | LCD_DISPLAYSHIFTOFF);	//-	Entrymode (Display Shift: off, Increment Address Counter)
    HAL::delayMilliseconds(40);
    lcdCommand(LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKINGOFF);	//-	Display on
    HAL::delayMilliseconds(40);
    uid.lastSwitch = uid.lastRefresh = HAL::timeInMilliseconds();
    uid.createChar(1,character_back);
    uid.createChar(2,character_degree);
    uid.createChar(3,character_selected);
    uid.createChar(4,character_unselected);
    uid.createChar(5,character_temperature);
    uid.createChar(6,character_folder);
    //uid.createChar(7,character_ready);
    uid.createChar(7,character_bed);
    
#if defined(UI_BACKLIGHT_PIN)
    SET_OUTPUT(UI_BACKLIGHT_PIN);
    WRITE(UI_BACKLIGHT_PIN, HIGH);
#endif
}
// ----------- end direct LCD driver
#endif
#if UI_DISPLAY_TYPE<4
void UIDisplay::printRow(uint8_t r,char *txt,char *txt2,uint8_t changeAtCol)
{
    changeAtCol = RMath::min(UI_COLS,changeAtCol);
    uint8_t col=0;
// Set row
    if(r >= UI_ROWS) return;
#if UI_DISPLAY_TYPE==3
    lcdStartWrite();
#endif
    lcdWriteByte(128 + HAL::readFlashByte((const char *)&LCDLineOffsets[r]),0); // Position cursor
    char c;
    while((c=*txt) != 0x00 && col<changeAtCol)
    {
        txt++;
        lcdPutChar(c);
        col++;
    }
    while(col<changeAtCol)
    {
        lcdPutChar(' ');
        col++;
    }
    if(txt2!=NULL)
    {
        while((c=*txt2) != 0x00 && col<UI_COLS)
        {
            txt2++;
            lcdPutChar(c);
            col++;
        }
        while(col<UI_COLS)
        {
            lcdPutChar(' ');
            col++;
        }
    }
#if UI_DISPLAY_TYPE==3
    lcdStopWrite();
#endif
#if UI_HAS_KEYS==1 && UI_HAS_I2C_ENCODER>0
    ui_check_slow_encoder();
#endif
}
#endif

#if UI_DISPLAY_TYPE==4
// Use LiquidCrystal library instead
#include <LiquidCrystal.h>

LiquidCrystal lcd(UI_DISPLAY_RS_PIN, UI_DISPLAY_RW_PIN,UI_DISPLAY_ENABLE_PIN,UI_DISPLAY_D4_PIN,UI_DISPLAY_D5_PIN,UI_DISPLAY_D6_PIN,UI_DISPLAY_D7_PIN);

void UIDisplay::createChar(uint8_t location,const uint8_t charmap[])
{
    location &= 0x7; // we only have 8 locations 0-7
    uint8_t data[8];
    for (int i=0; i<8; i++)
    {
        data[i]=pgm_read_byte(&(charmap[i]));
    }
    lcd.createChar(location, data);
}
void UIDisplay::printRow(uint8_t r,char *txt,char *txt2,uint8_t changeAtCol)
{
    changeAtCol = RMath::min(UI_COLS,changeAtCol);
    uint8_t col=0;
// Set row
    if(r >= UI_ROWS) return;
    lcd.setCursor(0,r);
    char c;
    while((c=*txt) != 0x00 && col<changeAtCol)
    {
        txt++;
        lcd.write(c);
        col++;
    }
    while(col<changeAtCol)
    {
        lcd.write(' ');
        col++;
    }
    if(txt2!=NULL)
    {
        while((c=*txt2) != 0x00 && col<UI_COLS)
        {
            txt2++;
            lcd.write(c);
            col++;
        }
        while(col<UI_COLS)
        {
            lcd.write(' ');
            col++;
        }
    }
#if UI_HAS_KEYS==1 && UI_HAS_I2C_ENCODER>0
    ui_check_slow_encoder();
#endif
}

void initializeLCD()
{
    lcd.begin(UI_COLS,UI_ROWS);
    uid.lastSwitch = uid.lastRefresh = HAL::timeInMilliseconds();
    uid.createChar(1,character_back);
    uid.createChar(2,character_degree);
    uid.createChar(3,character_selected);
    uid.createChar(4,character_unselected);
}
// ------------------ End LiquidCrystal library as LCD driver
#endif // UI_DISPLAY_TYPE==4

#if UI_DISPLAY_TYPE==5
//u8glib
#ifdef U8GLIB_ST7920
#define UI_SPI_SCK UI_DISPLAY_D4_PIN
#define UI_SPI_MOSI UI_DISPLAY_ENABLE_PIN
#define UI_SPI_CS UI_DISPLAY_RS_PIN
#endif
#include "u8glib_ex.h"
u8g_t u8g;
u8g_uint_t u8_tx = 0, u8_ty = 0;

void u8PrintChar(char c)
{
    switch(c)
    {
    case 0x7E: // right arrow
        u8g_SetFont(&u8g, u8g_font_6x12_67_75);
        u8_tx += u8g_DrawGlyph(&u8g, u8_tx, u8_ty, 0x52);
        u8g_SetFont(&u8g, UI_FONT_DEFAULT);
        break;
    case CHAR_SELECTOR:
        u8g_SetFont(&u8g, u8g_font_6x12_67_75);
        u8_tx += u8g_DrawGlyph(&u8g, u8_tx, u8_ty, 0xb7);
        u8g_SetFont(&u8g, UI_FONT_DEFAULT);
        break;
    case CHAR_SELECTED:
        u8g_SetFont(&u8g, u8g_font_6x12_67_75);
        u8_tx += u8g_DrawGlyph(&u8g, u8_tx, u8_ty, 0xb6);
        u8g_SetFont(&u8g, UI_FONT_DEFAULT);
        break;
    default:
        u8_tx += u8g_DrawGlyph(&u8g, u8_tx, u8_ty, c);
    }
}
void printU8GRow(uint8_t x,uint8_t y,char *text)
{
    char c;
    while((c = *(text++)) != 0)
        x += u8g_DrawGlyph(&u8g,x,y,c);
}
void UIDisplay::printRow(uint8_t r,char *txt,char *txt2,uint8_t changeAtCol)
{
    changeAtCol = RMath::min(UI_COLS,changeAtCol);
    uint8_t col=0;
// Set row
    if(r >= UI_ROWS) return;
    int y = r*UI_FONT_HEIGHT;
    if(!u8g_IsBBXIntersection(&u8g,0,y,UI_LCD_WIDTH,UI_FONT_HEIGHT+2)) return; // row not visible
    u8_tx = 0;
    u8_ty = y+UI_FONT_HEIGHT; //set position
    bool highlight = ((uint8_t)(*txt) == CHAR_SELECTOR) || ((uint8_t)(*txt) == CHAR_SELECTED);
    if(highlight)
    {
        u8g_SetColorIndex(&u8g,1);
        u8g_draw_box(&u8g, 0, y+1, u8g_GetWidth(&u8g), UI_FONT_HEIGHT+1);
        u8g_SetColorIndex(&u8g,0);
    }
    char c;
    while((c = *(txt++)) != 0 && col < changeAtCol)
    {
        u8PrintChar(c);
        col++;
    }
    if(txt2 != NULL)
    {
        col = changeAtCol;
        u8_tx = col*UI_FONT_WIDTH; //set position
        while((c=*(txt2++)) != 0 && col < UI_COLS)
        {
            u8PrintChar(c);
            col++;
        }
    }
    if(highlight)
    {
        u8g_SetColorIndex(&u8g,1);
    }

#if UI_HAS_KEYS==1 && UI_HAS_I2C_ENCODER>0
    ui_check_slow_encoder();
#endif
}

void initializeLCD()
{
#ifdef U8GLIB_ST7920
//U8GLIB_ST7920_128X64_1X u8g(UI_DISPLAY_D4_PIN, UI_DISPLAY_ENABLE_PIN, UI_DISPLAY_RS_PIN);
    u8g_InitSPI(&u8g,&u8g_dev_st7920_128x64_sw_spi,  UI_DISPLAY_D4_PIN, UI_DISPLAY_ENABLE_PIN, UI_DISPLAY_RS_PIN, U8G_PIN_NONE, U8G_PIN_NONE);
#endif
    u8g_Begin(&u8g);
    //u8g.firstPage();
    u8g_FirstPage(&u8g);
    do
    {
        u8g_SetColorIndex(&u8g, 0);
    }
    while( u8g_NextPage(&u8g) );

    u8g_SetFont(&u8g, UI_FONT_DEFAULT);
    u8g_SetColorIndex(&u8g, 1);
    uid.lastSwitch = uid.lastRefresh = HAL::timeInMilliseconds();
}
// ------------------ End u8GLIB library as LCD driver
#endif // UI_DISPLAY_TYPE==5

char printCols[MAX_COLS+1];
UIDisplay::UIDisplay()
{

}
#if UI_ANIMATION
void slideIn(uint8_t row,FSTRINGPARAM(text))
{
    char *empty="";
    int8_t i = 0;
    uid.col=0;
    uid.addStringP(text);
    printCols[uid.col]=0;
    for(i=UI_COLS-1; i>=0; i--)
    {
        uid.printRow(row,empty,printCols,i);
        HAL::pingWatchdog();
        HAL::delayMilliseconds(10);
    }
}
#endif // UI_ANIMATION
void UIDisplay::initialize()
{
    oldMenuLevel = -2;
#ifdef COMPILE_I2C_DRIVER
    uid.outputMask = UI_DISPLAY_I2C_OUTPUT_START_MASK;
#if UI_DISPLAY_I2C_CHIPTYPE==0 && BEEPER_TYPE==2 && BEEPER_PIN>=0
#if BEEPER_ADDRESS == UI_DISPLAY_I2C_ADDRESS
    uid.outputMask |= BEEPER_PIN;
#endif
#endif
    HAL::i2cInit(UI_I2C_CLOCKSPEED);
#if UI_DISPLAY_I2C_CHIPTYPE==1
    // set direction of pins
    HAL::i2cStart(UI_DISPLAY_I2C_ADDRESS+I2C_WRITE);
    HAL::i2cWrite(0); // IODIRA
    HAL::i2cWrite(~(UI_DISPLAY_I2C_OUTPUT_PINS & 255));
    HAL::i2cWrite(~(UI_DISPLAY_I2C_OUTPUT_PINS >> 8));
    HAL::i2cStop();
    // Set pullups according to  UI_DISPLAY_I2C_PULLUP
    HAL::i2cStart(UI_DISPLAY_I2C_ADDRESS+I2C_WRITE);
    HAL::i2cWrite(0x0C); // GPPUA
    HAL::i2cWrite(UI_DISPLAY_I2C_PULLUP & 255);
    HAL::i2cWrite(UI_DISPLAY_I2C_PULLUP >> 8);
    HAL::i2cStop();
#endif

#endif
    flags = 0;
    menuLevel = 0;
    shift = -2;
    menuPos[0] = 0;
    lastAction = 0;
    lastButtonAction = 0;
    activeAction = 0;
    statusMsg[0] = 0;
    ui_init_keys();
    cwd[0]='/';
    cwd[1]=0;
    folderLevel=0;
    UI_STATUS(UI_TEXT_PRINTER_READY);
#if UI_DISPLAY_TYPE>0
    initializeLCD();
#if UI_DISPLAY_TYPE==3
    // I don't know why but after power up the lcd does not come up
    // but if I reinitialize i2c and the lcd again here it works.
    HAL::delayMilliseconds(10);
    HAL::i2cInit(UI_I2C_CLOCKSPEED);
        // set direction of pins
    HAL::i2cStart(UI_DISPLAY_I2C_ADDRESS+I2C_WRITE);
    HAL::i2cWrite(0); // IODIRA
    HAL::i2cWrite(~(UI_DISPLAY_I2C_OUTPUT_PINS & 255));
    HAL::i2cWrite(~(UI_DISPLAY_I2C_OUTPUT_PINS >> 8));
    HAL::i2cStop();
    // Set pullups according to  UI_DISPLAY_I2C_PULLUP
    HAL::i2cStart(UI_DISPLAY_I2C_ADDRESS+I2C_WRITE);
    HAL::i2cWrite(0x0C); // GPPUA
    HAL::i2cWrite(UI_DISPLAY_I2C_PULLUP & 255);
    HAL::i2cWrite(UI_DISPLAY_I2C_PULLUP >> 8);
    HAL::i2cStop();
    initializeLCD();
#endif
#if UI_ANIMATION==false || UI_DISPLAY_TYPE==5
#if UI_DISPLAY_TYPE == 5
    //u8g picture loop
    u8g_FirstPage(&u8g);
    do
    {
#endif
        for(uint8_t y=0; y<UI_ROWS; y++) displayCache[y][0] = 0;
        printRowP(0, PSTR(UI_PRINTER_NAME));
        printRowP(1, "");
#if UI_ROWS>2
        printRowP(2, versionString);
        printRowP(3, "");
#endif
#if UI_DISPLAY_TYPE == 5
    }
    while( u8g_NextPage(&u8g) );  //end picture loop
#endif
#else
    slideIn(0, PSTR(UI_PRINTER_NAME));
    strcpy(displayCache[0], printCols);
    slideIn(1, versionString);
    strcpy(displayCache[1], printCols);
    
#if UI_ROWS>2
	slideIn(2, versionString2);
    strcpy(displayCache[2], printCols);
    slideIn(UI_ROWS-1, PSTR(UI_PRINTER_COMPANY));
    strcpy(displayCache[UI_ROWS-1], printCols);
#endif
#endif
    HAL::delayMilliseconds(UI_START_SCREEN_DELAY);
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==0 && (BEEPER_TYPE==2 || defined(UI_HAS_I2C_KEYS))
    // Make sure the beeper is off
    HAL::i2cStartWait(UI_I2C_KEY_ADDRESS+I2C_WRITE);
    HAL::i2cWrite(255); // Disable beeper, enable read for other pins.
    HAL::i2cStop();
#endif
}
#if UI_DISPLAY_TYPE==1 || UI_DISPLAY_TYPE==2 || UI_DISPLAY_TYPE==3
void UIDisplay::createChar(uint8_t location,const uint8_t PROGMEM charmap[])
{
    location &= 0x7; // we only have 8 locations 0-7
    lcdCommand(LCD_SETCGRAMADDR | (location << 3));
    for (int i=0; i<8; i++)
    {
        lcdPutChar(pgm_read_byte(&(charmap[i])));
    }
}
#endif
void  UIDisplay::waitForKey()
{
    int nextAction = 0;

    lastButtonAction = 0;
    while(lastButtonAction==nextAction)
    {
        ui_check_slow_keys(nextAction);
    }
}

void UIDisplay::printRowP(uint8_t r,PGM_P txt)
{
    if(r >= UI_ROWS) return;
    col=0;
    addStringP(txt);
    printCols[col]=0;
    printRow(r,printCols,NULL,UI_COLS);
}
void UIDisplay::addInt(int value,uint8_t digits,char fillChar)
{
    uint8_t dig=0,neg=0;
    if(value<0)
    {
        value = -value;
        neg=1;
        dig++;
    }
    char buf[7]; // Assumes 8-bit chars plus zero byte.
    char *str = &buf[6];
    buf[6]=0;
    do
    {
        unsigned int m = value;
        value /= 10;
        char c = m - 10 * value;
        *--str = c + '0';
        dig++;
    }
    while(value);
    if(neg)
        printCols[col++]='-';
    if(digits<6)
        while(dig<digits)
        {
            *--str = fillChar; //' ';
            dig++;
        }
    while(*str && col<MAX_COLS)
    {
        printCols[col++] = *str;
        str++;
    }
}
void UIDisplay::addLong(long value,char digits)
{
    uint8_t dig = 0,neg=0;
    if(value<0)
    {
        neg=1;
        value = -value;
        dig++;
    }
    char buf[13]; // Assumes 8-bit chars plus zero byte.
    char *str = &buf[12];
    buf[12]=0;
    do
    {
        unsigned long m = value;
        value /= 10;
        char c = m - 10 * value;
        *--str = c + '0';
        dig++;
    }
    while(value);
    if(neg)
        printCols[col++]='-';
    if(digits<=11)
        while(dig<digits)
        {
            *--str = ' ';
            dig++;
        }
    while(*str && col<MAX_COLS)
    {
        printCols[col++] = *str;
        str++;
    }
}
const float roundingTable[] PROGMEM = {0.5,0.05,0.005,0.0005};
void UIDisplay::addFloat(float number, char fixdigits,uint8_t digits)
{
    // Handle negative numbers
    if (number < 0.0)
    {
        printCols[col++]='-';
        if(col>=MAX_COLS) return;
        number = -number;
        fixdigits--;
    }
    number += pgm_read_float(&roundingTable[digits]); // for correct rounding

    // Extract the integer part of the number and print it
    unsigned long int_part = (unsigned long)number;
    float remainder = number - (float)int_part;
    addLong(int_part,fixdigits);
    if(col>=UI_COLS) return;

    // Print the decimal point, but only if there are digits beyond
    if (digits > 0)
    {
        printCols[col++]='.';
    }

    // Extract digits from the remainder one at a time
    while (col<MAX_COLS && digits-- > 0)
    {
        remainder *= 10.0;
        uint8_t toPrint = uint8_t(remainder);
        printCols[col++] = '0'+toPrint;
        remainder -= toPrint;
    }
}
void UIDisplay::addStringP(FSTRINGPARAM(text))
{
    while(col<MAX_COLS)
    {
        uint8_t c = HAL::readFlashByte(text++);
        if(c==0) return;
        printCols[col++]=c;
    }
}

UI_STRING(ui_text_on,UI_TEXT_ON);
UI_STRING(ui_text_off,UI_TEXT_OFF);
UI_STRING(ui_text_na,UI_TEXT_NA);
UI_STRING(ui_yes,UI_TEXT_YES);
UI_STRING(ui_no,UI_TEXT_NO);
UI_STRING(ui_print_pos,UI_TEXT_PRINT_POS);
UI_STRING(ui_selected,UI_TEXT_SEL);
UI_STRING(ui_unselected,UI_TEXT_NOSEL);
UI_STRING(ui_action,UI_TEXT_STRING_ACTION);

void UIDisplay::parse(const char *txt,bool ram)
{
    int ivalue=0;
    float fvalue=0;
    while(col<MAX_COLS)
    {
        char c=(ram ? *(txt++) : pgm_read_byte(txt++));
        if(c==0) break; // finished
        if(c!='%')
        {
            printCols[col++]=c;
            continue;
        }
        // dynamic parameter, parse meaning and replace
        char c1=(ram ? *(txt++) : pgm_read_byte(txt++));
        char c2=(ram ? *(txt++) : pgm_read_byte(txt++));
        switch(c1)
        {
        case '%':
            if(c2=='%' && col<MAX_COLS)
                printCols[col++]='%';
            break;
        case 'a': // Acceleration settings
            if(c2=='x') addFloat(Printer::maxAccelerationMMPerSquareSecond[X_AXIS],5,0);
            else if(c2=='y') addFloat(Printer::maxAccelerationMMPerSquareSecond[Y_AXIS],5,0);
            else if(c2=='z') addFloat(Printer::maxAccelerationMMPerSquareSecond[Z_AXIS],5,0);
            else if(c2=='X') addFloat(Printer::maxTravelAccelerationMMPerSquareSecond[X_AXIS],5,0);
            else if(c2=='Y') addFloat(Printer::maxTravelAccelerationMMPerSquareSecond[Y_AXIS],5,0);
            else if(c2=='Z') addFloat(Printer::maxTravelAccelerationMMPerSquareSecond[Z_AXIS],5,0);
            else if(c2=='j') addFloat(Printer::maxJerk,3,1);
#if DRIVE_SYSTEM!=3
            else if(c2=='J') addFloat(Printer::maxZJerk,3,1);
#endif
            break;
        case 'A':
            if(c2=='m') addStringP(Printer:: isAutolevelActive()?ui_text_on:ui_text_off);
        break;
        case 'B':
        if(c2=='1') //heat PLA
                {
                        bool allheat=true;
                        if(extruder[0].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
                       #if NUM_EXTRUDER>1
                        if(extruder[1].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
                        #endif
                       #if NUM_EXTRUDER>2
                        if(extruder[2].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
                        #endif
                        #if HAVE_HEATED_BED==true
                        if(heatedBedController.targetTemperatureC!=EEPROM::ftemp_bed_pla)allheat=false;
                        #endif
                        addStringP(allheat?"\003":"\004");
                }
        else if(c2=='2') //heat ABS
                {
                    bool allheat=true;
                    if(extruder[0].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
                   #if NUM_EXTRUDER>1
                    if(extruder[1].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
                    #endif
                   #if NUM_EXTRUDER>2
                    if(extruder[2].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
                    #endif
                    #if HAVE_HEATED_BED==true
                    if(heatedBedController.targetTemperatureC!=EEPROM::ftemp_bed_abs)allheat=false;
                    #endif
                    addStringP(allheat?"\003":"\004");
                }
          else if(c2=='3') //Cooldown
                {
                     bool alloff=true;
                     if(extruder[0].tempControl.targetTemperatureC>0)alloff=false;
#if NUM_EXTRUDER>1
                    if(extruder[1].tempControl.targetTemperatureC>0)alloff=false;
#endif
#if NUM_EXTRUDER>2
                     if(extruder[2].tempControl.targetTemperatureC>0)alloff=false;
#endif
#if HAVE_HEATED_BED==true
                    if (heatedBedController.targetTemperatureC>0)alloff=false;
#endif
               addStringP(alloff?"\003":"\004");
                }
            else if(c2=='4') //Extruder 1 Off
                {
                    addStringP(extruder[0].tempControl.targetTemperatureC>0?"\004":"\003");
                }
            else if(c2=='5') //Extruder 2 Off
                {
                addStringP(extruder[1].tempControl.targetTemperatureC>0?"\004":"\003");
                }
             else if(c2=='6') //Extruder 3 Off
                {
                addStringP(extruder[2].tempControl.targetTemperatureC>0?"\004":"\003");
                }
             else if(c2=='7') //Bed Off
                {
                addStringP(heatedBedController.targetTemperatureC>0?"\004":"\003");
                }
        break;
	    case 'C':
            if(c2=='1')
                {
                addStringP(uipagedialog[0]);
                }
            else if(c2=='2')
                {
                addStringP(uipagedialog[1]);
                }
            else if(c2=='3')
                {
                addStringP(uipagedialog[2]);
                }
            else if(c2=='4')
                {
                addStringP(uipagedialog[3]);
                }
	    break;
        case 'd':
            if(c2=='o') addStringP(Printer::debugEcho()?ui_text_on:ui_text_off);
            else if(c2=='i') addStringP(Printer::debugInfo()?ui_text_on:ui_text_off);
            else if(c2=='e') addStringP(Printer::debugErrors()?ui_text_on:ui_text_off);
            else if(c2=='d') addStringP(Printer::debugDryrun()?ui_text_on:ui_text_off);
            break;

        case 'e': // Extruder temperature
            if(c2=='r')   // Extruder relative mode
            {
                addStringP(Printer::relativeExtruderCoordinateMode?ui_yes:ui_no);
                break;
            }
            ivalue = UI_TEMP_PRECISION;
            if(Printer::flag0 & PRINTER_FLAG0_TEMPSENSOR_DEFECT)
            {
                addStringP(PSTR(" def "));
                break;
            }
            if(c2=='c') fvalue=Extruder::current->tempControl.currentTemperatureC;
            else if(c2>='0' && c2<='9') fvalue=extruder[c2-'0'].tempControl.currentTemperatureC;
            else if(c2=='b') fvalue=Extruder::getHeatedBedTemperature();
            else if(c2=='B')
            {
                ivalue=0;
                fvalue=Extruder::getHeatedBedTemperature();
            }
            addFloat(fvalue,3,ivalue);
            break;
        case 'E': // Target extruder temperature
            if(c2=='c') fvalue=Extruder::current->tempControl.targetTemperatureC;
            else if(c2>='0' && c2<='9') fvalue=extruder[c2-'0'].tempControl.targetTemperatureC;
#if HAVE_HEATED_BED
            else if(c2=='b') fvalue=heatedBedController.targetTemperatureC;
#endif
            addFloat(fvalue,3,0 /*UI_TEMP_PRECISION*/);
            break;
#if FAN_PIN > -1
        case 'F': // FAN speed
            if(c2=='s') addInt(Printer::getFanSpeed()*100/255,3);
            break;
#endif
        case 'f':
            if(c2=='x') addFloat(Printer::maxFeedrate[0],5,0);
            else if(c2=='y') addFloat(Printer::maxFeedrate[1],5,0);
            else if(c2=='z') addFloat(Printer::maxFeedrate[2],5,0);
            else if(c2=='X') addFloat(Printer::homingFeedrate[0],5,0);
            else if(c2=='Y') addFloat(Printer::homingFeedrate[1],5,0);
            else if(c2=='Z') addFloat(Printer::homingFeedrate[2],5,0);
            break;
        case 'i':
            if(c2=='s') addLong(stepperInactiveTime/1000,4);
            else if(c2=='p') addLong(maxInactiveTime/1000,4);
            else if(c2=='l') addLong(EEPROM::timepowersaving/1000,4);
            break;
        case 'O': // ops related stuff
            break;
         case 'L':
			 if(c2=='x') addFloat(Printer::xLength,4,0);
			 else if(c2=='y') addFloat(Printer::yLength,4,0);
			 else if(c2=='z') addFloat(Printer::zLength,4,0);
         break;
        case 'l':
            if(c2=='a') addInt(lastAction,4);
#if defined(CASE_LIGHTS_PIN) && CASE_LIGHTS_PIN>=0
            else if(c2=='o') addStringP(READ(CASE_LIGHTS_PIN)?ui_text_on:ui_text_off);        // Lights on/off
            else if(c2=='k') addStringP(EEPROM::bkeeplighton?ui_text_on:ui_text_off);        // Keep Lights on/off
#endif
            break;
        case 'o':
            if(c2=='s')
            {
#if SDSUPPORT
                if(sd.sdactive && sd.sdmode)
                {
                    addStringP(PSTR( UI_TEXT_PRINT_POS));
                    unsigned long percent;
                    if(sd.filesize<20000000) percent=sd.sdpos*100/sd.filesize;
                    else percent = (sd.sdpos>>8)*100/(sd.filesize>>8);
                    addInt((int)percent,3);
                    if(col<MAX_COLS)
                        printCols[col++]='%';
                }
                else
#endif
                    parse(statusMsg,true);
                break;
            }
            if(c2=='c')
            {
                addLong(baudrate,6);
                break;
            }
            if(c2=='e')
            {
                if(errorMsg!=0)addStringP((char PROGMEM *)errorMsg);
                break;
            }
            if(c2=='B')
            {
                addInt((int)PrintLine::linesCount,2);
                break;
            }
            if(c2=='f')
            {
                addInt(Printer::extrudeMultiply,3);
                break;
            }
            if(c2=='m')
            {
                addInt(Printer::feedrateMultiply,3);
                break;
            }
            // Extruder output level
            if(c2>='0' && c2<='9') ivalue=pwm_pos[c2-'0'];
#if HAVE_HEATED_BED
            else if(c2=='b') ivalue=pwm_pos[heatedBedController.pwmIndex];
#endif
            else if(c2=='C') ivalue=pwm_pos[Extruder::current->id];
            ivalue=(ivalue*100)/255;
            addInt(ivalue,3);
            if(col<MAX_COLS)
                printCols[col++]='%';
            break;
        case 'M':
			if(c2=='d')
				{
					addStringP((display_mode&ADVANCED_MODE)?UI_TEXT_ADVANCED_MODE:UI_TEXT_EASY_MODE);
				}
			break;
        case 'x':
            if(c2>='0' && c2<='3')
                if(c2=='0')
                    fvalue = Printer::realXPosition();
                else if(c2=='1')
                    fvalue = Printer::realYPosition();
                else if(c2=='2')
                    fvalue = Printer::realZPosition();
                else
                    fvalue = (float)Printer::currentPositionSteps[3]*Printer::invAxisStepsPerMM[3];
            addFloat(fvalue,4,2);
            break;
        case 'y':
#if DRIVE_SYSTEM==3
            if(c2>='0' && c2<='3') fvalue = (float)Printer::currentDeltaPositionSteps[c2-'0']*Printer::invAxisStepsPerMM[c2-'0'];
            addFloat(fvalue,3,2);
#endif
            break;
        case 'X': // Extruder related
#if NUM_EXTRUDER>0
            if(c2>='0' && c2<='9')
            {
                addStringP(Extruder::current->id==c2-'0'?ui_selected:ui_unselected);
            }
#ifdef TEMP_PID
            else if(c2=='i')
            {
                addFloat(Extruder::current->tempControl.pidIGain,4,2);
            }
            else if(c2=='p')
            {
                addFloat(Extruder::current->tempControl.pidPGain,4,2);
            }
            else if(c2=='d')
            {
                addFloat(Extruder::current->tempControl.pidDGain,4,2);
            }
            else if(c2=='m')
            {
                addInt(Extruder::current->tempControl.pidDriveMin,3);
            }
            else if(c2=='M')
            {
                addInt(Extruder::current->tempControl.pidDriveMax,3);
            }
            else if(c2=='D')
            {
                addInt(Extruder::current->tempControl.pidMax,3);
            }
#endif
            else if(c2=='w')
            {
                addInt(Extruder::current->watchPeriod,4);
            }
#if RETRACT_DURING_HEATUP
            else if(c2=='T')
            {
                addInt(Extruder::current->waitRetractTemperature,4);
            }
            else if(c2=='U')
            {
                addInt(Extruder::current->waitRetractUnits,2);
            }
#endif
            else if(c2=='h')
            {
                uint8_t hm = Extruder::current->tempControl.heatManager;
                if(hm == 1)
                    addStringP(PSTR(UI_TEXT_STRING_HM_PID));
                else if(hm == 3)
                    addStringP(PSTR(UI_TEXT_STRING_HM_DEADTIME));
                else if(hm == 2)
                    addStringP(PSTR(UI_TEXT_STRING_HM_SLOWBANG));
                else
                    addStringP(PSTR(UI_TEXT_STRING_HM_BANGBANG));
            }
#ifdef USE_ADVANCE
#ifdef ENABLE_QUADRATIC_ADVANCE
            else if(c2=='a')
            {
                addFloat(Extruder::current->advanceK,3,0);
            }
#endif
            else if(c2=='l')
            {
                addFloat(Extruder::current->advanceL,3,0);
            }
#endif
            else if(c2=='x')
            {
                addFloat(Extruder::current->xOffset,4,2);
            }
            else if(c2=='y')
            {
                addFloat(Extruder::current->yOffset,4,2);
            }
            else if(c2=='f')
            {
                addFloat(Extruder::current->maxStartFeedrate,5,0);
            }
            else if(c2=='F')
            {
                addFloat(Extruder::current->maxFeedrate,5,0);
            }
            else if(c2=='A')
            {
                addFloat(Extruder::current->maxAcceleration,5,0);
            }
#endif
            break;
        case 'T':
			if(c2=='1')addFloat(EEPROM::ftemp_ext_abs,3,0 );
			else if(c2=='2')addFloat(EEPROM::ftemp_ext_pla,3,0 );
			 #if HAVE_HEATED_BED==true
			 else if(c2=='3')addFloat(EEPROM::ftemp_bed_abs,3,0 );
			 else if(c2=='4')addFloat(EEPROM::ftemp_bed_pla,3,0 );
			 #endif
        break;
        case 's': // Endstop positions and sound
			#if FEATURE_BEEPER
		    if(c2=='o')addStringP(HAL::enablesound?ui_text_on:ui_text_off);        // sound on/off
			#endif
			#if defined(FIL_SENSOR1_PIN)
			  if(c2=='f')addStringP(EEPROM::busesensor?ui_text_on:ui_text_off);        //filament sensors on/off
			#endif
            #if defined(TOP_SENSOR_PIN)
			  if(c2=='t')addStringP(EEPROM::btopsensor?ui_text_on:ui_text_off);        //top sensors on/off
			#endif
            if(c2=='x')
            {
#if (X_MIN_PIN > -1) && MIN_HARDWARE_ENDSTOP_X
                addStringP(Printer::isXMinEndstopHit()?ui_text_on:ui_text_off);
#else
                addStringP(ui_text_na);
#endif
            }
            if(c2=='X')
#if (X_MAX_PIN > -1) && MAX_HARDWARE_ENDSTOP_X
                addStringP(Printer::isXMaxEndstopHit()?ui_text_on:ui_text_off);
#else
                addStringP(ui_text_na);
#endif
            if(c2=='y')
#if (Y_MIN_PIN > -1)&& MIN_HARDWARE_ENDSTOP_Y
                addStringP(Printer::isYMinEndstopHit()?ui_text_on:ui_text_off);
#else
                addStringP(ui_text_na);
#endif
            if(c2=='Y')
#if (Y_MAX_PIN > -1) && MAX_HARDWARE_ENDSTOP_Y
                addStringP(Printer::isYMaxEndstopHit()?ui_text_on:ui_text_off);
#else
                addStringP(ui_text_na);
#endif
            if(c2=='z')
#if (Z_MIN_PIN > -1) && MIN_HARDWARE_ENDSTOP_Z
                addStringP(Printer::isZMinEndstopHit()?ui_text_on:ui_text_off);
#else
                addStringP(ui_text_na);
#endif
            if(c2=='Z')
#if (Z_MAX_PIN > -1) && MAX_HARDWARE_ENDSTOP_Z
                addStringP(Printer::isZMaxEndstopHit()?ui_text_on:ui_text_off);
#else
                addStringP(ui_text_na);
#endif
            break;
        case 'S':
            if(c2=='x') addFloat(Printer::axisStepsPerMM[0],3,1);
            if(c2=='y') addFloat(Printer::axisStepsPerMM[1],3,1);
            if(c2=='z') addFloat(Printer::axisStepsPerMM[2],3,1);
            if(c2=='e') addFloat(Extruder::current->stepsPerMM,3,1);
            break;
        case 'N':
			 if(c2=='e')
				{
#if NUM_EXTRUDER>1
					addInt(Extruder::current->id+1,1);
#endif
				}
        break;
        case 'P':
            if(c2=='N') addStringP(PSTR(UI_PRINTER_NAME));
            #if UI_AUTOLIGHTOFF_AFTER > 0
			else if(c2=='s') 
			if((EEPROM::timepowersaving!=maxInactiveTime)||(EEPROM::timepowersaving!=stepperInactiveTime))
					{
					addStringP("  ");//for alignement need a better way as it should be depending of size of translation of "off" vs "30min"
					addStringP(ui_text_on);//if not defined by preset
					}
			else if(EEPROM::timepowersaving==0) 
				{
				addStringP("  ");//for alignement need a better way as it should be depending of size of translation of "off/on" vs "30min"
				addStringP(ui_text_off);        // powersave off
				}
			else if (EEPROM::timepowersaving==(1000 * 60)) addStringP(" 1min");//1mn
			else if (EEPROM::timepowersaving==(1000 * 60 *5)) addStringP(" 5min");//5 min
			else if (EEPROM::timepowersaving==(1000 * 60 * 15)) addStringP("15min");//15 min
			else if (EEPROM::timepowersaving==(1000 * 60 * 30)) addStringP("30min");//30 min
			else {
					addStringP("  ");//for alignement need a better way as it should be depending of size of translation of "off" vs "30min"
					addStringP(ui_text_on);//if not defined by preset
					}
      	    #endif 
            break;
        case 'Z':
            if(c2=='1' || c2=='2' || c2=='3')
                    {
                        int p = c2- '0';
                        p--;
                        if (Z_probe[p] == -1000)
                            {
                                addStringP("  --.--- mm");
                            }
                        else
                             if (Z_probe[p] == -2000)
                            {
                                addStringP(UI_TEXT_FAILED);
                            }
                        else 
                            {
                                addFloat(Z_probe[p]-10 ,4,3);
                                addStringP(" mm");
                            }
                    }
            break;
		case 'z':
			if(c2=='m')
			 {
			 addFloat(Printer::zMin,4,2);
			 }
		
        case 'U':
            if(c2=='t')   // Printing time
            {
#if EEPROM_MODE!=0
                bool alloff = true;
                for(uint8_t i=0; i<NUM_EXTRUDER; i++)
                    if(tempController[i]->targetTemperatureC>15) alloff = false;

                long seconds = (alloff ? 0 : (HAL::timeInMilliseconds()-Printer::msecondsPrinting)/1000)+HAL::eprGetInt32(EPR_PRINTING_TIME);
                long tmp = seconds/86400;
                seconds-=tmp*86400;
                addInt(tmp,2);
                addStringP(PSTR(UI_TEXT_PRINTTIME_DAYS));
                tmp=seconds/3600;
                addInt(tmp,2);
                addStringP(PSTR(UI_TEXT_PRINTTIME_HOURS));
                seconds-=tmp*3600;
                tmp = seconds/60;
                addInt(tmp,2,'0');
                addStringP(PSTR(UI_TEXT_PRINTTIME_MINUTES));
#endif
            }
            else if(c2=='f')     // Filament usage
            {
#if EEPROM_MODE!=0
                float dist = Printer::filamentPrinted*0.001+HAL::eprGetFloat(EPR_PRINTING_DISTANCE);
                addFloat(dist,6,1);
#endif
            }
        }
    }
    printCols[col] = 0;
}
void UIDisplay::setStatusP(PGM_P txt,bool error)
{
    if(!error && Printer::isUIErrorMessage()) return;
    uint8_t i=0;
    while(i<16)
    {
        uint8_t c = pgm_read_byte(txt++);
        if(!c) break;
        statusMsg[i++] = c;
    }
    statusMsg[i]=0;
    if(error)
        Printer::setUIErrorMessage(true);
}
void UIDisplay::setStatus(const char *txt,bool error)
{
    if(!error && Printer::isUIErrorMessage()) return;
    uint8_t i=0;
    while(*txt && i<16)
        statusMsg[i++] = *txt++;
    statusMsg[i]=0;
    if(error)
        Printer::setUIErrorMessage(true);
}

const UIMenu * const ui_pages[UI_NUM_PAGES] PROGMEM = UI_PAGES;
uint8_t nFilesOnCard;
void UIDisplay::updateSDFileCount()
{
#if SDSUPPORT
    dir_t* p = NULL;
    byte offset = menuTop[menuLevel];
    SdBaseFile *root = sd.fat.vwd();

    root->rewind();
    nFilesOnCard = 0;
    while ((p = root->getLongFilename(p, tempLongFilename, 0, NULL)))
    {
        if (! (DIR_IS_FILE(p) || DIR_IS_SUBDIR(p)))
            continue;
        if (folderLevel>=SD_MAX_FOLDER_DEPTH && DIR_IS_SUBDIR(p) && !(p->name[0]=='.' && p->name[1]=='.'))
            continue;
 #if HIDE_BINARY_ON_SD
      	//hide unwished files
		if (!SDCard::showFilename(p,tempLongFilename))continue;
#endif
        nFilesOnCard++;
        if (nFilesOnCard==254)
            return;
    }
#endif
}

void getSDFilenameAt(uint16_t filePos,char *filename)
{
#if SDSUPPORT
    dir_t* p;
    byte c=0;
    SdBaseFile *root = sd.fat.vwd();

    root->rewind();
    while ((p = root->getLongFilename(p, tempLongFilename, 0, NULL)))
    {
        HAL::pingWatchdog();
        if (!DIR_IS_FILE(p) && !DIR_IS_SUBDIR(p)) continue;
        if(uid.folderLevel>=SD_MAX_FOLDER_DEPTH && DIR_IS_SUBDIR(p) && !(p->name[0]=='.' && p->name[1]=='.')) continue;
 #if HIDE_BINARY_ON_SD
        //hide unwished files
		if (!SDCard::showFilename(p,tempLongFilename))continue;
#endif
        if (filePos--)
            continue;
        strcpy(filename, tempLongFilename);
        if(DIR_IS_SUBDIR(p)) strcat(filename, "/"); // Set marker for directory
        break;
    }
#endif
}

bool UIDisplay::isDirname(char *name)
{
    while(*name) name++;
    name--;
    return *name=='/';
}

void UIDisplay::goDir(char *name)
{
#if SDSUPPORT
    char *p = cwd;
    while(*p)p++;
    if(name[0]=='.' && name[1]=='.')
    {
        if(folderLevel==0) return;
        p--;
        p--;
        while(*p!='/') p--;
        p++;
        *p = 0;
        folderLevel--;
    }
    else
    {
        if(folderLevel>=SD_MAX_FOLDER_DEPTH) return;
        while(*name) *p++ = *name++;
        *p = 0;
        folderLevel++;
    }
    sd.fat.chdir(cwd);
    updateSDFileCount();
    #endif
}

void UIDisplay::sdrefresh(uint8_t &r,char cache[UI_ROWS][MAX_COLS+1])
{
#if SDSUPPORT
    dir_t* p = NULL;
    byte offset = uid.menuTop[uid.menuLevel];
    SdBaseFile *root;
    byte length, skip;

    sd.fat.chdir(uid.cwd);
    root = sd.fat.vwd();
    root->rewind();

    skip = (offset>0?offset-1:0);

    while (r+offset<nFilesOnCard+1 && r<UI_ROWS && (p = root->getLongFilename(p, tempLongFilename, 0, NULL)))
    {
        HAL::pingWatchdog();
        // done if past last used entry
        // skip deleted entry and entries for . and  ..
        // only list subdirectories and files
        if ((DIR_IS_FILE(p) || DIR_IS_SUBDIR(p)))
        {
            if(uid.folderLevel >= SD_MAX_FOLDER_DEPTH && DIR_IS_SUBDIR(p) && !(p->name[0]=='.' && p->name[1]=='.'))
                continue;
 #if HIDE_BINARY_ON_SD
			//hide unwished files
			if (!SDCard::showFilename(p,tempLongFilename))continue;
 #endif
            if(skip>0)
            {
                skip--;
                continue;
            }
            uid.col=0;
            if(r+offset == uid.menuPos[uid.menuLevel])
                printCols[uid.col++] = CHAR_SELECTOR;
            else
                printCols[uid.col++] = ' ';
            // print file name with possible blank fill
            if(DIR_IS_SUBDIR(p))
                printCols[uid.col++] = 6; // Prepend folder symbol
            length = RMath::min((int)strlen(tempLongFilename), MAX_COLS-uid.col);
            memcpy(printCols+uid.col, tempLongFilename, length);
            uid.col += length;
            printCols[uid.col] = 0;
            strcpy(cache[r++],printCols);
        }
    }
    #endif
}
// Refresh current menu page
void UIDisplay::refreshPage()
{
    uint8_t r;
    uint8_t mtype;
    char cache[UI_ROWS][MAX_COLS+1];
    adjustMenuPos();
#if UI_AUTORETURN_TO_MENU_AFTER!=0
    // Reset timeout on menu back when user active on menu
    if (uid.encoderLast != encoderStartScreen)
        ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;
#endif
#if UI_AUTOLIGHTOFF_AFTER!=0
	//reset timeout for power saving
	if (uid.encoderLast != encoderStartScreen)
        UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
#endif
    encoderStartScreen = uid.encoderLast;

    // Copy result into cache
    if(menuLevel==0)
    {
        UIMenu *men = (UIMenu*)pgm_read_word(&(ui_pages[menuPos[0]]));
        uint8_t nr = pgm_read_word_near(&(men->numEntries));
        UIMenuEntry **entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
        //check entry is visible
        UIMenuEntry *testent =(UIMenuEntry *)pgm_read_word(&(entries[0]));
         if (!testent->showEntry())//not visible so look for next one
			{
			int  nb = 0;
			bool bloop =true;
		  while( nb < UI_NUM_PAGES+1 && bloop)
			  {
				  if (lastAction==UI_ACTION_PREVIOUS)
				  {
					menuPos[0] = (menuPos[0]==0 ? UI_NUM_PAGES-1 : menuPos[0]-1);
				  }
				  else
				  {
				  menuPos[0]++;
				  if(menuPos[0]>=UI_NUM_PAGES)menuPos[0]=0;
				 }
			  nb++; //to avoid to loop more than all entries once
			  UIMenu *mentest = (UIMenu*)pgm_read_word(&(ui_pages[menuPos[0]]));
			  UIMenuEntry **entriestest = (UIMenuEntry**)pgm_read_word(&(mentest->entries));
			  UIMenuEntry *enttest=(UIMenuEntry *)pgm_read_word(&(entriestest[0]));
			   if (enttest->showEntry()) bloop=false; //ok we found visible so we end here
			   }
		   if (bloop) menuPos[0]=0; //nothing was found so set page 0 even not visible
		   men = (UIMenu*)pgm_read_word(&(ui_pages[menuPos[0]]));
			nr = pgm_read_word_near(&(men->numEntries));
			entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
			}
        for(r=0; r<nr && r<UI_ROWS; r++)
        {
            UIMenuEntry *ent =(UIMenuEntry *)pgm_read_word(&(entries[r]));
            col=0;
            parse((char*)pgm_read_word(&(ent->text)),false);
            strcpy(cache[r],printCols);
        }
    }
    else
    {
		int numrows = UI_ROWS;
        UIMenu *men = (UIMenu*)menu[menuLevel];
        uint8_t nr = pgm_read_word_near((void*)&(men->numEntries));
        mtype = pgm_read_byte((void*)&(men->menuType));
        uint8_t offset = menuTop[menuLevel];
        UIMenuEntry **entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
        if (mtype== UI_MENU_TYPE_MENU_WITH_STATUS)numrows--;
        for(r=0; r+offset<nr && r<numrows; )
        {
            UIMenuEntry *ent =(UIMenuEntry *)pgm_read_word(&(entries[r+offset]));
            if(!ent->showEntry())
            {
                offset++;
                continue;
            }
            unsigned char entType = pgm_read_byte(&(ent->menuType));
            unsigned int entAction = pgm_read_word(&(ent->action));
            col=0;
            if(entType>=UI_MENU_ENTRY_MIN_TYPE_CHECK && entType<=UI_MENU_ENTRY_MAX_TYPE_CHECK)
            {
                if(r+offset==menuPos[menuLevel] && activeAction!=entAction)
                    printCols[col++]=CHAR_SELECTOR;
                else if(activeAction==entAction)
                    printCols[col++]=CHAR_SELECTED;
                else
                    printCols[col++]=' ';
            }
            parse((char*)pgm_read_word(&(ent->text)),false);
            if(entType==UI_MENU_TYPE_SUBMENU || entType==UI_MENU_TYPE_MENU_WITH_STATUS)   // Draw submenu marker at the right side
            {
                while(col<UI_COLS-1) printCols[col++]=' ';
                if(col>UI_COLS)
                {
                    printCols[RMath::min(UI_COLS-1,col)] = CHAR_RIGHT;
                }
                else
                    printCols[col] = CHAR_RIGHT; // Arrow right
                printCols[++col] = 0;
            }
            strcpy(cache[r],printCols);
            r++;
        }
	   
		if (mtype==UI_MENU_TYPE_MENU_WITH_STATUS) 
		{
		UIMenuEntry *ent =(UIMenuEntry *)pgm_read_word(&(entries[nr-1]));	//this is status bar
		printCols[0]=0;//fill with blank line if number of entries < 3(1 or 2)
		if (nr<UI_ROWS)r--;
		while(r<UI_ROWS)
			{
			strcpy(cache[r++],printCols);
			}	
		col=0;//init parsing 
		 parse((char*)pgm_read_word(&(ent->text)),false);
		 strcpy(cache[UI_ROWS-1],printCols);//copy status 
		 r=UI_ROWS;
		}
    }
#if SDSUPPORT
    if(mtype == UI_MENU_TYPE_FILE_SELECTOR)
    {
        sdrefresh(r,cache);
    }
#endif
    printCols[0]=0;
    while(r<UI_ROWS)
        strcpy(cache[r++],printCols);
    // Compute transition
    uint8_t transition = 0; // 0 = display, 1 = up, 2 = down, 3 = left, 4 = right
#if UI_ANIMATION
    if(menuLevel != oldMenuLevel && !PrintLine::hasLines())
    {
        if(oldMenuLevel == 0 || oldMenuLevel == -2)
            transition = 1;
        else if(menuLevel == 0)
            transition = 2;
        else if(menuLevel>oldMenuLevel)
            transition = 3;
        else
            transition = 4;
    }
#endif
    uint8_t loops = 1;
    uint8_t dt = 1,y;
    if(transition == 1 || transition == 2) loops = UI_ROWS;
    else if(transition>2)
    {
        dt = (UI_COLS+UI_COLS-1)/16;
        loops = UI_COLS+1/dt;
    }
    uint8_t off0 = (shift<=0 ? 0 : shift);
    uint8_t scroll = dt;
    uint8_t off[UI_ROWS];
    if(transition == 0)
    {
        for(y=0; y<UI_ROWS; y++)
            strcpy(displayCache[y],cache[y]);
    }
    for(y=0; y<UI_ROWS; y++)
    {
        uint8_t len = strlen(displayCache[y]); // length of line content
        off[y] = len > UI_COLS ? RMath::min(len - UI_COLS,off0) : 0;
        if(len > UI_COLS) {
           off[y] = RMath::min(len - UI_COLS,off0);
           if(transition == 0 && (mtype == UI_MENU_TYPE_FILE_SELECTOR || mtype == UI_MENU_TYPE_SUBMENU || mtype == UI_MENU_TYPE_MENU_WITH_STATUS)) {// Copy first char to front
            //displayCache[y][off[y]] = displayCache[y][0];
            cache[y][off[y]] = cache[y][0];
           }
        } else off[y] = 0;
#if UI_ANIMATION
        if(transition == 3)
        {
            for(r=len; r<MAX_COLS; r++)
            {
                displayCache[y][r] = 32;
            }
            displayCache[y][MAX_COLS] = 0;
        }
        else if(transition == 4)
        {
            for(r=strlen(cache[y]); r<MAX_COLS; r++)
            {
                cache[y][r] = 32;
            }
            cache[y][MAX_COLS] = 0;
        }
#endif
    }
    for(uint8_t l=0; l<loops; l++)
    {
        if(uid.encoderLast != encoderStartScreen)
        {
            scroll = 200;
        }
        scroll += dt;
#if UI_DISPLAY_TYPE == 5
#define drawHProgressBar(x,y,width,height,progress) \
     {u8g_DrawFrame(&u8g,x,y, width, height);  \
     int p = ceil((width-2) * progress / 100); \
     u8g_DrawBox(&u8g,x+1,y+1, p, height-2);}


#define drawVProgressBar(x,y,width,height,progress) \
     {u8g_DrawFrame(&u8g,x,y, width, height);  \
     int p = height-1 - ceil((height-2) * progress / 100); \
     u8g_DrawBox(&u8g,x+1,y+p, width-2, (height-p));}
#if UI_DISPLAY_TYPE == 5
#if SDSUPPORT
        unsigned long sdPercent;
#endif
        //fan
        int fanPercent;
        char fanString[2];
        if(menuLevel==0 && menuPos[0] == 0 )
        {
//ext1 and ext2 animation symbols
            if(extruder[0].tempControl.targetTemperatureC > 0)
                cache[0][0] = Printer::isAnimation()?'\x08':'\x09';
            else
                cache[0][0] = '\x0a'; //off
#if NUM_EXTRUDER>1
            if(extruder[1].tempControl.targetTemperatureC > 0)
                cache[1][0] = Printer::isAnimation()?'\x08':'\x09';
            else
#endif
                cache[1][0] = '\x0a'; //off
#if HAVE_HEATED_BED==true

            //heatbed animated icons
            if(heatedBedController.targetTemperatureC > 0)
                cache[2][0] = Printer::isAnimation()?'\x0c':'\x0d';
            else
                cache[2][0] = '\x0b';
#endif
            //fan
            fanPercent = Printer::getFanSpeed()*100/255;
            fanString[1]=0;
            if(fanPercent > 0)  //fan running anmation
            {
                fanString[0] = Printer::isAnimation() ? '\x0e' : '\x0f';
            }
            else
            {
                fanString[0] = '\x0e';
            }
#if SDSUPPORT
            //SD Card
            if(sd.sdactive)
            {
                if(sd.sdactive && sd.sdmode)
                {
                    if(sd.filesize<20000000) sdPercent=sd.sdpos*100/sd.filesize;
                    else sdPercent = (sd.sdpos>>8)*100/(sd.filesize>>8);
                }
                else
                {
                    sdPercent = 0;
                }
            }
#endif
        }
#endif
        //u8g picture loop
        u8g_FirstPage(&u8g);
        do
        {
#endif
            if(transition == 0)
            {
#if UI_DISPLAY_TYPE == 5

                if(menuLevel==0 && menuPos[0] == 0 )
                {
                    u8g_SetFont(&u8g,UI_FONT_SMALL);
                    uint8_t py = 8;
                    for(uint8_t r=0; r<3; r++)
                    {
                        if(u8g_IsBBXIntersection(&u8g, 0, py-UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                            printU8GRow(0,py,cache[r]);
                        py+=10;
                    }
                    //fan
                    if(u8g_IsBBXIntersection(&u8g, 0, 30-UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                        printU8GRow(117,30,fanString);
                    drawVProgressBar(116, 0, 9, 20, fanPercent);
                    if(u8g_IsBBXIntersection(&u8g, 0, 43-UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                        printU8GRow(0,43,cache[3]); //mul
                    if(u8g_IsBBXIntersection(&u8g, 0, 52-UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                        printU8GRow(0,52,cache[4]); //buf

#if SDSUPPORT
                    //SD Card
                    if(sd.sdactive && u8g_IsBBXIntersection(&u8g, 70, 48-UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                    {
                        printU8GRow(70,48,"SD");
                        drawHProgressBar(83,42, 40, 5, sdPercent);
                    }
#endif
                    //Status
                    py = u8g_GetHeight(&u8g)-2;
                    if(u8g_IsBBXIntersection(&u8g, 70, py-UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                        printU8GRow(0,py,cache[5]);

                    //divider lines
                    u8g_DrawHLine(&u8g,0, 32, u8g_GetWidth(&u8g));
                    if ( u8g_IsBBXIntersection(&u8g, 55, 0, 1, 32) )
                    {
                        u8g_draw_vline(&u8g,112, 0, 32);
                        u8g_draw_vline(&u8g,62, 0, 32);
                    }
                    u8g_SetFont(&u8g, UI_FONT_DEFAULT);
                }
                else
                {
#endif
                    for(y=0; y<UI_ROWS; y++)
                        printRow(y,&cache[y][off[y]],NULL,UI_COLS);
#if UI_DISPLAY_TYPE == 5
                }
#endif
            }
#if UI_ANIMATION
            else
            {
                if(transition == 1)   // up
                {
                    if(scroll > UI_ROWS)
                    {
                        scroll = UI_ROWS;
                        l = loops;
                    }
                    for(y=0; y<UI_ROWS-scroll; y++)
                    {
                        r = y+scroll;
                        printRow(y,&displayCache[r][off[r]],NULL,UI_COLS);
                    }
                    for(y=0; y<scroll; y++)
                    {
                        printRow(UI_ROWS-scroll+y,cache[y],NULL,UI_COLS);
                    }
                }
                else if(transition == 2)     // down
                {
                    if(scroll > UI_ROWS)
                    {
                        scroll = UI_ROWS;
                        l = loops;
                    }
                    for(y=0; y<scroll; y++)
                    {
                        printRow(y,cache[UI_ROWS-scroll+y],NULL,UI_COLS);
                    }
                    for(y=0; y<UI_ROWS-scroll; y++)
                    {
                        r = y+scroll;
                        printRow(y+scroll,&displayCache[y][off[y]],NULL,UI_COLS);
                    }
                }
                else if(transition == 3)     // left
                {
                    if(scroll > UI_COLS)
                    {
                        scroll = UI_COLS;
                        l = loops;
                    }
                    for(y=0; y<UI_ROWS; y++)
                    {
                        printRow(y,&displayCache[y][off[y]+scroll],cache[y],UI_COLS-scroll);
                    }
                }
                else     // right
                {
                    if(scroll > UI_COLS)
                    {
                        scroll = UI_COLS;
                        l = loops;
                    }
                    for(y=0; y<UI_ROWS; y++)
                    {
                        printRow(y,cache[y]+UI_COLS-scroll,&displayCache[y][off[y]],scroll);
                    }
                }
#if DISPLAY_TYPE != 5
                HAL::delayMilliseconds(transition<3 ? 200 : 70);
#endif
                HAL::pingWatchdog();
            }
#endif
#if UI_DISPLAY_TYPE == 5
        }
        while( u8g_NextPage(&u8g) );  //end picture loop
        Printer::toggleAnimation();
#endif
    } // for l
#if UI_ANIMATION
    // copy to last cache
    if(transition != 0)
        for(y=0; y<UI_ROWS; y++)
            strcpy(displayCache[y],cache[y]);
    oldMenuLevel = menuLevel;
#endif
}

void UIDisplay::pushMenu(const UIMenu *men,bool refresh)
{
    if(men==menu[menuLevel])
    {
        refreshPage();
        return;
    }
    if(menuLevel==UI_MENU_MAXLEVEL-1) return;
    menuLevel++;
    menu[menuLevel]=men;
    menuTop[menuLevel] = menuPos[menuLevel] = 0;
#if SDSUPPORT
    UIMenu *men2 = (UIMenu*)menu[menuLevel];
    if(pgm_read_byte(&(men2->menuType))==UI_MENU_TYPE_FILE_SELECTOR) {
      // Menu is Open files list
      updateSDFileCount();
      // Keep menu positon in file list, more user friendly.
      // If file list changed, still need to reset position.
      if (menuPos[menuLevel] > nFilesOnCard) {
        //This exception can happen if the card was unplugged or modified.
        menuTop[menuLevel] = 0;
        menuPos[menuLevel] = UI_MENU_BACKCNT; // if top entry is back, default to next useful item
      }
    } else
#endif
    {
      // With or without SDCARD, being here means the menu is not a files list
      // Reset menu to top
      menuTop[menuLevel] = 0;
      menuPos[menuLevel] = UI_MENU_BACKCNT; // if top entry is back, default to next useful item
   }
    if(refresh)
        refreshPage();
}
void UIDisplay::okAction()
{
    if(Printer::isUIErrorMessage()) {
        Printer::setUIErrorMessage(false);
        return;
    }
#if UI_HAS_KEYS==1
    if(menuLevel==0)   // Enter menu
    {
        menuLevel = 1;
        menuTop[1] = 0;
        menuPos[1] =  UI_MENU_BACKCNT; // if top entry is back, default to next useful item
        menu[1] = &ui_menu_main;
        BEEP_SHORT
        return;
    }
    UIMenu *men = (UIMenu*)menu[menuLevel];
    //uint8_t nr = pgm_read_word_near(&(menu->numEntries));
    uint8_t mtype = pgm_read_byte(&(men->menuType));
    UIMenuEntry **entries;
    UIMenuEntry *ent;
    unsigned char entType;
    int action;
    if(mtype==UI_MENU_TYPE_ACTION_MENU)   // action menu
    {
        action = pgm_read_word(&(men->id));
        finishAction(action);
        executeAction(UI_ACTION_BACK);
        return;
    }
    if(((mtype==UI_MENU_TYPE_SUBMENU) ||(mtype == UI_MENU_TYPE_MENU_WITH_STATUS) ) && entType==UI_MENU_TYPE_MODIFICATION_MENU)   // Modify action
    {
        if(activeAction)   // finish action
        {
            finishAction(action);
            activeAction = 0;
        }
        else
            activeAction = action;
        return;
    }
#if SDSUPPORT
    if(mtype == UI_MENU_TYPE_FILE_SELECTOR)
    {
        if(menuPos[menuLevel]==0)   // Selected back instead of file
        {
            executeAction(UI_ACTION_BACK);
            return;
        }
        if(!sd.sdactive)
            return;

        uint8_t filePos = menuPos[menuLevel]-1;
        char filename[LONG_FILENAME_LENGTH+1];

        getSDFilenameAt(filePos, filename);
        if(isDirname(filename))   // Directory change selected
        {
            goDir(filename);
            menuTop[menuLevel]=0;
            menuPos[menuLevel]=1;
            refreshPage();
            oldMenuLevel = -1;
            return;
        }

        int16_t shortAction; // renamed to avoid scope confusion
        if (Printer::isAutomount())
            shortAction = UI_ACTION_SD_PRINT;
        else
        {
            men = menu[menuLevel-1];
            entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
            ent =(UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel-1]]));
            shortAction = pgm_read_word(&(ent->action));
        }
        sd.file.close();
        sd.fat.chdir(cwd);
        switch(shortAction)
        {
        case UI_ACTION_SD_PRINT:
            if (sd.selectFile(filename, false))
            {
                sd.startPrint();
                BEEP_LONG;
                menuLevel = 0;
            }
            break;
        case UI_ACTION_SD_DELETE:
            if(sd.sdactive)
            {
                sd.sdmode = false;
                sd.file.close();
                if(sd.fat.remove(filename))
                {
                    Com::printFLN(Com::tFileDeleted);
                    BEEP_LONG
                    if(menuPos[menuLevel]>0)
						menuPos[menuLevel]--;
					updateSDFileCount();
                }
                else
                {
                    Com::printFLN(Com::tDeletionFailed);
                }
            }
            break;
        }
        return;
    }
#endif
    entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
    ent =(UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
    entType = pgm_read_byte(&(ent->menuType));// 0 = Info, 1 = Headline, 2 = submenu ref, 3 = direct action command, 4 = modify action, 5=special menu with status
    action = pgm_read_word(&(ent->action));
    if(mtype == UI_MENU_TYPE_ACTION_MENU)   // action menu
    {
        action = pgm_read_word(&(men->id));
        finishAction(action);
        executeAction(UI_ACTION_BACK);
        return;
    }
    if(((mtype == UI_MENU_TYPE_SUBMENU) ||(mtype == UI_MENU_TYPE_MENU_WITH_STATUS) )&& entType == UI_MENU_TYPE_MODIFICATION_MENU)   // Modify action
    {
        if(activeAction)   // finish action
        {
            finishAction(action);
            activeAction = 0;
        }
        else
            activeAction = action;
        return;
    }
    if(entType==UI_MENU_TYPE_MENU_WITH_STATUS || entType==UI_MENU_TYPE_SUBMENU)   // Enter submenu
    {
        pushMenu((UIMenu*)action,false);
        BEEP_SHORT
        return;
    }
    if(entType==UI_MENU_TYPE_ACTION_MENU)
    {
        executeAction(action);
        return;
    }
    executeAction(UI_ACTION_BACK);
#endif
}

#define INCREMENT_MIN_MAX(a,steps,_min,_max) if ( (increment<0) && (_min>=0) && (a<_min-increment*steps) ) {a=_min;} else { a+=increment*steps; if(a<_min) a=_min; else if(a>_max) a=_max;};

void UIDisplay::adjustMenuPos()
{
    if(menuLevel == 0) return;
    UIMenu *men = (UIMenu*)menu[menuLevel];
    UIMenuEntry **entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
    uint8_t mtype = HAL::readFlashByte((const prog_char*)&(men->menuType));
    int numrows=UI_ROWS;
    if(!((mtype == UI_MENU_TYPE_SUBMENU)||(mtype == UI_MENU_TYPE_MENU_WITH_STATUS))) return;
    if (mtype == UI_MENU_TYPE_MENU_WITH_STATUS)numrows--;
    while(menuPos[menuLevel]>0)
    {
        if(((UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]])))->showEntry())
            break;
        menuPos[menuLevel]--;
    }
    uint8_t skipped = 0;
    bool modified;
    if(menuTop[menuLevel] > menuPos[menuLevel])
        menuTop[menuLevel] = menuPos[menuLevel];
    do
    {
        skipped = 0;
        modified = false;
        for(uint8_t r=menuTop[menuLevel]; r<menuPos[menuLevel]; r++)
        {
            UIMenuEntry *ent =(UIMenuEntry *)pgm_read_word(&(entries[r]));
            if(!ent->showEntry())
                skipped++;
        }
        if(menuTop[menuLevel] + skipped + numrows - 1 < menuPos[menuLevel]) {
            menuTop[menuLevel] = menuPos[menuLevel] + 1 - numrows;
            modified = true;
        }
    }
    while(modified);
    //fix for first items that are hiden by filter - cannot be selected
     uint8_t nr = pgm_read_word_near(&(men->numEntries));
     while (!((UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]])))->showEntry() && (nr > 0))
			{
			menuPos[menuLevel]++;
			nr--;
	     	}
}

void UIDisplay::nextPreviousAction(int8_t next)
{
    if(Printer::isUIErrorMessage()) {
        Printer::setUIErrorMessage(false);
        return;
    }
    millis_t actTime = HAL::timeInMilliseconds();
    millis_t dtReal;
    millis_t dt = dtReal = actTime-lastNextPrev;
    lastNextPrev = actTime;
    if(dt<SPEED_MAX_MILLIS) dt = SPEED_MAX_MILLIS;
    if(dt>SPEED_MIN_MILLIS)
    {
        dt = SPEED_MIN_MILLIS;
        lastNextAccumul = 1;
    }
    float f = (float)(SPEED_MIN_MILLIS-dt)/(float)(SPEED_MIN_MILLIS-SPEED_MAX_MILLIS);
    lastNextAccumul = 1.0f+(float)SPEED_MAGNIFICATION*f*f*f;

#if UI_HAS_KEYS==1
    if(menuLevel==0)
    {
        lastSwitch = HAL::timeInMilliseconds();
        if((UI_INVERT_MENU_DIRECTION && next<0) || (!UI_INVERT_MENU_DIRECTION && next>0))
        {
            menuPos[0]++;
            if(menuPos[0]>=UI_NUM_PAGES)
                menuPos[0]=0;
        }
        else
        {
            menuPos[0] = (menuPos[0]==0 ? UI_NUM_PAGES-1 : menuPos[0]-1);
        }
        return;
    }
    UIMenu *men = (UIMenu*)menu[menuLevel];
    uint8_t nr = pgm_read_word_near(&(men->numEntries));
    uint8_t mtype = HAL::readFlashByte((const prog_char*)&(men->menuType));
    UIMenuEntry **entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
    UIMenuEntry *ent =(UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
    UIMenuEntry *testEnt;
    uint8_t entType = HAL::readFlashByte((const prog_char*)&(ent->menuType));// 0 = Info, 1 = Headline, 2 = submenu ref, 3 = direct action command, 4 = modify action, 5=special menu with status
    int action = pgm_read_word(&(ent->action));
    if(((mtype == UI_MENU_TYPE_SUBMENU)||(mtype == UI_MENU_TYPE_MENU_WITH_STATUS)) && activeAction == 0)   // browse through menu items
    {
        if((UI_INVERT_MENU_DIRECTION && next<0) || (!UI_INVERT_MENU_DIRECTION && next>0))
        {
            while(menuPos[menuLevel]+1<nr)
            {
				if ((mtype == UI_MENU_TYPE_MENU_WITH_STATUS)&&(menuPos[menuLevel]+2 ==nr))break;
                menuPos[menuLevel]++;
                testEnt = (UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
                if(testEnt->showEntry())
                    break;
            }
        }
        else if(menuPos[menuLevel]>0)
        {
            while(menuPos[menuLevel]>0)
            {
                menuPos[menuLevel]--;
                testEnt = (UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
                if(testEnt->showEntry())
                    break;
            }
        }
        adjustMenuPos();
        return;
    }
#if SDSUPPORT
    if(mtype == UI_MENU_TYPE_FILE_SELECTOR)   // SD listing
    {
        if((UI_INVERT_MENU_DIRECTION && next<0) || (!UI_INVERT_MENU_DIRECTION && next>0))
        {
            if(menuPos[menuLevel]<nFilesOnCard) menuPos[menuLevel]++;
        }
        else if(menuPos[menuLevel]>0)
            menuPos[menuLevel]--;
        if(menuTop[menuLevel]>menuPos[menuLevel])
            menuTop[menuLevel]=menuPos[menuLevel];
        else if(menuTop[menuLevel]+UI_ROWS-1<menuPos[menuLevel])
            menuTop[menuLevel]=menuPos[menuLevel]+1-UI_ROWS;
        return;
    }
#endif
    if(mtype == UI_MENU_TYPE_ACTION_MENU) action = pgm_read_word(&(men->id));
    else action = activeAction;
    int8_t increment = -next;
    switch(action)
    {
    case UI_ACTION_FANSPEED:
        Commands::setFanSpeed(Printer::getFanSpeed()+increment*3,false);
        break;
    case UI_ACTION_XPOSITION:
#if UI_SPEEDDEPENDENT_POSITIONING
    {
        float d = 0.01*(float)increment*lastNextAccumul;
        if(fabs(d)*2000>Printer::maxFeedrate[X_AXIS]*dtReal)
            d *= Printer::maxFeedrate[X_AXIS]*dtReal/(2000*fabs(d));
        long steps = (long)(d*Printer::axisStepsPerMM[X_AXIS]);
        steps = ( increment<0 ? RMath::min(steps,(long)increment) : RMath::max(steps,(long)increment));
        PrintLine::moveRelativeDistanceInStepsReal(steps,0,0,0,Printer::maxFeedrate[X_AXIS],true);
    }
#else
    PrintLine::moveRelativeDistanceInStepsReal(increment,0,0,0,Printer::homingFeedrate[X_AXIS],true);
#endif
    Commands::printCurrentPosition();
    break;
    case UI_ACTION_YPOSITION:
#if UI_SPEEDDEPENDENT_POSITIONING
    {
        float d = 0.01*(float)increment*lastNextAccumul;
        if(fabs(d)*2000>Printer::maxFeedrate[Y_AXIS]*dtReal)
            d *= Printer::maxFeedrate[Y_AXIS]*dtReal/(2000*fabs(d));
        long steps = (long)(d*Printer::axisStepsPerMM[Y_AXIS]);
        steps = ( increment<0 ? RMath::min(steps,(long)increment) : RMath::max(steps,(long)increment));
        PrintLine::moveRelativeDistanceInStepsReal(0,steps,0,0,Printer::maxFeedrate[Y_AXIS],true);
    }
#else
    PrintLine::moveRelativeDistanceInStepsReal(0,increment,0,0,Printer::homingFeedrate[Y_AXIS],true);
#endif
    Commands::printCurrentPosition();
    break;
    case UI_ACTION_ZPOSITION:
#if UI_SPEEDDEPENDENT_POSITIONING
    {
        float d = 0.01*(float)increment*lastNextAccumul;
        if(fabs(d)*2000>Printer::maxFeedrate[Z_AXIS]*dtReal)
            d *= Printer::maxFeedrate[Z_AXIS]*dtReal/(2000*fabs(d));
        long steps = (long)(d*Printer::axisStepsPerMM[Z_AXIS]);
        steps = ( increment<0 ? RMath::min(steps,(long)increment) : RMath::max(steps,(long)increment));
        PrintLine::moveRelativeDistanceInStepsReal(0,0,steps,0,Printer::maxFeedrate[Z_AXIS],true);
    }
#else
    PrintLine::moveRelativeDistanceInStepsReal(0,0,increment,0,Printer::homingFeedrate[Z_AXIS],true);
#endif
    Commands::printCurrentPosition();
    break;
    case UI_ACTION_X_1:
    case UI_ACTION_X_10:
    case UI_ACTION_X_100:
    {
		float tmp_pos=Printer::currentPosition[X_AXIS];
		int istep=1;
		if (action==UI_ACTION_X_10)istep=10;
		if (action==UI_ACTION_X_100)istep=100;
		if (!Printer::isXHomed())//ask for home to secure movement
		{
			if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_HOME_X,UI_TEXT_WARNING_POS_X_UNKNOWN,UI_CONFIRMATION_TYPE_YES_NO,true))
					{
					 executeAction(UI_ACTION_HOME_X);
					}
		}
		if(Printer::isXHomed())//check if accepted to home
		{
			if (Extruder::current->id==0)
				{
					INCREMENT_MIN_MAX(tmp_pos,istep,Printer::xMin-ENDSTOP_X_BACK_ON_HOME,Printer::xMin+Printer::xLength);//this is for default extruder
				}
			else
				{
					INCREMENT_MIN_MAX(tmp_pos,istep,Printer::xMin-ENDSTOP_X_BACK_ON_HOME+round(Extruder::current->xOffset/Printer::axisStepsPerMM[X_AXIS]),Printer::xMin+Printer::xLength+round(Extruder::current->xOffset/Printer::axisStepsPerMM[X_AXIS]));//this is for default extruder
				}
			if (tmp_pos!=(increment*istep)+Printer::currentPosition[X_AXIS]) break; //we are out of range so do not do
		}
		//we move under control range or not homed
		PrintLine::moveRelativeDistanceInStepsReal(Printer::axisStepsPerMM[X_AXIS]*increment*istep,0,0,0,Printer::homingFeedrate[X_AXIS],true);
        Commands::printCurrentPosition();
    break;
	}

    case UI_ACTION_Y_1:
    case UI_ACTION_Y_10:
    case UI_ACTION_Y_100:
    {
		float tmp_pos=Printer::currentPosition[Y_AXIS];
		int istep=1;
		if (action==UI_ACTION_Y_10)istep=10;
		if (action==UI_ACTION_Y_100)istep=100;
		if (!Printer::isYHomed())//ask for home to secure movement
		{
			if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_HOME_Y,UI_TEXT_WARNING_POS_Y_UNKNOWN,UI_CONFIRMATION_TYPE_YES_NO,true))
					{
					 executeAction(UI_ACTION_HOME_Y);
					}
		}
		if(Printer::isYHomed())//check if accepted to home
		{
			if (Extruder::current->id==0)
				{
					INCREMENT_MIN_MAX(tmp_pos,istep,Printer::yMin-ENDSTOP_Y_BACK_ON_HOME,Printer::yMin+Printer::yLength);//this is for default extruder
				}
			else
				{
					INCREMENT_MIN_MAX(tmp_pos,istep,Printer::yMin-ENDSTOP_Y_BACK_ON_HOME+round(Extruder::current->yOffset/Printer::axisStepsPerMM[Y_AXIS]),Printer::yMin+Printer::yLength+round(Extruder::current->yOffset/Printer::axisStepsPerMM[Y_AXIS]));//this is for default extruder
				}
			if (tmp_pos!=(increment*istep)+Printer::currentPosition[Y_AXIS]) break; //we are out of range so do not do
		}
		//we move under control range or not homed
		PrintLine::moveRelativeDistanceInStepsReal(0,Printer::axisStepsPerMM[Y_AXIS]*increment*istep,0,0,Printer::homingFeedrate[Y_AXIS],true);
        Commands::printCurrentPosition();
    break;
	}
   
    case UI_ACTION_Z_0_1:
    case UI_ACTION_Z_1:
    case UI_ACTION_Z_10:
    case UI_ACTION_Z_100:
    {
		float tmp_pos=Printer::currentPosition[Z_AXIS];
		float istep=0.1;
        if (action==UI_ACTION_Z_1)istep=1;
		if (action==UI_ACTION_Z_10)istep=10;
		if (action==UI_ACTION_Z_100)istep=100;
        increment=-increment; //upside down increment to allow keys to follow  Z movement, Up Key make Z going up, down key make Z going down
		if (!Printer::isZHomed())//ask for home to secure movement
		{
			if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_HOME_Z,UI_TEXT_WARNING_POS_Z_UNKNOWN,UI_CONFIRMATION_TYPE_YES_NO,true))
					{
					 executeAction(UI_ACTION_HOME_Z);
					}
		}
		if(Printer::isZHomed())//check if accepted to home
		{
			INCREMENT_MIN_MAX(tmp_pos,istep,Printer::zMin-ENDSTOP_Z_BACK_ON_HOME,Printer::zMin+Printer::zLength);

			if (tmp_pos!=(increment*istep)+Printer::currentPosition[Z_AXIS]) break; //we are out of range so do not do
		}
		//we move under control range or not homed
		PrintLine::moveRelativeDistanceInStepsReal(0,0,Printer::axisStepsPerMM[Z_AXIS]*increment*istep,0,Printer::homingFeedrate[Z_AXIS],true);
        Commands::printCurrentPosition();
    break;
	}
	case UI_ACTION_E_1:
    case UI_ACTION_E_10:
    case UI_ACTION_E_100:
    {
		int istep=1;
		if (action==UI_ACTION_E_10)istep=10;
		if (action==UI_ACTION_E_100)istep=100;
        increment=-increment; //upside down increment to allow keys to follow  filament movement, Up Key make filament going up, down key make filament going down
		if(reportTempsensorError() or Printer::debugDryrun()) break;
		//check temperature
		if(Extruder::current->tempControl.currentTemperatureC<=MIN_EXTRUDER_TEMP)
			{
				if (confirmationDialog(UI_TEXT_WARNING ,UI_TEXT_EXTRUDER_COLD,UI_TEXT_HEAT_EXTRUDER,UI_CONFIRMATION_TYPE_YES_NO,true))
					{
					UI_STATUS(UI_TEXT_HEATING_EXTRUDER);
					Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,Extruder::current->id);
					}
				else
					{
					UI_STATUS(UI_TEXT_EXTRUDER_COLD);
					}
			executeAction(UI_ACTION_TOP_MENU);
			 break;
			}
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
		bool btmp_autoreturn=benable_autoreturn; //save current value
		benable_autoreturn=false;//desactivate no need to test if active or not
#endif
		//we move 
		PrintLine::moveRelativeDistanceInSteps(0,0,0,Printer::axisStepsPerMM[E_AXIS]*increment*istep,UI_SET_EXTRUDER_FEEDRATE,true,false);
        Commands::printCurrentPosition();
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
			if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
			{
			benable_autoreturn=true;//reactivate 
			ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
			}
 #endif
    break;
	}
   
    case UI_ACTION_EPOSITION:
         //need to check temperature ?
        PrintLine::moveRelativeDistanceInSteps(0,0,0,Printer::axisStepsPerMM[3]*increment,UI_SET_EXTRUDER_FEEDRATE,true,false);
        Commands::printCurrentPosition();
        break;

    case UI_ACTION_Z_BABYSTEPS:
        {
            previousMillisCmd = HAL::timeInMilliseconds();
            if(increment > 0) {
                if((int)Printer::zBabystepsMissing+BABYSTEP_MULTIPLICATOR<127)
                    Printer::zBabystepsMissing+=BABYSTEP_MULTIPLICATOR;
            } else {
                if((int)Printer::zBabystepsMissing-BABYSTEP_MULTIPLICATOR>-127)
                    Printer::zBabystepsMissing-=BABYSTEP_MULTIPLICATOR;
            }
        }
        break;
    case UI_ACTION_HEATED_BED_TEMP:
#if HAVE_HEATED_BED==true
    {
        int tmp = (int)heatedBedController.targetTemperatureC;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_HEATED_BED_TEMP;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_HEATED_BED_TEMP) tmp = UI_SET_MAX_HEATED_BED_TEMP;
        Extruder::setHeatedBedTemperature(tmp);
    }
#endif
    break;
    case UI_ACTION_EXT_TEMP_ABS :
    {
		 int tmp = EEPROM::ftemp_ext_abs;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_EXTRUDER_TEMP;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_EXTRUDER_TEMP) tmp = UI_SET_MAX_EXTRUDER_TEMP;
        EEPROM::ftemp_ext_abs=tmp;
	}
	break;
	case UI_ACTION_EXT_TEMP_PLA :
    {
		 int tmp = EEPROM::ftemp_ext_pla;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_EXTRUDER_TEMP;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_EXTRUDER_TEMP) tmp = UI_SET_MAX_EXTRUDER_TEMP;
        EEPROM::ftemp_ext_pla=tmp;
	}
	break;
	case UI_ACTION_BED_TEMP_ABS :
    {
		 int tmp = EEPROM::ftemp_bed_abs;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_HEATED_BED_TEMP;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_HEATED_BED_TEMP) tmp = UI_SET_MAX_HEATED_BED_TEMP;
        EEPROM::ftemp_bed_abs=tmp;
	}
	break;
case UI_ACTION_BED_TEMP_PLA :
    {
		 int tmp = EEPROM::ftemp_bed_pla;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_HEATED_BED_TEMP;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_HEATED_BED_TEMP) tmp = UI_SET_MAX_HEATED_BED_TEMP;
        EEPROM::ftemp_bed_pla=tmp;
	}
	break;
    case UI_ACTION_EXTRUDER0_TEMP:
    {
        int tmp = (int)extruder[0].tempControl.targetTemperatureC;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_EXTRUDER_TEMP;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_EXTRUDER_TEMP) tmp = UI_SET_MAX_EXTRUDER_TEMP;
        Extruder::setTemperatureForExtruder(tmp,0);
    }
    break;
    case UI_ACTION_EXTRUDER1_TEMP:
#if NUM_EXTRUDER>1
    {
        int tmp = (int)extruder[1].tempControl.targetTemperatureC;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_EXTRUDER_TEMP;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_EXTRUDER_TEMP) tmp = UI_SET_MAX_EXTRUDER_TEMP;
        Extruder::setTemperatureForExtruder(tmp,1);
    }
#endif
    break;
    case UI_ACTION_FEEDRATE_MULTIPLY:
    {
        int fr = Printer::feedrateMultiply;
        INCREMENT_MIN_MAX(fr,1,25,500);
        Commands::changeFeedrateMultiply(fr);
    }
    break;
    case UI_ACTION_FLOWRATE_MULTIPLY:
    {
        INCREMENT_MIN_MAX(Printer::extrudeMultiply,1,25,500);
        Com::printFLN(Com::tFlowMultiply,(int)Printer::extrudeMultiply);
    }
    break;
    case UI_ACTION_STEPPER_INACTIVE:
        stepperInactiveTime -= stepperInactiveTime % 1000;
        INCREMENT_MIN_MAX(stepperInactiveTime,60000UL,0,10080000UL);
        //save directly to eeprom 
        EEPROM:: update(EPR_STEPPER_INACTIVE_TIME,EPR_TYPE_LONG,stepperInactiveTime,0);
        break;
    case UI_ACTION_MAX_INACTIVE:
        maxInactiveTime -= maxInactiveTime % 1000;
        INCREMENT_MIN_MAX(maxInactiveTime,60000UL,0,10080000UL);
        //save directly to eeprom 
         EEPROM:: update(EPR_MAX_INACTIVE_TIME,EPR_TYPE_LONG,maxInactiveTime,0);
        break;
      case UI_ACTION_LIGHT_OFF_AFTER:
        EEPROM::timepowersaving -= EEPROM::timepowersaving % 1000;
        INCREMENT_MIN_MAX(EEPROM::timepowersaving,60000UL,0,10080000UL);
         //save directly to eeprom 
        EEPROM:: update(EPR_POWERSAVE_AFTER_TIME,EPR_TYPE_LONG,EEPROM::timepowersaving,0);
        break;
     
    case UI_ACTION_PRINT_ACCEL_X:
        INCREMENT_MIN_MAX(Printer::maxAccelerationMMPerSquareSecond[X_AXIS],100,0,10000);
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_PRINT_ACCEL_Y:
#if DRIVE_SYSTEM!=3
        INCREMENT_MIN_MAX(Printer::maxAccelerationMMPerSquareSecond[Y_AXIS],1,0,10000);
#else
        INCREMENT_MIN_MAX(Printer::maxAccelerationMMPerSquareSecond[Y_AXIS],100,0,10000);
#endif
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_PRINT_ACCEL_Z:
        INCREMENT_MIN_MAX(Printer::maxAccelerationMMPerSquareSecond[Z_AXIS],100,0,10000);
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_MOVE_ACCEL_X:
        INCREMENT_MIN_MAX(Printer::maxTravelAccelerationMMPerSquareSecond[X_AXIS],100,0,10000);
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_MOVE_ACCEL_Y:
        INCREMENT_MIN_MAX(Printer::maxTravelAccelerationMMPerSquareSecond[Y_AXIS],100,0,10000);
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_MOVE_ACCEL_Z:
#if DRIVE_SYSTEM!=3
        INCREMENT_MIN_MAX(Printer::maxTravelAccelerationMMPerSquareSecond[Z_AXIS],1,0,10000);
#else
        INCREMENT_MIN_MAX(Printer::maxTravelAccelerationMMPerSquareSecond[Z_AXIS],100,0,10000);
#endif
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_MAX_JERK:
        INCREMENT_MIN_MAX(Printer::maxJerk,0.1,1,99.9);
        break;
#if DRIVE_SYSTEM!=3
    case UI_ACTION_MAX_ZJERK:
        INCREMENT_MIN_MAX(Printer::maxZJerk,0.1,0.1,99.9);
        break;
#endif
    case UI_ACTION_HOMING_FEEDRATE_X:
        INCREMENT_MIN_MAX(Printer::homingFeedrate[0],1,5,1000);
        break;
    case UI_ACTION_HOMING_FEEDRATE_Y:
        INCREMENT_MIN_MAX(Printer::homingFeedrate[1],1,5,1000);
        break;
    case UI_ACTION_HOMING_FEEDRATE_Z:
        INCREMENT_MIN_MAX(Printer::homingFeedrate[2],1,1,1000);
        break;
    case UI_ACTION_MAX_FEEDRATE_X:
        INCREMENT_MIN_MAX(Printer::maxFeedrate[0],1,1,1000);
        break;
    case UI_ACTION_MAX_FEEDRATE_Y:
        INCREMENT_MIN_MAX(Printer::maxFeedrate[1],1,1,1000);
        break;
    case UI_ACTION_MAX_FEEDRATE_Z:
        INCREMENT_MIN_MAX(Printer::maxFeedrate[2],1,1,1000);
        break;
    case UI_ACTION_STEPS_X:
        INCREMENT_MIN_MAX(Printer::axisStepsPerMM[0],0.1,0,999);
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_STEPS_Y:
        INCREMENT_MIN_MAX(Printer::axisStepsPerMM[1],0.1,0,999);
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_STEPS_Z:
        INCREMENT_MIN_MAX(Printer::axisStepsPerMM[2],0.1,0,999);
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_BAUDRATE:
#if EEPROM_MODE!=0
    {
        char p=0;
        int32_t rate;
        do
        {
            rate = pgm_read_dword(&(baudrates[p]));
            if(rate==baudrate) break;
            p++;
        }
        while(rate!=0);
        if(rate==0) p-=2;
        p+=increment;
        if(p<0) p = 0;
        rate = pgm_read_dword(&(baudrates[p]));
        if(rate==0) p--;
        baudrate = pgm_read_dword(&(baudrates[p]));
    }
#endif
    break;
#ifdef TEMP_PID
    case UI_ACTION_PID_PGAIN:
        INCREMENT_MIN_MAX(Extruder::current->tempControl.pidPGain,0.1,0,200);
        break;
    case UI_ACTION_PID_IGAIN:
        INCREMENT_MIN_MAX(Extruder::current->tempControl.pidIGain,0.01,0,100);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_PID_DGAIN:
        INCREMENT_MIN_MAX(Extruder::current->tempControl.pidDGain,0.1,0,200);
        break;
    case UI_ACTION_DRIVE_MIN:
        INCREMENT_MIN_MAX(Extruder::current->tempControl.pidDriveMin,1,1,255);
        break;
    case UI_ACTION_DRIVE_MAX:
        INCREMENT_MIN_MAX(Extruder::current->tempControl.pidDriveMax,1,1,255);
        break;
    case UI_ACTION_PID_MAX:
        INCREMENT_MIN_MAX(Extruder::current->tempControl.pidMax,1,1,255);
        break;
#endif
	case UI_ACTION_X_LENGTH:
			INCREMENT_MIN_MAX(Printer::xLength,1,0,250);
		break;
	case UI_ACTION_Y_LENGTH:
			INCREMENT_MIN_MAX(Printer::yLength,1,0,250);
		break;
	case UI_ACTION_Z_LENGTH:
			INCREMENT_MIN_MAX(Printer::zLength,1,0,250);
		break;
    case UI_ACTION_X_OFFSET:
        INCREMENT_MIN_MAX(Extruder::current->xOffset,1,-99999,99999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_Y_OFFSET:
        INCREMENT_MIN_MAX(Extruder::current->yOffset,1,-99999,99999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_STEPS:
        INCREMENT_MIN_MAX(Extruder::current->stepsPerMM,1,1,9999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_ACCELERATION:
        INCREMENT_MIN_MAX(Extruder::current->maxAcceleration,10,10,99999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_MAX_FEEDRATE:
        INCREMENT_MIN_MAX(Extruder::current->maxFeedrate,1,1,999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_START_FEEDRATE:
        INCREMENT_MIN_MAX(Extruder::current->maxStartFeedrate,1,1,999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_HEATMANAGER:
        INCREMENT_MIN_MAX(Extruder::current->tempControl.heatManager,1,0,3);
        break;
    case UI_ACTION_EXTR_WATCH_PERIOD:
        INCREMENT_MIN_MAX(Extruder::current->watchPeriod,1,0,999);
        break;
#if RETRACT_DURING_HEATUP
    case UI_ACTION_EXTR_WAIT_RETRACT_TEMP:
        INCREMENT_MIN_MAX(Extruder::current->waitRetractTemperature,1,100,UI_SET_MAX_EXTRUDER_TEMP);
        break;
    case UI_ACTION_EXTR_WAIT_RETRACT_UNITS:
        INCREMENT_MIN_MAX(Extruder::current->waitRetractUnits,1,0,99);
        break;
#endif
#ifdef USE_ADVANCE
#ifdef ENABLE_QUADRATIC_ADVANCE
    case UI_ACTION_ADVANCE_K:
        INCREMENT_MIN_MAX(Extruder::current->advanceK,1,0,200);
        break;
#endif
    case UI_ACTION_ADVANCE_L:
        INCREMENT_MIN_MAX(Extruder::current->advanceL,1,0,600);
        break;
#endif
    }
#if UI_AUTORETURN_TO_MENU_AFTER!=0
    ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;
#endif
#if UI_AUTOLIGHTOFF_AFTER!=0
    UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
#endif

#endif
}

void UIDisplay::finishAction(int action)
{
}

bool UIDisplay::confirmationDialog(char * title,char * line1,char * line2,int type, bool defaultresponse)
{
bool response=defaultresponse;
bool process_it=true;
int previousaction=0;
int tmpmenulevel = menuLevel;
if (menuLevel>3)menuLevel=3;
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
bool btmp_autoreturn=benable_autoreturn; //save current value
benable_autoreturn=false;//desactivate no need to test if active or not
#endif
//init dialog strings
col=0;
parse(title,false);
strcpy(uipagedialog[0], printCols);
col=0;
parse(line1,true);
strcpy(uipagedialog[1], printCols);
col=0;
parse(line2,true);
strcpy(uipagedialog[2], printCols);
if (response) strcpy(uipagedialog[3],UI_TEXT_YES_SELECTED); //default for response=true
else strcpy(uipagedialog[3],UI_TEXT_NO_SELECTED); //default for response=false
//push dialog
pushMenu(&ui_menu_confirmation,true);
//ensure last button pressed is not OK to have the dialog closing too fast
while(lastButtonAction==UI_ACTION_OK){
	Commands::checkForPeriodicalActions();
	}
delay(500);
//main loop
while (process_it)
	{
    Printer::setMenuMode(MENU_MODE_PRINTING,true);
    Commands::delay_flag_change=0;
	//process critical actions
	Commands::checkForPeriodicalActions();
	//be sure button is pressed and not same one 
	if (lastButtonAction!=previousaction)
		{
		 previousaction=lastButtonAction;
		 //wake up light if power saving has been launched
		#if UI_AUTOLIGHTOFF_AFTER!=0
		if (EEPROM::timepowersaving>0)
			{
			UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
			#if CASE_LIGHTS_PIN > 0
			if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
				{
				TOGGLE(CASE_LIGHTS_PIN);
				}
			#endif
			#if defined(UI_BACKLIGHT_PIN)
			if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
			#endif
			}
		#endif
		//if button ok we are done
		if (lastButtonAction==UI_ACTION_OK)
			{
			process_it=false;
			}
		//if left key then select Yes
		 else if (lastButtonAction==UI_ACTION_BACK)
			{
			strcpy(uipagedialog[3],UI_TEXT_YES_SELECTED);
			response=true;
			}
		//if right key then select No
		else if (lastButtonAction==UI_ACTION_RIGHT_KEY)
			{
			strcpy(uipagedialog[3],UI_TEXT_NO_SELECTED);
			response=false;
			}
		if(previousaction!=0)BEEP_SHORT;
		refreshPage();
		}
	}//end while
 menuLevel=tmpmenulevel;
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
			if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
			{
			benable_autoreturn=true;//reactivate 
			ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
			}
 #endif
return response;
}
// Actions are events from user input. Depending on the current state, each
// action can behave differently. Other actions do always the same like home, disable extruder etc.
void UIDisplay::executeAction(int action)
{
#if UI_HAS_KEYS==1
    bool skipBeep = false;
	bool process_it=false;
	int previousaction=0;
	millis_t printedTime;
    millis_t currentTime;
	int load_dir=1;
	int extruderid=0;
	int tmpextruderid=0;
	int step =0;
	int counter;
#if UI_AUTOLIGHTOFF_AFTER!=0
    if (EEPROM::timepowersaving>0)
	{
	UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
	#if CASE_LIGHTS_PIN > 0
	if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
		{
		TOGGLE(CASE_LIGHTS_PIN);
		}
	#endif
	#if defined(UI_BACKLIGHT_PIN)
    if (!(READ(UI_BACKLIGHT_PIN)))WRITE(UI_BACKLIGHT_PIN, HIGH);
	#endif
	}
#endif
    if(action & UI_ACTION_TOPMENU)   // Go to start menu
    {
        action -= UI_ACTION_TOPMENU;
        menuLevel = 0;
        activeAction = 0;
        action=0;
        playsound(4000,240);
		playsound(5000,240);
    }
    if(action>=2000 && action<3000)
    {
        setStatusP(ui_action);
    }
    else
        switch(action)
        {
		//fast switch UI without saving
		case UI_ACTION_OK_PREV_RIGHT:
		if (display_mode&ADVANCED_MODE)display_mode=EASY_MODE;
		else display_mode=ADVANCED_MODE;
		playsound(5000,240);
		playsound(5000,240);
		break;
		case UI_ACTION_RIGHT_KEY:
        case UI_ACTION_OK:
            okAction();
            skipBeep=true; // Prevent double beep
            break;
        case UI_ACTION_BACK:
            if(menuLevel>0) menuLevel--;
			else menuPos[0]=0;
            Printer::setAutomount(false);
            activeAction = 0;
            break;
        case UI_ACTION_NEXT:
            nextPreviousAction(1);
            break;
        case UI_ACTION_PREVIOUS:
            nextPreviousAction(-1);
            break;
        case UI_ACTION_MENU_UP:
            if(menuLevel>0) menuLevel--;
            break;
        case UI_ACTION_TOP_MENU:
            menuLevel = 0;
			menuPos[0]=0;
			 activeAction = 0;
			action=0;
            playsound(4000,240);
		    playsound(5000,240);
            break;
        case UI_ACTION_EMERGENCY_STOP:
            Commands::emergencyStop();
            break;
        case UI_ACTION_VERSION:
            pushMenu(&ui_page_version,true);
            break;
        case UI_ACTION_HOME_ALL:
			{
			int tmpmenu=menuLevel;
			int tmpmenupos=menuPos[menuLevel];
			UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
			menuLevel=0;
			menuPos[0] = 0;
			refreshPage();
            Printer::homeAxis(true,true,true);
            Commands::printCurrentPosition();
            menuLevel=tmpmenu;
			menuPos[menuLevel]=tmpmenupos;
			menu[menuLevel]=tmpmen;
            refreshPage();
            break;
			}
        case UI_ACTION_HOME_X:
			{
			int tmpmenu=menuLevel;
			int tmpmenupos=menuPos[menuLevel];
			UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
			menuLevel=0;
			menuPos[0] = 0;
			refreshPage();
            Printer::homeAxis(true,false,false);
            Commands::printCurrentPosition();
            menuLevel=tmpmenu;
			menuPos[menuLevel]=tmpmenupos;
			menu[menuLevel]=tmpmen;
            refreshPage();
            break;
			}
        case UI_ACTION_HOME_Y:
			{
			int tmpmenu=menuLevel;
			int tmpmenupos=menuPos[menuLevel];
			UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
			menuLevel=0;
			menuPos[0] = 0;
			refreshPage();
            Printer::homeAxis(false,true,false);
            Commands::printCurrentPosition();
			menuLevel=tmpmenu;
			menuPos[menuLevel]=tmpmenupos;
			menu[menuLevel]=tmpmen;
            refreshPage();
            break;
           }
        case UI_ACTION_HOME_Z:
			{
			int tmpmenu=menuLevel;
			int tmpmenupos=menuPos[menuLevel];
			UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
			menuLevel=0;
			menuPos[0] = 0;
			refreshPage();
            Printer::homeAxis(false,false,true);
            Commands::printCurrentPosition();
			menuLevel=tmpmenu;
			menuPos[menuLevel]=tmpmenupos;
			menu[menuLevel]=tmpmen;
            refreshPage();
            break;
			}
        case UI_ACTION_SET_ORIGIN:
            Printer::setOrigin(0,0,0);
            break;
        case UI_ACTION_DEBUG_ECHO:
            if(Printer::debugEcho()) Printer::debugLevel-=1;
            else Printer::debugLevel+=1;
            break;
        case UI_ACTION_DEBUG_INFO:
            if(Printer::debugInfo()) Printer::debugLevel-=2;
            else Printer::debugLevel+=2;
            break;
        case UI_ACTION_DEBUG_ERROR:
            if(Printer::debugErrors()) Printer::debugLevel-=4;
            else Printer::debugLevel+=4;
            break;
        case UI_ACTION_DEBUG_DRYRUN:
            if(Printer::debugDryrun()) Printer::debugLevel-=8;
            else Printer::debugLevel+=8;
            if(Printer::debugDryrun())   // simulate movements without printing
            {
                Extruder::setTemperatureForExtruder(0,0);
#if NUM_EXTRUDER>1
                Extruder::setTemperatureForExtruder(0,1);
#endif
#if NUM_EXTRUDER>2
                Extruder::setTemperatureForExtruder(0,2);
#endif
#if HAVE_HEATED_BED==true
                Extruder::setHeatedBedTemperature(0);
#endif
            }
            break;
        case UI_ACTION_POWER:
#if PS_ON_PIN>=0 // avoid compiler errors when the power supply pin is disabled
            Commands::waitUntilEndOfAllMoves();
            SET_OUTPUT(PS_ON_PIN); //GND
            TOGGLE(PS_ON_PIN);
#endif
            break;
    case UI_ACTION_DISPLAY_MODE:
		if (display_mode&ADVANCED_MODE)display_mode=EASY_MODE;
		else display_mode=ADVANCED_MODE;
		//save directly to eeprom
         EEPROM:: update(EPR_DISPLAY_MODE,EPR_TYPE_BYTE,UIDisplay::display_mode,0);
    break;
            
	#if FEATURE_BEEPER
	case UI_ACTION_SOUND:
	HAL::enablesound=!HAL::enablesound;
	//save directly to eeprom
    EEPROM:: update(EPR_SOUND_ON,EPR_TYPE_BYTE,HAL::enablesound,0);
	UI_STATUS(UI_TEXT_SOUND_ONOF);
	break;
	#endif
#if UI_AUTOLIGHTOFF_AFTER >0
	case UI_ACTION_KEEP_LIGHT_ON:
		EEPROM::bkeeplighton=!EEPROM::bkeeplighton;
		//save directly to eeprom
		EEPROM:: update(EPR_KEEP_LIGHT_ON,EPR_TYPE_BYTE,EEPROM::bkeeplighton,0);
		UI_STATUS(UI_TEXT_KEEP_LIGHT_ON);
	break;
	case UI_ACTION_TOGGLE_POWERSAVE:
		if (EEPROM::timepowersaving==0) EEPROM::timepowersaving = 1000*60;// move to 1 min
		else if (EEPROM::timepowersaving==(1000 * 60) )EEPROM::timepowersaving = 1000*60*5;// move to 5 min
		else if (EEPROM::timepowersaving==(1000 * 60 * 5)) EEPROM::timepowersaving = 1000*60*15;// move to 15 min
		else if (EEPROM::timepowersaving==(1000 * 60 * 15)) EEPROM::timepowersaving = 1000*60*30;// move to 30 min
		else EEPROM::timepowersaving = 0;// move to off
        //reset counter
		if (EEPROM::timepowersaving>0)UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
        //save directly to eeprom 1 by one as each setting is reloaded by reading eeprom
        EEPROM:: update(EPR_POWERSAVE_AFTER_TIME,EPR_TYPE_LONG,EEPROM::timepowersaving,0);
		//apply to stepper
		stepperInactiveTime=EEPROM::timepowersaving;
        EEPROM:: update(EPR_STEPPER_INACTIVE_TIME,EPR_TYPE_LONG,stepperInactiveTime,0);
		//apply to inactivity timer
		maxInactiveTime=EEPROM::timepowersaving;
        EEPROM:: update(EPR_MAX_INACTIVE_TIME,EPR_TYPE_LONG,maxInactiveTime,0);
        UI_STATUS(UI_TEXT_POWER_SAVE);
	break;
#endif
 #if defined(FIL_SENSOR1_PIN)
	 case UI_ACTION_FILAMENT_SENSOR_ONOFF:
		 EEPROM::busesensor=!EEPROM::busesensor;
		 //save directly to eeprom
        EEPROM:: update(EPR_FIL_SENSOR_ON,EPR_TYPE_BYTE,EEPROM::busesensor,0);
		UI_STATUS(UI_TEXT_FIL_SENSOR_ONOFF);
	 break;
#endif
 #if defined(TOP_SENSOR_PIN)
	 case UI_ACTION_TOP_SENSOR_ONOFF:
		 EEPROM::btopsensor=!EEPROM::btopsensor;
		 //save directly to eeprom
        EEPROM:: update(EPR_TOP_SENSOR_ON,EPR_TYPE_BYTE,EEPROM::btopsensor,0);
		UI_STATUS(UI_TEXT_TOP_SENSOR_ONOFF);
	 break;
#endif
#if CASE_LIGHTS_PIN > 0
        case UI_ACTION_LIGHTS_ONOFF:
            TOGGLE(CASE_LIGHTS_PIN);
			if (READ(CASE_LIGHTS_PIN))
			EEPROM::buselight=true;
			else
			EEPROM::buselight=false;
			//save directly to eeprom
            EEPROM:: update(EPR_LIGHT_ON,EPR_TYPE_BYTE,EEPROM::buselight,0);
            UI_STATUS(UI_TEXT_LIGHTS_ONOFF);
            break;
#endif
#if ENABLE_CLEAN_NOZZLE==1
	case UI_ACTION_CLEAN_NOZZLE:
	    {//be sure no issue
		if(reportTempsensorError() or Printer::debugDryrun()) break;
		//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
		bool btmp_autoreturn=benable_autoreturn; //save current value
		benable_autoreturn=false;//desactivate no need to test if active or not
#endif
        //save current target temp
        float extrudertarget1=extruder[0].tempControl.targetTemperatureC;
        #if NUM_EXTRUDER>1
        float extrudertarget2=extruder[1].tempControl.targetTemperatureC;
        #endif
        #if HAVE_HEATED_BED==true
        float bedtarget=heatedBedController.targetTemperatureC;
        #endif
		int status=STATUS_OK;
		int tmpmenu=menuLevel;
		int tmpmenupos=menuPos[menuLevel];
		UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
		if(menuLevel>0) menuLevel--;
		pushMenu(&ui_menu_clean_nozzle_page,true);
		process_it=true;
		printedTime = HAL::timeInMilliseconds();
		step=STEP_HEATING;
		while (process_it)
		{
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
		Commands::delay_flag_change=0;
		Commands::checkForPeriodicalActions();
		currentTime = HAL::timeInMilliseconds();
        if( (currentTime - printedTime) > 1000 )   //Print Temp Reading every 1 second while heating up.
            {
            Commands::printTemperatures();
            printedTime = currentTime;
           }
		 switch(step)
		 {
		 case  STEP_HEATING:
            if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
           Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
		   #if NUM_EXTRUDER>1
           if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
		   #endif
			step =  STEP_WAIT_FOR_TEMPERATURE;
		 break;
		 case STEP_WAIT_FOR_TEMPERATURE:

			UI_STATUS(UI_TEXT_HEATING);
			//no need to be extremely accurate so no need stable temperature 
			if(abs(extruder[0].tempControl.currentTemperatureC- extruder[0].tempControl.targetTemperatureC)<2)
				{
				step = STEP_CLEAN_NOOZLE;
				}
			#if NUM_EXTRUDER==2
			if(!((abs(extruder[1].tempControl.currentTemperatureC- extruder[1].tempControl.targetTemperatureC)<2) && (step == STEP_CLEAN_NOOZLE)))
				{
				step = STEP_WAIT_FOR_TEMPERATURE;
				}
			#endif
		 break;
		 case STEP_CLEAN_NOOZLE:
			//clean
			Printer::cleanNozzle(false);
			//move to a position to let user to clean manually
 			#if DAVINCI==1 //be sure we cannot hit cleaner on 1.0
			if(Printer::currentPosition[Y_AXIS]<=20)
				{
				Printer::moveToReal(0,0,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[0]);//go 0,0 or xMin,yMin ?
				Printer::moveToReal(0,20,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
				}
			#endif
			Printer::moveToReal(Printer::xLength/2,Printer::yLength-20,150,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
			Commands::waitUntilEndOfAllMoves();
			step =STEP_WAIT_FOR_OK;
			playsound(3000,240);
			playsound(4000,240);
			playsound(5000,240);
		 break;
		 case STEP_WAIT_FOR_OK:
		 UI_STATUS(UI_TEXT_WAIT_FOR_OK);
		 //just need to wait for key to be pressed
		 break;
		 }
		 //check what key is pressed
		 if (previousaction!=lastButtonAction)
			{
			previousaction=lastButtonAction;
			if(previousaction!=0)BEEP_SHORT;
			 if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_WAIT_FOR_OK))
				{//we are done
				process_it=false;
				playsound(5000,240);
				playsound(3000,240);
				Printer::homeAxis(true,true,false);
				}
			 if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
				{
				if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_CLEANING_NOZZLE))
					{
					UI_STATUS(UI_TEXT_CANCELED);
					status=STATUS_CANCEL;
					process_it=false;
					}
				else
					{//we continue as before
					if(menuLevel>0) menuLevel--;
					pushMenu(&ui_menu_clean_nozzle_page,true);
					}
				delay(100);
				}
			 //wake up light if power saving has been launched
			#if UI_AUTOLIGHTOFF_AFTER!=0
			if (EEPROM::timepowersaving>0)
				{
				UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
				#if CASE_LIGHTS_PIN > 0
				if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
					{
					TOGGLE(CASE_LIGHTS_PIN);
					}
				#endif
				#if defined(UI_BACKLIGHT_PIN)
				if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
				#endif
				}
			#endif
			}
		}
		//cool down if necessary
		Extruder::setTemperatureForExtruder(extrudertarget1,0);
		#if NUM_EXTRUDER>1
        Extruder::setTemperatureForExtruder(extrudertarget2,1);
		#endif
		if(status==STATUS_OK)
			{
			 UI_STATUS(UI_TEXT_PRINTER_READY);
			menuLevel=tmpmenu;
			menuPos[menuLevel]=tmpmenupos;
			menu[menuLevel]=tmpmen;
			}
		else if (status==STATUS_CANCEL)
			{
			while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions();
			UI_STATUS(UI_TEXT_CANCELED);
			menuLevel=0;
			}
		refreshPage();
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
			if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
			{
			benable_autoreturn=true;//reactivate 
			ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
			}
 #endif
        break;
		}
#endif	

#if ENABLE_CLEAN_DRIPBOX==1
	case UI_ACTION_CLEAN_DRIPBOX:
		{
		process_it=true;
		printedTime = HAL::timeInMilliseconds();
		step=STEP_CLEAN_DRIPBOX;
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
		bool btmp_autoreturn=benable_autoreturn; //save current value
		benable_autoreturn=false;//desactivate no need to test if active or not
#endif
		int tmpmenu=menuLevel;
		int tmpmenupos=menuPos[menuLevel];
		UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
		menuLevel=0;
		refreshPage();
		//need to home if not
		if(!Printer::isHomed()) Printer::homeAxis(true,true,true);
        else 
            {//put proper position in case position has been manualy changed no need to home Z as cannot be manualy changed and in case of something on plate it could be catastrophic
                Printer::homeAxis(true,true,false);
            }
        UI_STATUS(UI_TEXT_PREPARING);
		while (process_it)
		{
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
		Commands::delay_flag_change=0;
		Commands::checkForPeriodicalActions();
		currentTime = HAL::timeInMilliseconds();
		 switch(step)
		 {
		 case STEP_CLEAN_DRIPBOX:
			//move to a position to let user to clean manually
			#if DAVINCI==1 //be sure we cannot hit cleaner on 1.0
            if(Printer::currentPosition[Y_AXIS]<=20)
				{
				Printer::moveToReal(Printer::xMin,Printer::yMin,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);//go 0,0 or xMin,yMin ?
				 PrintLine::moveRelativeDistanceInStepsReal(0,Printer::axisStepsPerMM[Y_AXIS]*20,0,0,Printer::homingFeedrate[Y_AXIS],true);
				}
			#endif
            Commands::waitUntilEndOfAllMoves();
            PrintLine::moveRelativeDistanceInStepsReal(0,0,Printer::axisStepsPerMM[Z_AXIS]*10,0,Printer::homingFeedrate[Z_AXIS],true);
			Printer::moveToReal(Printer::xLength/2,Printer::yLength-20,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::maxFeedrate[X_AXIS]);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,150,IGNORE_COORDINATE,Printer::maxFeedrate[Z_AXIS]);
			Commands::waitUntilEndOfAllMoves();
			step =STEP_CLEAN_DRIPBOX_WAIT_FOR_OK;
			playsound(3000,240);
			playsound(4000,240);
			playsound(5000,240);
			pushMenu(&ui_menu_clean_dripbox_page,true);
		 break;
		 case STEP_CLEAN_DRIPBOX_WAIT_FOR_OK:
		 UI_STATUS(UI_TEXT_WAIT_FOR_OK);
		 //just need to wait for key to be pressed
		 break;
		 }
		 //check what key is pressed
		 if (previousaction!=lastButtonAction)
			{
			previousaction=lastButtonAction;
			if(previousaction!=0)BEEP_SHORT;
			 if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_CLEAN_DRIPBOX_WAIT_FOR_OK))
				{//we are done
				process_it=false;
				playsound(5000,240);
				playsound(3000,240);
		        menuLevel=0;
		        refreshPage();
				Printer::homeAxis(true,true,true);
				}
			//there is no step that user can cancel but keep it if function become more complex
			 /*if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
				{
				if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_CLEANING_DRIPBOX))
					{
					UI_STATUS(UI_TEXT_CANCELED);
					process_it=false;
					}
				else
					{//we continue as before
					if(menuLevel>0) menuLevel--;
					pushMenu(&ui_menu_clean_dripbox_page,true);
					}
				delay(100);
				}*/
			 //wake up light if power saving has been launched
			#if UI_AUTOLIGHTOFF_AFTER!=0
			if (EEPROM::timepowersaving>0)
				{
				UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
				#if CASE_LIGHTS_PIN > 0
				if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
					{
					TOGGLE(CASE_LIGHTS_PIN);
					}
				#endif
				#if defined(UI_BACKLIGHT_PIN)
				if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
				#endif
				}
			#endif
			}
		}
		menuLevel=tmpmenu;
		menuPos[menuLevel]=tmpmenupos;
		menu[menuLevel]=tmpmen;
		refreshPage();
		UI_STATUS(UI_TEXT_PRINTER_READY);
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
			if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
			{
			benable_autoreturn=true;//reactivate 
			ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
			}
 #endif
        break;
		}
#endif
		
		case UI_ACTION_NO_FILAMENT:
		{
		int tmpmenu=menuLevel;
		int tmpmenupos=menuPos[menuLevel];
		UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
		process_it=true;
		sd.pausePrint(true);
		//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
		bool btmp_autoreturn=benable_autoreturn; //save current value
		benable_autoreturn=false;//desactivate no need to test if active or not
#endif
		playsound(3000,240);
		playsound(4000,240);
		playsound(5000,240);
		if(menuLevel>0) menuLevel--;
		pushMenu(&ui_no_filament_box,true);
		previousaction=0;
		while (process_it)
		{
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
		Commands::delay_flag_change=0;
		Commands::checkForPeriodicalActions();
		if (previousaction!=lastButtonAction)
			{
			previousaction=lastButtonAction;
			if(previousaction!=0)BEEP_SHORT;
			if (lastButtonAction==UI_ACTION_OK)
				{
					process_it=false;
				}
			delay(100);
				}
			 //wake up light if power saving has been launched
			#if UI_AUTOLIGHTOFF_AFTER!=0
			if (EEPROM::timepowersaving>0)
				{
				UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
				#if CASE_LIGHTS_PIN > 0
				if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
					{
					TOGGLE(CASE_LIGHTS_PIN);
					}
				#endif
				#if defined(UI_BACKLIGHT_PIN)
				if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
				#endif
				}
			#endif
			}
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
			if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
			{
			benable_autoreturn=true;//reactivate 
			ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
			}
 #endif
		//menuLevel=0;
		//pushMenu(&ui_menu_load_unload,true);
		menuLevel=tmpmenu;
		menuPos[menuLevel]=tmpmenupos;
		menu[menuLevel]=tmpmen;
		break;
		}
		
		case UI_ACTION_LOAD_EXTRUDER_0:
		case UI_ACTION_UNLOAD_EXTRUDER_0:
		#if NUM_EXTRUDER > 1
		case UI_ACTION_LOAD_EXTRUDER_1:
		case UI_ACTION_UNLOAD_EXTRUDER_1:
		#endif
		{
		int status=STATUS_OK;
		int tmpmenu=menuLevel;
		int tmpmenupos=menuPos[menuLevel];
		UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
		bool btmp_autoreturn=benable_autoreturn; //save current value
		benable_autoreturn=false;//desactivate no need to test if active or not
#endif
		if (action== UI_ACTION_LOAD_EXTRUDER_0)
			{
			load_dir=1;
			extruderid=0;
			}
		else if (action== UI_ACTION_UNLOAD_EXTRUDER_0)
			{
			load_dir=-1;
			extruderid=0;
			}
		#if NUM_EXTRUDER > 1
		else if (action== UI_ACTION_LOAD_EXTRUDER_1)
			{
			load_dir=1;
			extruderid=1;
			}
		else if (action== UI_ACTION_UNLOAD_EXTRUDER_1)
			{
			load_dir=-1;
			extruderid=1;
			}
		#endif
		tmpextruderid=Extruder::current->id;
		Extruder::selectExtruderById(extruderid,false);
         //save current target temp
        float extrudertarget=extruder[extruderid].tempControl.targetTemperatureC;
	    //be sure no issue
		if(reportTempsensorError() or Printer::debugDryrun()) break;
		UI_STATUS(UI_TEXT_HEATING);
		 if(menuLevel>0) menuLevel--;
		pushMenu(&ui_menu_heatextruder_page,true);
		process_it=true;
		printedTime = HAL::timeInMilliseconds();
		step=STEP_EXT_HEATING;
		while (process_it)
		{
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
		Commands::delay_flag_change=0;
		Commands::checkForPeriodicalActions();
		currentTime = HAL::timeInMilliseconds();
        if( (currentTime - printedTime) > 1000 )   //Print Temp Reading every 1 second while heating up.
            {
            Commands::printTemperatures();
            printedTime = currentTime;
           }
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
		Commands::delay_flag_change=0;
		 switch(step)
		 {
		 case STEP_EXT_HEATING:
           if (extruder[extruderid].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                   Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,extruderid);
			step =  STEP_EXT_WAIT_FOR_TEMPERATURE;
		 break;
		 case STEP_EXT_WAIT_FOR_TEMPERATURE:
			UI_STATUS(UI_TEXT_HEATING);
			//no need to be extremely accurate so no need stable temperature 
			if(abs(extruder[extruderid].tempControl.currentTemperatureC- extruder[extruderid].tempControl.targetTemperatureC)<2)
				{
				step = STEP_EXT_ASK_FOR_FILAMENT;
				playsound(3000,240);
				playsound(4000,240);
				playsound(5000,240);
				UI_STATUS(UI_TEXT_WAIT_FILAMENT);
				}
			counter=0;
		 break;
		 case STEP_EXT_ASK_FOR_FILAMENT:
		 if (load_dir==-1) //no need to ask
			{
			step=STEP_EXT_LOAD_UNLOAD;
			}
		else 
				{
				if (extruderid==0)//filament sensor override pushing ok button to start if filament detected, if not user still can push Ok to start 
					{
						#if defined(FIL_SENSOR1_PIN)
							if(EEPROM::busesensor && !READ(FIL_SENSOR1_PIN))step=STEP_EXT_LOAD_UNLOAD;
						#endif
					}
				else
					{
						#if defined(FIL_SENSOR2_PIN)
							if(EEPROM::busesensor &&!READ(FIL_SENSOR2_PIN))step=STEP_EXT_LOAD_UNLOAD;
						#endif
					}
			    }
		if((step==STEP_EXT_LOAD_UNLOAD)&&(load_dir==1))
			{
				playsound(5000,240);
				playsound(3000,240);
				UI_STATUS(UI_TEXT_PUSH_FILAMENT);
			}
		//ok key is managed in key section so if wait for press ok - just do nothing
		 break;
		 case STEP_EXT_LOAD_UNLOAD:
			if(!PrintLine::hasLines())
			{
				//load or unload
				counter++;
				if (load_dir==-1)
					{
					UI_STATUS(UI_TEXT_UNLOADING_FILAMENT);
					PrintLine::moveRelativeDistanceInSteps(0,0,0,load_dir * Printer::axisStepsPerMM[E_AXIS],4,false,false);
					if (extruderid==0)//filament sensor override to stop earlier
					{
						#if defined(FIL_SENSOR1_PIN)
							if(EEPROM::busesensor &&READ(FIL_SENSOR1_PIN))
								{
								process_it=false;
								}
						#endif
					}
				else
					{
						#if defined(FIL_SENSOR2_PIN)
							if(EEPROM::busesensor &&READ(FIL_SENSOR2_PIN))
								{
								process_it=false;
								}
						#endif
					}
					}
				else
					{
					UI_STATUS(UI_TEXT_LOADING_FILAMENT);
					PrintLine::moveRelativeDistanceInSteps(0,0,0,load_dir * Printer::axisStepsPerMM[E_AXIS],2,false,false);
					}
				if (counter>=60)step = STEP_EXT_ASK_CONTINUE;
			}	
			break;
		 case STEP_EXT_ASK_CONTINUE:
			if(!PrintLine::hasLines())
			{
				//ask to redo or stop
				if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CONTINUE_ACTION,UI_TEXT_PUSH_FILAMENT,UI_CONFIRMATION_TYPE_YES_NO,true))
						{
						 if(menuLevel>0) menuLevel--;
						pushMenu(&ui_menu_heatextruder_page,true);
						counter=0;
						step =STEP_EXT_LOAD_UNLOAD;
						}
					else
						{//we end
						process_it=false;
						}
					delay(100);
			}	
		 break;
		 }
		 //check what key is pressed
		 if (previousaction!=lastButtonAction)
			{
			previousaction=lastButtonAction;
			if(previousaction!=0)BEEP_SHORT;
			if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_EXT_ASK_FOR_FILAMENT))
				{//we can continue
				step=STEP_EXT_LOAD_UNLOAD;
				playsound(5000,240);
				playsound(3000,240);
				UI_STATUS(UI_TEXT_PUSH_FILAMENT);
				}
			 if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
				{
				if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_LOADUNLOAD_FILAMENT))
					{
					UI_STATUS(UI_TEXT_CANCELED);
					status=STATUS_CANCEL;
					process_it=false;
					}
				else
					{//we continue as before
					if(menuLevel>0) menuLevel--;
					pushMenu(&ui_menu_heatextruder_page,true);
					}
				delay(100);
				}
			 //wake up light if power saving has been launched
			#if UI_AUTOLIGHTOFF_AFTER!=0
			if (EEPROM::timepowersaving>0)
				{
				UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
				#if CASE_LIGHTS_PIN > 0
				if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
					{
					TOGGLE(CASE_LIGHTS_PIN);
					}
				#endif
				#if defined(UI_BACKLIGHT_PIN)
				if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
				#endif
				}
			#endif
			}
		}
		
		//cool down if necessary
		Extruder::setTemperatureForExtruder(extrudertarget,extruderid);
        Extruder::selectExtruderById(tmpextruderid,false);
		if(status==STATUS_OK)
			{
			UI_STATUS(UI_TEXT_PRINTER_READY);
			menuLevel=tmpmenu;
			menuPos[menuLevel]=tmpmenupos;
			menu[menuLevel]=tmpmen;
			}
		else if (status==STATUS_CANCEL)
			{
			while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions();
			UI_STATUS(UI_TEXT_CANCELED);
			menuLevel=0;
			}
		refreshPage();
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
			if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
			{
			benable_autoreturn=true;//reactivate 
			ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
			}
 #endif
        break;
		}
#if FEATURE_AUTOLEVEL
        case UI_ACTION_AUTOLEVEL_ON :
            Printer::setAutolevelActive(!Printer:: isAutolevelActive());
            EEPROM:: update(EPR_AUTOLEVEL_ACTIVE,EPR_TYPE_BYTE,Printer:: isAutolevelActive(),0);
        break;
		case UI_ACTION_AUTOLEVEL:
		{
        Z_probe[0]=-1000;
        Z_probe[1]=-1000;
        Z_probe[2]=-1000;
	    //be sure no issue
		if(reportTempsensorError() or Printer::debugDryrun()) break;
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
		bool btmp_autoreturn=benable_autoreturn; //save current value
		benable_autoreturn=false;//desactivate no need to test if active or not
#endif
		int tmpmenu=menuLevel;
		int tmpmenupos=menuPos[menuLevel];
		UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
        //save current target temp
        float extrudertarget1=extruder[0].tempControl.targetTemperatureC;
        #if NUM_EXTRUDER>1
        float extrudertarget2=extruder[1].tempControl.targetTemperatureC;
        #endif
        #if HAVE_HEATED_BED==true
        float bedtarget=heatedBedController.targetTemperatureC;
        #endif
		#if ENABLE_CLEAN_NOZZLE==1
		//ask for user if he wants to clean nozzle and plate
			if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_CLEAN1,UI_TEXT_CLEAN2))
					{
                    //heat extruders first to keep them hot
                    if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                   Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
                   #if NUM_EXTRUDER>1
                   if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                    Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
                   #endif
					executeAction(UI_ACTION_CLEAN_NOZZLE);
					}
			
		 #endif
		 if(menuLevel>0) menuLevel--;
		 pushMenu(&ui_menu_autolevel_page,true);
		UI_STATUS(UI_TEXT_HEATING);
		process_it=true;
		printedTime = HAL::timeInMilliseconds();
		step=STEP_AUTOLEVEL_HEATING;
        float h1,h2,h3;
		float oldFeedrate = Printer::feedrate;
		int xypoint=1;
		float z = 0;
		int32_t sum ,probeDepth;
		int32_t lastCorrection; 
		float distance;
		int status=STATUS_OK;
		float currentzMin = Printer::zMin;
 
		while (process_it)
		{
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
		Commands::delay_flag_change=0;
		Commands::checkForPeriodicalActions();
		currentTime = HAL::timeInMilliseconds();
        if( (currentTime - printedTime) > 1000 )   //Print Temp Reading every 1 second while heating up.
            {
            Commands::printTemperatures();
            printedTime = currentTime;
           }
		 switch(step)
		 {
		 case STEP_AUTOLEVEL_HEATING:
            if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
           Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
		   #if NUM_EXTRUDER>1
           if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
		   #endif
		   #if HAVE_HEATED_BED==true
           if (heatedBedController.targetTemperatureC<EEPROM::ftemp_bed_abs)
            Extruder::setHeatedBedTemperature(EEPROM::ftemp_bed_abs);
		   #endif
		   step =  STEP_AUTOLEVEL_WAIT_FOR_TEMPERATURE;
		 break;
		 case STEP_AUTOLEVEL_WAIT_FOR_TEMPERATURE:
			UI_STATUS(UI_TEXT_HEATING);
			//no need to be extremely accurate so no need stable temperature 
			if(abs(extruder[0].tempControl.currentTemperatureC- extruder[0].tempControl.targetTemperatureC)<2)
				{
				step = STEP_ZPROBE_SCRIPT;
				}
			#if NUM_EXTRUDER==2
			if(!((abs(extruder[1].tempControl.currentTemperatureC- extruder[1].tempControl.targetTemperatureC)<2) && (step == STEP_ZPROBE_SCRIPT)))
				{
				step = STEP_AUTOLEVEL_WAIT_FOR_TEMPERATURE;
				}
			#endif
			#if HAVE_HEATED_BED==true
			if(!((abs(heatedBedController.currentTemperatureC-heatedBedController.targetTemperatureC)<2) && (step == STEP_ZPROBE_SCRIPT)))
				{
				step = STEP_AUTOLEVEL_WAIT_FOR_TEMPERATURE;
				}
			#endif
		 break;
		 case STEP_ZPROBE_SCRIPT:
            //Home first
            Printer::homeAxis(true,true,true);
            //then put bed +10mm
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
            Commands::waitUntilEndOfAllMoves();
		 	//Startup script
			GCode::executeFString(Com::tZProbeStartScript);
			step = STEP_AUTOLEVEL_START;
			break;
		 case STEP_AUTOLEVEL_START:
			Printer::coordinateOffset[0] = Printer::coordinateOffset[1] = Printer::coordinateOffset[2] = 0;
			Printer::setAutolevelActive(false); // iterate
			step = STEP_AUTOLEVEL_MOVE;
            Z_probe[0]=-1000;
            Z_probe[1]=-1000;
            Z_probe[2]=-1000;
            if(menuLevel>0) menuLevel--;
		    pushMenu(&ui_menu_autolevel_results_page,true);
			break;
			
		case STEP_AUTOLEVEL_MOVE:
			UI_STATUS(UI_TEXT_ZPOSITION);
			if (xypoint==1)Printer::moveTo(EEPROM::zProbeX1(),EEPROM::zProbeY1(),IGNORE_COORDINATE,IGNORE_COORDINATE,EEPROM::zProbeXYSpeed());

			else if (xypoint==2) Printer::moveTo(EEPROM::zProbeX2(),EEPROM::zProbeY2(),IGNORE_COORDINATE,IGNORE_COORDINATE,EEPROM::zProbeXYSpeed());
			else Printer::moveTo(EEPROM::zProbeX3(),EEPROM::zProbeY3(),IGNORE_COORDINATE,IGNORE_COORDINATE,EEPROM::zProbeXYSpeed());
			step = STEP_AUTOLEVEL_DO_ZPROB;
			break;	
		case STEP_AUTOLEVEL_DO_ZPROB:
			{
			UI_STATUS(UI_TEXT_ZPROBING);
			sum = 0;
			probeDepth = (int32_t)((float)Z_PROBE_SWITCHING_DISTANCE*Printer::axisStepsPerMM[Z_AXIS]);
			lastCorrection = Printer::currentPositionSteps[Z_AXIS];
			#if NONLINEAR_SYSTEM
				Printer::realDeltaPositionSteps[Z_AXIS] = Printer::currentDeltaPositionSteps[Z_AXIS]; // update real
			#endif	
			probeDepth = 2 * (Printer::zMaxSteps - Printer::zMinSteps);
			Printer::stepsRemainingAtZHit = -1;
			Printer::setZProbingActive(true);
			PrintLine::moveRelativeDistanceInSteps(0,0,-probeDepth,0,EEPROM::zProbeSpeed(),true,true);
			if(Printer::stepsRemainingAtZHit<0)
				{
				Com::printErrorFLN(Com::tZProbeFailed);
				UI_STATUS(UI_TEXT_Z_PROBE_FAILED);
                 Z_probe[xypoint-1]=-2000;
                process_it=false;
				Printer::setZProbingActive(false);
                status=STATUS_FAIL;
                uid.refreshPage();
				PrintLine::moveRelativeDistanceInSteps(0,0,10*Printer::axisStepsPerMM[Z_AXIS],0,Printer::homingFeedrate[0],true,false);
				playsound(3000,240);
				playsound(5000,240);
				playsound(3000,240);
				break;
				}
			Printer::setZProbingActive(false);
			Printer::currentPositionSteps[Z_AXIS] += Printer::stepsRemainingAtZHit; // now current position is correct
			sum += lastCorrection - Printer::currentPositionSteps[Z_AXIS];
			distance = (float)sum * Printer::invAxisStepsPerMM[Z_AXIS] / (float)1.00 + EEPROM::zProbeHeight();
			Com::printF(Com::tZProbe,distance);
			Com::printF(Com::tSpaceXColon,Printer::realXPosition());
			Com::printFLN(Com::tSpaceYColon,Printer::realYPosition());
			uid.setStatusP(Com::tHitZProbe);
             Z_probe[xypoint-1]=distance;
			uid.refreshPage();
			// Go back to start position
			PrintLine::moveRelativeDistanceInSteps(0,0,lastCorrection-Printer::currentPositionSteps[Z_AXIS],0,EEPROM::zProbeSpeed(),true,false);
			if(xypoint==1)
				{
				xypoint++;
				step = STEP_AUTOLEVEL_MOVE;
				h1=distance;
				}
			else if(xypoint==2)
				{
				xypoint++;
				step = STEP_AUTOLEVEL_MOVE;
				h2=distance;
				}
			else{
				h3=distance;
				Printer::buildTransformationMatrix(h1,h2,h3);
				z = -((Printer::autolevelTransformation[0]*Printer::autolevelTransformation[5]-
                         Printer::autolevelTransformation[2]*Printer::autolevelTransformation[3])*
                        (float)Printer::currentPositionSteps[Y_AXIS]*Printer::invAxisStepsPerMM[Y_AXIS]+
                        (Printer::autolevelTransformation[2]*Printer::autolevelTransformation[4]-
                         Printer::autolevelTransformation[1]*Printer::autolevelTransformation[5])*
                        (float)Printer::currentPositionSteps[X_AXIS]*Printer::invAxisStepsPerMM[X_AXIS])/
                      (Printer::autolevelTransformation[1]*Printer::autolevelTransformation[3]-Printer::autolevelTransformation[0]*Printer::autolevelTransformation[4]);
#ifdef ZPROBE_ADJUST_ZMIN
				Com::printFLN("Old zMin:", Printer::zMin);
				Com::printFLN("Z : ", z);
				Com::printFLN("h3 : ", h3);
				Com::printFLN("bed : ", (float)EEPROM::zProbeBedDistance());
				Printer::zMin = z + h3 - EEPROM::zProbeBedDistance();
				Com::printFLN("New zMin:", Printer::zMin);
#else
				Printer::zMin = 0;
#endif
				step = STEP_AUTOLEVEL_RESULTS;
                playsound(3000,240);
				playsound(4000,240);
				playsound(5000,240);
                UI_STATUS(UI_TEXT_WAIT_OK);
				}
			break;
			}
        case STEP_AUTOLEVEL_RESULTS:
        //just wait Ok is pushed
        break;
		 case STEP_AUTOLEVEL_SAVE_RESULTS:
				playsound(3000,240);
				playsound(4000,240);
				playsound(5000,240);
			if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_SAVE,UI_TEXT_ZMIN))
				{
                //save matrix to memory as individual save to eeprom erase all
                float tmpmatrix[9];
                for(uint8_t ti=0; ti<9; ti++)tmpmatrix[ti]=Printer::autolevelTransformation[ti];
				//save to eeprom
                EEPROM:: update(EPR_Z_HOME_OFFSET,EPR_TYPE_FLOAT,0,Printer::zMin);
                //Set autolevel to false for saving
                 EEPROM:: update(EPR_AUTOLEVEL_ACTIVE,EPR_TYPE_BYTE,0,0);
                 Printer::setAutolevelActive(false);
                //save Matrix
                 for(uint8_t ti=0; ti<9; ti++)EEPROM:: update(EPR_AUTOLEVEL_MATRIX + (((int)ti) << 2),EPR_TYPE_FLOAT,0,tmpmatrix[ti]); 
                 //Set autolevel to true
                 EEPROM:: update(EPR_AUTOLEVEL_ACTIVE,EPR_TYPE_BYTE,1,0);
                 Printer::setAutolevelActive(true);
                 for(uint8_t ti=0; ti<9; ti++)Printer::autolevelTransformation[ti]=tmpmatrix[ti];
				}
			else{
				//back to original value
				Printer::zMin=currentzMin;
				}
            if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_REDO_ACTION,UI_TEXT_AUTOLEVEL))
					{
                    if(menuLevel>0) menuLevel--;
					 pushMenu(&ui_menu_autolevel_page,true);
                    xypoint=1;
                    z = 0;
                    currentzMin = Printer::zMin;
                    Printer::setAutolevelActive(true);
                    Printer::updateDerivedParameter();
                    Printer::updateCurrentPosition(true);
                    step=STEP_ZPROBE_SCRIPT;
                    }
            else 	process_it=false;	
			break;
		 }
		 //check what key is pressed
		 if (previousaction!=lastButtonAction)
			{
			previousaction=lastButtonAction;
			if(previousaction!=0)BEEP_SHORT;
             if (lastButtonAction==UI_ACTION_OK  && step==STEP_AUTOLEVEL_RESULTS)
                {
                    step=STEP_AUTOLEVEL_SAVE_RESULTS;
                }
			 if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
				{
				if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_AUTOLEVEL))
					{
					Printer::setZProbingActive(false);
					status=STATUS_CANCEL;
					PrintLine::moveRelativeDistanceInSteps(0,0,10*Printer::axisStepsPerMM[Z_AXIS],0,Printer::homingFeedrate[0],true,false);
					UI_STATUS(UI_TEXT_CANCELED);
					process_it=false;
					}
				else
					{//we continue as before
					if(menuLevel>0) menuLevel--;
					pushMenu(&ui_menu_autolevel_page,true);
					}
				delay(100);
				}
			 //wake up light if power saving has been launched
			#if UI_AUTOLIGHTOFF_AFTER!=0
			if (EEPROM::timepowersaving>0)
				{
				UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
				#if CASE_LIGHTS_PIN > 0
				if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
					{
					TOGGLE(CASE_LIGHTS_PIN);
					}
				#endif
				#if defined(UI_BACKLIGHT_PIN)
				if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
				#endif
				}
			#endif
			}
		}
        //cool down if necessary
		Extruder::setTemperatureForExtruder(extrudertarget1,0);
		#if NUM_EXTRUDER>1
        Extruder::setTemperatureForExtruder(extrudertarget2,1);
		#endif
		#if HAVE_HEATED_BED==true
          Extruder::setHeatedBedTemperature(bedtarget);
		#endif
		Printer::setAutolevelActive(true);
		Printer::updateDerivedParameter();
        Printer::updateCurrentPosition(true);
		//home again
		Printer::homeAxis(true,true,true);
		Printer::feedrate = oldFeedrate;
		if(status==STATUS_OK)
			{
			UI_STATUS(UI_TEXT_PRINTER_READY);
			menuLevel=tmpmenu;
			menuPos[menuLevel]=tmpmenupos;
			menu[menuLevel]=tmpmen;
			refreshPage();
			}
		else if (status==STATUS_FAIL)
			{
			while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions();
			UI_STATUS(UI_TEXT_Z_PROBE_FAILED);
			menuLevel=0;
			}
		else if (status==STATUS_CANCEL)
			{
			while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions();
			UI_STATUS(UI_TEXT_CANCELED);
			menuLevel=0;
			}
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
			if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
			{
			benable_autoreturn=true;//reactivate 
			ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
			}
 #endif
		refreshPage();
		break;
		}
#endif
		
		case UI_ACTION_MANUAL_LEVEL:
		{
	    //be sure no issue
		if(reportTempsensorError() or Printer::debugDryrun()) break;
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
		bool btmp_autoreturn=benable_autoreturn; //save current value
		benable_autoreturn=false;//desactivate no need to test if active or not
#endif
		int tmpmenu=menuLevel;
		int tmpmenupos=menuPos[menuLevel];
		UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
        //save current target temp
        float extrudertarget1=extruder[0].tempControl.targetTemperatureC;
        #if NUM_EXTRUDER>1
        float extrudertarget2=extruder[1].tempControl.targetTemperatureC;
        #endif
        #if HAVE_HEATED_BED==true
        float bedtarget=heatedBedController.targetTemperatureC;
        #endif
		#if ENABLE_CLEAN_NOZZLE==1
		//ask for user if he wants to clean nozzle and plate
			if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_CLEAN1,UI_TEXT_CLEAN2))
					{
                      //heat extruders first to keep them hot
                    if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                   Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
                   #if NUM_EXTRUDER>1
                   if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                    Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
                   #endif
					executeAction(UI_ACTION_CLEAN_NOZZLE);
					}
		 #endif
		 if(menuLevel>0) menuLevel--;
		 pushMenu(&ui_menu_manual_level_heat_page,true);
		//Home first
		Printer::homeAxis(true,true,true);
		//then put bed +10mm
		Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
        Commands::waitUntilEndOfAllMoves();
		UI_STATUS(UI_TEXT_HEATING);
		process_it=true;
		printedTime = HAL::timeInMilliseconds();
		step=STEP_MANUAL_LEVEL_HEATING;
		int status=STATUS_OK;
		
		while (process_it)
		{
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
		Commands::delay_flag_change=0;
		Commands::checkForPeriodicalActions();
		currentTime = HAL::timeInMilliseconds();
        if( (currentTime - printedTime) > 1000 )   //Print Temp Reading every 1 second while heating up.
            {
            Commands::printTemperatures();
            printedTime = currentTime;
           }
		 switch(step)
		 {
		 case STEP_MANUAL_LEVEL_HEATING:
           if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
           Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
		   #if NUM_EXTRUDER>1
           if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
		   #endif
		   #if HAVE_HEATED_BED==true
           if (heatedBedController.targetTemperatureC<EEPROM::ftemp_bed_abs)
            Extruder::setHeatedBedTemperature(EEPROM::ftemp_bed_abs);
		   #endif
		   step =  STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE;
		 break;
		 case STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE:
			UI_STATUS(UI_TEXT_HEATING);
			//no need to be extremely accurate so no need stable temperature 
			if(abs(extruder[0].tempControl.currentTemperatureC- extruder[0].tempControl.targetTemperatureC)<2)
				{
				step = STEP_MANUAL_LEVEL_PAGE0;
				}
			#if NUM_EXTRUDER==2
			if(!((abs(extruder[1].tempControl.currentTemperatureC- extruder[1].tempControl.targetTemperatureC)<2) && (step == STEP_MANUAL_LEVEL_PAGE0)))
				{
				step = STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE;
				}
			#endif
			#if HAVE_HEATED_BED==true
			if(!((abs(heatedBedController.currentTemperatureC-heatedBedController.targetTemperatureC)<2) && (step == STEP_MANUAL_LEVEL_PAGE0)))
				{
				step = STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE;
				}
			#endif
		 break;
		case  STEP_MANUAL_LEVEL_PAGE0:
			if(menuLevel>0) menuLevel--;
			pushMenu(&ui_menu_manual_level_page1,true);
		  	playsound(3000,240);
			playsound(4000,240);
			playsound(5000,240);
		  step =STEP_MANUAL_LEVEL_PAGE1;
		  break;
		 case  STEP_MANUAL_LEVEL_POINT_1:
			//go to point 1
			if(menuLevel>0) menuLevel--;
			pushMenu(&ui_menu_manual_level_heat_page,true);
			UI_STATUS(UI_TEXT_MOVING);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Printer::moveToReal(EEPROM::ManualProbeX1() ,EEPROM::ManualProbeY1(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Commands::waitUntilEndOfAllMoves();
			 if(menuLevel>0) menuLevel--;
		  	playsound(3000,240);
			playsound(4000,240);
			playsound(5000,240);
			pushMenu(&ui_menu_manual_level_page6,true);
			step =STEP_MANUAL_LEVEL_PAGE6;
			break;
		 case  STEP_MANUAL_LEVEL_POINT_2:
			//go to point 2
			if(menuLevel>0) menuLevel--;
			pushMenu(&ui_menu_manual_level_heat_page,true);
			UI_STATUS(UI_TEXT_MOVING);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Printer::moveToReal(EEPROM::ManualProbeX2() ,EEPROM::ManualProbeY2(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Commands::waitUntilEndOfAllMoves();
			 if(menuLevel>0) menuLevel--;
		  	playsound(3000,240);
			playsound(4000,240);
			playsound(5000,240);
			pushMenu(&ui_menu_manual_level_page7,true);
			step =STEP_MANUAL_LEVEL_PAGE7;
			break;
		 case  STEP_MANUAL_LEVEL_POINT_3:
			//go to point 3
			 if(menuLevel>0) menuLevel--;
			pushMenu(&ui_menu_manual_level_heat_page,true);
			UI_STATUS(UI_TEXT_MOVING);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Printer::moveToReal(EEPROM::ManualProbeX3() ,EEPROM::ManualProbeY3(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Commands::waitUntilEndOfAllMoves();
			 if(menuLevel>0) menuLevel--;
		  	playsound(3000,240);
			playsound(4000,240);
			playsound(5000,240);
			pushMenu(&ui_menu_manual_level_page8,true);
			step =STEP_MANUAL_LEVEL_PAGE8;
			break;
		 case  STEP_MANUAL_LEVEL_POINT_4:
			//go to point 4
			 if(menuLevel>0) menuLevel--;
			pushMenu(&ui_menu_manual_level_heat_page,true);
			UI_STATUS(UI_TEXT_MOVING);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Printer::moveToReal(EEPROM::ManualProbeX4() ,EEPROM::ManualProbeY4(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Commands::waitUntilEndOfAllMoves();
			 if(menuLevel>0) menuLevel--;
		  	playsound(3000,240);
			playsound(4000,240);
			playsound(5000,240);
			pushMenu(&ui_menu_manual_level_page9,true);
			step =STEP_MANUAL_LEVEL_PAGE9;
			break;
		  case  STEP_MANUAL_LEVEL_POINT_5:
			//go to point 5
			 if(menuLevel>0) menuLevel--;
			pushMenu(&ui_menu_manual_level_heat_page,true);
			UI_STATUS(UI_TEXT_MOVING);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Printer::moveToReal(EEPROM::ManualProbeX2() ,EEPROM::ManualProbeY3(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Commands::waitUntilEndOfAllMoves();
			 if(menuLevel>0) menuLevel--;
		  	playsound(3000,240);
			playsound(4000,240);
			playsound(5000,240);
			pushMenu(&ui_menu_manual_level_page10,true);
			step =STEP_MANUAL_LEVEL_PAGE10;
			break;
		}
		 //check what key is pressed
		 if (previousaction!=lastButtonAction)
			{
			previousaction=lastButtonAction;
			if(previousaction!=0)BEEP_SHORT;
			if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE2))
			{
				step=STEP_MANUAL_LEVEL_PAGE3;
			 if(menuLevel>0) menuLevel--;
				pushMenu(&ui_menu_manual_level_page3,true);
			}
			else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE1))
			{
				step=STEP_MANUAL_LEVEL_PAGE2;
				if(menuLevel>0) menuLevel--;
				pushMenu(&ui_menu_manual_level_page2,true);
			}
			
			else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE3))
			{
				step=STEP_MANUAL_LEVEL_PAGE4;
				 if(menuLevel>0) menuLevel--;
				pushMenu(&ui_menu_manual_level_page4,true);
			}
			else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE4))
			{
				step=STEP_MANUAL_LEVEL_PAGE5;
			if(menuLevel>0) menuLevel--;
				pushMenu(&ui_menu_manual_level_page5,true);
			}
			else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE5))
			{
				step=STEP_MANUAL_LEVEL_POINT_1;
			}
			else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE6))
			{
				step=STEP_MANUAL_LEVEL_POINT_2;
			}
			else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE7))
			{
				step=STEP_MANUAL_LEVEL_POINT_3;
			}
			else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE8))
			{
				step=STEP_MANUAL_LEVEL_POINT_4;
			}
			if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE9))
			{
				step=STEP_MANUAL_LEVEL_POINT_5;
			}
			else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE10))
			{
                playsound(5000,240);
				playsound(3000,240);
                if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_REDO_ACTION,UI_TEXT_MANUAL_LEVEL))
					{
                    if(menuLevel>0) menuLevel--;
					pushMenu(&ui_menu_manual_level_heat_page,true);
                    step=STEP_MANUAL_LEVEL_POINT_1;
                    }
                else
                    {
                     if(menuLevel>0) menuLevel--;
                    pushMenu(&ui_menu_manual_level_heat_page,true);
                    Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
                    process_it=false;
                    }
			}
			else if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
				{
				if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_MANUAL_LEVEL))
					{
					status=STATUS_CANCEL;
					if(menuLevel>0) menuLevel--;
					pushMenu(&ui_menu_manual_level_heat_page,true);
					PrintLine::moveRelativeDistanceInSteps(0,0,10*Printer::axisStepsPerMM[Z_AXIS],0,Printer::homingFeedrate[0],true,false);
					UI_STATUS(UI_TEXT_CANCELED);
					process_it=false;
					}
				else
					{//we continue as before
					if(menuLevel>0) menuLevel--;
					if(step==STEP_MANUAL_LEVEL_PAGE1)pushMenu(&ui_menu_manual_level_page1,true);
					if(step==STEP_MANUAL_LEVEL_PAGE2)pushMenu(&ui_menu_manual_level_page2,true);
					if(step==STEP_MANUAL_LEVEL_PAGE3)pushMenu(&ui_menu_manual_level_page3,true);
					if(step==STEP_MANUAL_LEVEL_PAGE4)pushMenu(&ui_menu_manual_level_page4,true);
					if(step==STEP_MANUAL_LEVEL_PAGE5)pushMenu(&ui_menu_manual_level_page5,true);
					if(step==STEP_MANUAL_LEVEL_PAGE6)pushMenu(&ui_menu_manual_level_page6,true);
					if(step==STEP_MANUAL_LEVEL_PAGE7)pushMenu(&ui_menu_manual_level_page7,true);
					if(step==STEP_MANUAL_LEVEL_PAGE8)pushMenu(&ui_menu_manual_level_page8,true);
					if(step==STEP_MANUAL_LEVEL_PAGE9)pushMenu(&ui_menu_manual_level_page9,true);
					if(step==STEP_MANUAL_LEVEL_PAGE10)pushMenu(&ui_menu_manual_level_page10,true);
					if(step==STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE)pushMenu(&ui_menu_manual_level_heat_page,true);
					}
					}
			delay(100);
			}
			 
			 //wake up light if power saving has been launched
			#if UI_AUTOLIGHTOFF_AFTER!=0
			if (EEPROM::timepowersaving>0)
				{
				UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
				#if CASE_LIGHTS_PIN > 0
				if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
					{
					TOGGLE(CASE_LIGHTS_PIN);
					}
				#endif
				#if defined(UI_BACKLIGHT_PIN)
				if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
				#endif
				}
			#endif
			}
		//cool down if necessary
		Extruder::setTemperatureForExtruder(extrudertarget1,0);
		#if NUM_EXTRUDER>1
        Extruder::setTemperatureForExtruder(extrudertarget2,1);
		#endif
		#if HAVE_HEATED_BED==true
          Extruder::setHeatedBedTemperature(bedtarget);
		#endif
		//home again
		Printer::homeAxis(true,true,true);
		if(status==STATUS_OK)
			{
			 UI_STATUS(UI_TEXT_PRINTER_READY);
			menuLevel=tmpmenu;
			menuPos[menuLevel]=tmpmenupos;
			menu[menuLevel]=tmpmen;
			}
		else if (status==STATUS_CANCEL)
			{
			while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions();
			UI_STATUS(UI_TEXT_CANCELED);
			menuLevel=0;
			}
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
			if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
			{
			benable_autoreturn=true;//reactivate 
			ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
			}
 #endif
		refreshPage();
        break;
		}

        case UI_ACTION_PREHEAT_PLA:
            {
            UI_STATUS(UI_TEXT_PREHEAT_PLA);
            bool allheat=true;
            if(extruder[0].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_pla,0);
#if NUM_EXTRUDER>1
            if(extruder[1].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_pla,1);
#endif
#if NUM_EXTRUDER>2
            if(extruder[2].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_pla,2);
#endif
#if HAVE_HEATED_BED==true
            if(heatedBedController.targetTemperatureC!=EEPROM::ftemp_bed_pla)allheat=false;
            Extruder::setHeatedBedTemperature(EEPROM::ftemp_bed_pla);
#endif
            if (allheat)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
			    playsound(5000,240);
                }
            break;
        }
        case UI_ACTION_PREHEAT_ABS:
        {
            UI_STATUS(UI_TEXT_PREHEAT_ABS);
            bool allheat=true;
            if(extruder[0].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
#if NUM_EXTRUDER>1
            if(extruder[1].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
#endif
#if NUM_EXTRUDER>2
            if(extruder[2].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,2);
#endif
#if HAVE_HEATED_BED==true
            if(heatedBedController.targetTemperatureC!=EEPROM::ftemp_bed_abs)allheat=false;
            Extruder::setHeatedBedTemperature(EEPROM::ftemp_bed_abs);
#endif
            if (allheat)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
			    playsound(5000,240);
                }
            break;
        }
        case UI_ACTION_COOLDOWN:
        {
            UI_STATUS(UI_TEXT_COOLDOWN);
            bool alloff=true;
            if(extruder[0].tempControl.targetTemperatureC>0)alloff=false;
             Extruder::setTemperatureForExtruder(0,0);
#if NUM_EXTRUDER>1
            if(extruder[1].tempControl.targetTemperatureC>0)alloff=false;
            Extruder::setTemperatureForExtruder(0,1);
#endif
#if NUM_EXTRUDER>2
             if(extruder[2].tempControl.targetTemperatureC>0)alloff=false;
             Extruder::setTemperatureForExtruder(0,2);
#endif
#if HAVE_HEATED_BED==true
             if (heatedBedController.targetTemperatureC>0)alloff=false;
             Extruder::setHeatedBedTemperature(0);
#endif
             if (alloff)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
			    playsound(5000,240);
                }
//do not allows to heatjust after cooling to avoid cached commands
            Extruder::disableheat_time =HAL::timeInMilliseconds() +3000;
            break;
        }
        case UI_ACTION_HEATED_BED_OFF:
#if HAVE_HEATED_BED==true
            if (heatedBedController.targetTemperatureC==0)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
			    playsound(5000,240);
                }
            Extruder::setHeatedBedTemperature(0);
#endif
            break;
        case UI_ACTION_EXTRUDER0_OFF:
            if (extruder[0].tempControl.targetTemperatureC==0)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
			    playsound(5000,240);
                }
            Extruder::setTemperatureForExtruder(0,0);
            break;
        case UI_ACTION_EXTRUDER1_OFF:
#if NUM_EXTRUDER>1
             if (extruder[1].tempControl.targetTemperatureC==0)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
			    playsound(5000,240);
                }
            Extruder::setTemperatureForExtruder(0,1);
#endif
            break;
        case UI_ACTION_EXTRUDER2_OFF:
#if NUM_EXTRUDER>2
            if (extruder[2].tempControl.targetTemperatureC==0)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
			    playsound(5000,240);
                }
            Extruder::setTemperatureForExtruder(0,2);
#endif
            break;
        case UI_ACTION_DISABLE_STEPPER:
            Printer::kill(true);
            break;
        case UI_ACTION_RESET_EXTRUDER:
            Printer::currentPositionSteps[3] = 0;
            break;
        case UI_ACTION_EXTRUDER_RELATIVE:
            Printer::relativeExtruderCoordinateMode=!Printer::relativeExtruderCoordinateMode;
            break;
        case UI_ACTION_SELECT_EXTRUDER0:
            Extruder::selectExtruderById(0);
            break;
        case UI_ACTION_SELECT_EXTRUDER1:
#if NUM_EXTRUDER>1
            Extruder::selectExtruderById(1);
#endif
            break;
        case UI_ACTION_SELECT_EXTRUDER2:
#if NUM_EXTRUDER>2
            Extruder::selectExtruderById(2);
#endif
            break;
#if EEPROM_MODE!=0
        case UI_ACTION_STORE_EEPROM:
            EEPROM::storeDataIntoEEPROM(false);
            pushMenu(&ui_menu_eeprom_saved,false);
            BEEP_LONG;
            skipBeep = true;
            break;
        case UI_ACTION_LOAD_EEPROM:
            EEPROM::readDataFromEEPROM();
            Extruder::selectExtruderById(Extruder::current->id);
            pushMenu(&ui_menu_eeprom_loaded,false);
            BEEP_LONG;
            skipBeep = true;
            break;
         case UI_ACTION_LOAD_FAILSAFE:
            EEPROM::restoreEEPROMSettingsFromConfiguration();
            Extruder::selectExtruderById(Extruder::current->id);
            BEEP_LONG;
            		//ask for user if he wants to save to eeprom after loading
			if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_STORE_TO_EEPROM,UI_TEXT_LOAD_FAILSAFE2))
					{
					executeAction(UI_ACTION_STORE_EEPROM);
					}
			else UI_STATUS(UI_TEXT_LOAD_FAILSAFE);
            skipBeep = true;
            break;
#endif
#if SDSUPPORT
        case UI_ACTION_SD_DELETE:
            if(sd.sdactive)
            {
                pushMenu(&ui_menu_sd_fileselector,false);
            }
            else
            {
                UI_ERROR(UI_TEXT_NOSDCARD);
            }
            break;
        case UI_ACTION_SD_PRINT:
            if(sd.sdactive)
            {
                pushMenu(&ui_menu_sd_fileselector,false);
            }
            break;
        case UI_ACTION_SD_PAUSE:
            sd.pausePrint(true);
            break;
        case UI_ACTION_SD_CONTINUE:
            sd.continuePrint();
            break;
        case UI_ACTION_SD_STOP:
            {
             playsound(400,400);
             //reset connect with host if any
            Com::printFLN(Com::tReset);
            //we are printing from sdcard or from host ?
			if(!Printer::isMenuMode(MENU_MODE_SD_PAUSED) || sd.sdmode )sd.stopPrint();
			else Com::printFLN(PSTR("Host Print stopped by user."));
			menuLevel=0;
			menuPos[0]=0;
			refreshPage();
             Printer::setMenuMode(MENU_MODE_STOP_REQUESTED,true);
             Printer::setMenuMode(MENU_MODE_STOP_DONE,false);
             Printer::setMenuMode(MENU_MODE_SD_PAUSED,false);
           	 //to be sure no moving  by dummy movement
              Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
             GCode::bufferReadIndex=0; ///< Read position in gcode_buffer.
             GCode::bufferWriteIndex=0; ///< Write position in gcode_buffer.
             GCode::commandsReceivingWritePosition=0; ///< Writing position in gcode_transbuffer
             GCode::lastLineNumber=0; ///< Last line number received.
             GCode::bufferLength=0; ///< Number of commands stored in gcode_buffer
             PrintLine::nlFlag = false;
             PrintLine::linesCount=0;
             PrintLine::linesWritePos = 0;            ///< Position where we write the next cached line move.
             PrintLine::linesPos = 0;                 ///< Position for executing line movement
             PrintLine::lines[0].block();
           	executeAction(UI_ACTION_COOLDOWN);
            Printer::setMenuMode(MENU_MODE_STOP_DONE,true);
            UI_STATUS(UI_TEXT_CANCELED);
            break;
        }
        case UI_ACTION_SD_UNMOUNT:
            sd.unmount();
            break;
        case UI_ACTION_SD_MOUNT:
            sd.mount();
            break;
        case UI_ACTION_MENU_SDCARD:
            pushMenu(&ui_menu_sd,false);
            break;
#endif
#if FAN_PIN>-1
        case UI_ACTION_FAN_OFF:
            Commands::setFanSpeed(0,false);
            break;
        case UI_ACTION_FAN_25:
            Commands::setFanSpeed(64,false);
            break;
        case UI_ACTION_FAN_50:
            Commands::setFanSpeed(128,false);
            break;
        case UI_ACTION_FAN_75:
            Commands::setFanSpeed(192,false);
            break;
        case UI_ACTION_FAN_FULL:
            Commands::setFanSpeed(255,false);
            break;
#endif
        case UI_ACTION_MENU_XPOS:
            pushMenu(&ui_menu_xpos,false);
            break;
        case UI_ACTION_MENU_YPOS:
            pushMenu(&ui_menu_ypos,false);
            break;
        case UI_ACTION_MENU_ZPOS:
            pushMenu(&ui_menu_zpos,false);
            break;
        case UI_ACTION_MENU_XPOSFAST:
            pushMenu(&ui_menu_xpos_fast,false);
            break;
        case UI_ACTION_MENU_YPOSFAST:
            pushMenu(&ui_menu_ypos_fast,false);
            break;
        case UI_ACTION_MENU_ZPOSFAST:
            pushMenu(&ui_menu_zpos_fast,false);
            break;
        case UI_ACTION_MENU_QUICKSETTINGS:
            //pushMenu(&ui_menu_quick,false);
            pushMenu(&ui_menu_settings,false);
            break;
        case UI_ACTION_MENU_EXTRUDER:
            pushMenu(&ui_menu_extruder,false);
            break;
        case UI_ACTION_MENU_POSITIONS:
            pushMenu(&ui_menu_positions,false);
            break;
#ifdef UI_USERMENU1
        case UI_ACTION_SHOW_USERMENU1:
            pushMenu(&UI_USERMENU1,false);
            break;
#endif
#ifdef UI_USERMENU2
        case UI_ACTION_SHOW_USERMENU2:
            pushMenu(&UI_USERMENU2,false);
            break;
#endif
#ifdef UI_USERMENU3
        case UI_ACTION_SHOW_USERMENU3:
            pushMenu(&UI_USERMENU3,false);
            break;
#endif
#ifdef UI_USERMENU4
        case UI_ACTION_SHOW_USERMENU4:
            pushMenu(&UI_USERMENU4,false);
            break;
#endif
#ifdef UI_USERMENU5
        case UI_ACTION_SHOW_USERMENU5:
            pushMenu(&UI_USERMENU5,false);
            break;
#endif
#ifdef UI_USERMENU6
        case UI_ACTION_SHOW_USERMENU6:
            pushMenu(&UI_USERMENU6,false);
            break;
#endif
#ifdef UI_USERMENU7
        case UI_ACTION_SHOW_USERMENU7:
            pushMenu(&UI_USERMENU7,false);
            break;
#endif
#ifdef UI_USERMENU8
        case UI_ACTION_SHOW_USERMENU8:
            pushMenu(&UI_USERMENU8,false);
            break;
#endif
#ifdef UI_USERMENU9
        case UI_ACTION_SHOW_USERMENU9:
            pushMenu(&UI_USERMENU9,false);
            break;
#endif
#ifdef UI_USERMENU10
        case UI_ACTION_SHOW_USERMENU10:
            pushMenu(&UI_USERMENU10,false);
            break;
#endif
        case UI_ACTION_X_UP:
            PrintLine::moveRelativeDistanceInStepsReal(Printer::axisStepsPerMM[X_AXIS],0,0,0,Printer::homingFeedrate[X_AXIS],false);
            break;
        case UI_ACTION_X_DOWN:
            PrintLine::moveRelativeDistanceInStepsReal(-Printer::axisStepsPerMM[X_AXIS],0,0,0,Printer::homingFeedrate[X_AXIS],false);
            break;
        case UI_ACTION_Y_UP:
            PrintLine::moveRelativeDistanceInStepsReal(0,Printer::axisStepsPerMM[1],0,0,Printer::homingFeedrate[1],false);
            break;
        case UI_ACTION_Y_DOWN:
            PrintLine::moveRelativeDistanceInStepsReal(0,-Printer::axisStepsPerMM[1],0,0,Printer::homingFeedrate[1],false);
            break;
        case UI_ACTION_Z_UP:
            PrintLine::moveRelativeDistanceInStepsReal(0,0,Printer::axisStepsPerMM[2],0,Printer::homingFeedrate[2],false);
            break;
        case UI_ACTION_Z_DOWN:
            PrintLine::moveRelativeDistanceInStepsReal(0,0,-Printer::axisStepsPerMM[2],0,Printer::homingFeedrate[2],false);
            break;
        case UI_ACTION_EXTRUDER_UP:
            PrintLine::moveRelativeDistanceInStepsReal(0,0,0,Printer::axisStepsPerMM[3],UI_SET_EXTRUDER_FEEDRATE,false);
            break;
        case UI_ACTION_EXTRUDER_DOWN:
            PrintLine::moveRelativeDistanceInStepsReal(0,0,0,-Printer::axisStepsPerMM[3],UI_SET_EXTRUDER_FEEDRATE,false);
            break;
        case UI_ACTION_EXTRUDER_TEMP_UP:
        {
            int tmp = (int)(Extruder::current->tempControl.targetTemperatureC)+1;
            if(tmp==1) tmp = UI_SET_MIN_EXTRUDER_TEMP;
            else if(tmp>UI_SET_MAX_EXTRUDER_TEMP) tmp = UI_SET_MAX_EXTRUDER_TEMP;
            Extruder::setTemperatureForExtruder(tmp,Extruder::current->id);
        }
        break;
        case UI_ACTION_EXTRUDER_TEMP_DOWN:
        {
            int tmp = (int)(Extruder::current->tempControl.targetTemperatureC)-1;
            if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
            Extruder::setTemperatureForExtruder(tmp,Extruder::current->id);
        }
        break;
        case UI_ACTION_HEATED_BED_UP:
#if HAVE_HEATED_BED==true
        {
            int tmp = (int)heatedBedController.targetTemperatureC+1;
            if(tmp==1) tmp = UI_SET_MIN_HEATED_BED_TEMP;
            else if(tmp>UI_SET_MAX_HEATED_BED_TEMP) tmp = UI_SET_MAX_HEATED_BED_TEMP;
            Extruder::setHeatedBedTemperature(tmp);
        }
#endif
        break;
#if MAX_HARDWARE_ENDSTOP_Z
        case UI_ACTION_SET_MEASURED_ORIGIN:
        {
            Printer::updateCurrentPosition();
            Printer::zLength -= Printer::currentPosition[Z_AXIS];
            Printer::currentPositionSteps[Z_AXIS] = 0;
            Printer::updateDerivedParameter();
#if NONLINEAR_SYSTEM
            transformCartesianStepsToDeltaSteps(Printer::currentPositionSteps, Printer::currentDeltaPositionSteps);
#endif
            Printer::updateCurrentPosition(true);
            Com::printFLN(Com::tZProbePrinterHeight,Printer::zLength);
#if EEPROM_MODE!=0
            EEPROM::storeDataIntoEEPROM(false);
            Com::printFLN(Com::tEEPROMUpdated);
#endif
            Commands::printCurrentPosition();
        }
        break;
#endif
        case UI_ACTION_SET_P1:
#ifdef SOFTWARE_LEVELING
            for (uint8_t i=0; i<3; i++)
            {
                Printer::levelingP1[i] = Printer::currentPositionSteps[i];
            }
#endif
            break;
        case UI_ACTION_SET_P2:
#ifdef SOFTWARE_LEVELING
            for (uint8_t i=0; i<3; i++)
            {
                Printer::levelingP2[i] = Printer::currentPositionSteps[i];
            }
#endif
            break;
        case UI_ACTION_SET_P3:
#ifdef SOFTWARE_LEVELING
            for (uint8_t i=0; i<3; i++)
            {
                Printer::levelingP3[i] = Printer::currentPositionSteps[i];
            }
#endif
            break;
        case UI_ACTION_CALC_LEVEL:
#ifdef SOFTWARE_LEVELING
            int32_t factors[4];
            PrintLine::calculatePlane(factors, Printer::levelingP1, Printer::levelingP2, Printer::levelingP3);
            Com::printFLN(Com::tLevelingCalc);
            Com::printFLN(Com::tTower1, PrintLine::calcZOffset(factors, Printer::deltaAPosXSteps, Printer::deltaAPosYSteps) * Printer::invAxisStepsPerMM[2]);
            Com::printFLN(Com::tTower2, PrintLine::calcZOffset(factors, Printer::deltaBPosXSteps, Printer::deltaBPosYSteps) * Printer::invAxisStepsPerMM[2]);
            Com::printFLN(Com::tTower3, PrintLine::calcZOffset(factors, Printer::deltaCPosXSteps, Printer::deltaCPosYSteps) * Printer::invAxisStepsPerMM[2]);
#endif
            break;
        case UI_ACTION_HEATED_BED_DOWN:
#if HAVE_HEATED_BED==true
        {
            int tmp = (int)heatedBedController.targetTemperatureC-1;
            if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
            Extruder::setHeatedBedTemperature(tmp);
        }
#endif
        break;
        case UI_ACTION_FAN_UP:
            Commands::setFanSpeed(Printer::getFanSpeed()+32,false);
            break;
        case UI_ACTION_FAN_DOWN:
            Commands::setFanSpeed(Printer::getFanSpeed()-32,false);
            break;
        case UI_ACTION_KILL:
            Commands::emergencyStop();
            break;
        case UI_ACTION_RESET:
            HAL::resetHardware();
            break;
        case UI_ACTION_PAUSE:
            Com::printFLN(PSTR("RequestPause:"));
            break;
#ifdef DEBUG_PRINT
        case UI_ACTION_WRITE_DEBUG:
            Com::printF(PSTR("Buf. Read Idx:"),(int)GCode::bufferReadIndex);
            Com::printF(PSTR(" Buf. Write Idx:"),(int)GCode::bufferWriteIndex);
            Com::printF(PSTR(" Comment:"),(int)GCode::commentDetected);
            Com::printF(PSTR(" Buf. Len:"),(int)GCode::bufferLength);
            Com::printF(PSTR(" Wait resend:"),(int)GCode::waitingForResend);
            Com::printFLN(PSTR(" Recv. Write Pos:"),(int)GCode::commandsReceivingWritePosition);
            Com::printF(PSTR("Min. XY Speed:"),Printer::minimumSpeed);
            Com::printF(PSTR(" Min. Z Speed:"),Printer::minimumZSpeed);
            Com::printF(PSTR(" Buffer:"),PrintLine::linesCount);
            Com::printF(PSTR(" Lines pos:"),(int)PrintLine::linesPos);
            Com::printFLN(PSTR(" Write Pos:"),(int)PrintLine::linesWritePos);
            Com::printFLN(PSTR("Wait loop:"),debugWaitLoop);
            Com::printF(PSTR("sd mode:"),(int)sd.sdmode);
            Com::printF(PSTR(" pos:"),sd.sdpos);
            Com::printFLN(PSTR(" of "),sd.filesize);
            break;
#endif
        }
    refreshPage();
    if(!skipBeep)
        BEEP_SHORT
#if UI_AUTORETURN_TO_MENU_AFTER!=0
        ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;
#endif
#if UI_AUTOLIGHTOFF_AFTER!=0
        UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
#endif
#endif
}
void UIDisplay::mediumAction()
{
#if UI_HAS_I2C_ENCODER>0
    ui_check_slow_encoder();
#endif
}
void UIDisplay::slowAction()
{
    unsigned long time = HAL::timeInMilliseconds();
    uint8_t refresh=0;
#if UI_HAS_KEYS==1
    // Update key buffer
    HAL::forbidInterrupts();
    if((flags & 9)==0)
    {
        flags|=8;
        HAL::allowInterrupts();
#if defined(UI_I2C_HOTEND_LED) || defined(UI_I2C_HEATBED_LED) || defined(UI_I2C_FAN_LED)
        {
            // check temps and set appropriate leds
            int led= 0;
#if NUM_EXTRUDER>0 && defined(UI_I2C_HOTEND_LED)
            led |= (tempController[Extruder::current->id]->targetTemperatureC > 0 ? UI_I2C_HOTEND_LED : 0);
#endif
#if HAVE_HEATED_BED && defined(UI_I2C_HEATBED_LED)
            led |= (heatedBedController.targetTemperatureC > 0 ? UI_I2C_HEATBED_LED : 0);
#endif
#if FAN_PIN>=0 && defined(UI_I2C_FAN_LED)
            led |= (Printer::getFanSpeed() > 0 ? UI_I2C_FAN_LED : 0);
#endif
            // update the leds
            uid.outputMask= ~led&(UI_I2C_HEATBED_LED|UI_I2C_HOTEND_LED|UI_I2C_FAN_LED);
        }
#endif
        int nextAction = 0;
        ui_check_slow_keys(nextAction);
        if(lastButtonAction!=nextAction)
        {
            lastButtonStart = time;
            lastButtonAction = nextAction;
            HAL::forbidInterrupts();
            flags|=2; // Mark slow action
        }
        HAL::forbidInterrupts();
        flags-=8;
    }
    HAL::forbidInterrupts();
    if((flags & 4)==0)
    {
        flags |= 4;
        // Reset click encoder
        HAL::forbidInterrupts();
        int8_t epos = encoderPos;
        encoderPos=0;
        HAL::allowInterrupts();
        if(epos)
        {
            nextPreviousAction(epos);
            BEEP_SHORT
            refresh=1;
        }
        if(lastAction!=lastButtonAction)
        {
            if(lastButtonAction==0)
            {
                if(lastAction>=2000 && lastAction<3000)
                {
                    statusMsg[0] = 0;
                }
                lastAction = 0;
                HAL::forbidInterrupts();
                flags &= ~3;
            }
            else if(time-lastButtonStart>UI_KEY_BOUNCETIME)     // New key pressed
            {
                lastAction = lastButtonAction;
                executeAction(lastAction);
                nextRepeat = time+UI_KEY_FIRST_REPEAT;
                repeatDuration = UI_KEY_FIRST_REPEAT;
            }
        }
        else if(lastAction<1000 && lastAction)     // Repeatable key
        {
            if(time-nextRepeat<10000)
            {
                executeAction(lastAction);
                repeatDuration -= UI_KEY_REDUCE_REPEAT;
                if(repeatDuration<UI_KEY_MIN_REPEAT) repeatDuration = UI_KEY_MIN_REPEAT;
                nextRepeat = time+repeatDuration;
            }
        }
        HAL::forbidInterrupts();
        flags -=4;
    }
    HAL::allowInterrupts();
#endif
#if UI_AUTORETURN_TO_MENU_AFTER!=0
    if(menuLevel>0 && ui_autoreturn_time<time && benable_autoreturn)
    {
        lastSwitch = time;
        menuLevel=0;
        activeAction = 0;
    }
#endif

#if UI_AUTOLIGHTOFF_AFTER!=0
if (ui_autolightoff_time==-1) ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
if ((ui_autolightoff_time<time) && (EEPROM::timepowersaving>0) )
    {//if printing and keep light on do not swich off
	if(!(EEPROM::bkeeplighton  &&((Printer::menuMode&MENU_MODE_SD_PRINTING)||(Printer::menuMode&MENU_MODE_PRINTING)||(Printer::menuMode&MENU_MODE_SD_PAUSED))))
	{
		#if CASE_LIGHTS_PIN > 0
		if ((READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
			{
			TOGGLE(CASE_LIGHTS_PIN);
			}
		#endif
		#if defined(UI_BACKLIGHT_PIN)
		WRITE(UI_BACKLIGHT_PIN, LOW);
		#endif
		}
	if((EEPROM::bkeeplighton  &&((Printer::menuMode&MENU_MODE_SD_PRINTING)||(Printer::menuMode&MENU_MODE_PRINTING)||(Printer::menuMode&MENU_MODE_SD_PAUSED))))ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving+10000;
	}
#endif
    if(menuLevel==0 && time>4000)
    {
        if(time-lastSwitch>UI_PAGES_DURATION)
        {
            lastSwitch = time;
#if !defined(UI_DISABLE_AUTO_PAGESWITCH) || !UI_DISABLE_AUTO_PAGESWITCH
            menuPos[0]++;
            if(menuPos[0]>=UI_NUM_PAGES)
                menuPos[0]=0;
#endif
            refresh = 1;
        }
        else if(time-lastRefresh>=1000) refresh=1;
    }
    else if(time-lastRefresh>=800)
    {
        UIMenu *men = (UIMenu*)menu[menuLevel];
        uint8_t mtype = pgm_read_byte((void*)&(men->menuType));
        //if(mtype!=1)
        refresh=1;
    }
    if(refresh)
    {
        if (menuLevel > 1 || Printer::isAutomount())
        {
            shift++;
            if(shift+UI_COLS>MAX_COLS+1)
                shift = -2;
        }
        else
            shift = -2;

        refreshPage();
        lastRefresh = time;
    }
}
void UIDisplay::fastAction()
{
#if UI_HAS_KEYS==1
    // Check keys
    HAL::forbidInterrupts();
    if((flags & 10)==0)
    {
        flags |= 8;
        HAL::allowInterrupts();
        int nextAction = 0;
        ui_check_keys(nextAction);
        if(lastButtonAction!=nextAction)
        {
            lastButtonStart = HAL::timeInMilliseconds();
            lastButtonAction = nextAction;
            HAL::forbidInterrupts();
            flags|=1;
        }
        HAL::forbidInterrupts();
        flags-=8;
    }
    HAL::allowInterrupts();
#endif
}
#if UI_ENCODER_SPEED==0
const int8_t encoder_table[16] PROGMEM = {0,1,-1,0,-1,0,0,1,1,0,0,-1,0,-1,1,0}; // Full speed
#elif UI_ENCODER_SPEED==1
const int8_t encoder_table[16] PROGMEM = {0,0,-1,0,0,0,0,1,1,0,0,0,0,-1,0,0}; // Half speed
#else
//const int8_t encoder_table[16] PROGMEM = {0,0,0,0,0,0,0,0,1,0,0,0,0,-1,0,0}; // Quart speed
//const int8_t encoder_table[16] PROGMEM = {0,1,0,0,-1,0,0,0,0,0,0,0,0,0,0,0}; // Quart speed
const int8_t encoder_table[16] PROGMEM = {0,0,0,0,0,0,0,0,0,0,0,-1,0,0,1,0}; // Quart speed
#endif

#endif

