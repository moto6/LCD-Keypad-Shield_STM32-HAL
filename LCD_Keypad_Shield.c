//#include <stdio.h>
#include <LCD_Keypad_Shield.h>


// When the display powers up, it is configured as follows:

// 1. Display clear
// 2. Function set: 
//    DL = 1; 8-bit interface data 
//    N = 0; 1-line display 
//    F = 0; 5x8 dot character font 
// 3. Display on/off control: 
//    D = 0; Display off 
//    C = 0; Cursor off 
//    B = 0; Blinking off 
// 4. Entry mode set: 
//    I/D = 1; Increment by 1 
//    S = 0; No shift 
//
// Note, however, that resetting the Arduino doesn't reset the LCD, so we
// can't assume that its in that state when a sketch starts (and the
// LiquidCrystal constructor is called).

// LCD Handler!
// Must be needed 2 define : Lcd_PortType data_ports[], Lcd_PinType  data_pins[]
// if you want change other LCDmodule or different hardware, change here (asigned LCD Handler's Pins&Ports)
#ifdef _LCD_8BITMODE_
   Lcd_PortType data_ports[] = { D0_GPIO_Port, D1_GPIO_Port, D2_GPIO_Port, D3_GPIO_Port,
		D4_GPIO_Port, D5_GPIO_Port, D6_GPIO_Port, D7_GPIO_Port};
   Lcd_PinType  data_pins[] = { D0_Pin, D1_Pin, D2_Pin, D3_Pin,
		D4_Pin, D5_Pin, D6_Pin, D7_Pin};
#endif
#ifdef _LCD_4BITMODE_
   Lcd_PortType data_ports[] = {
		D4_GPIO_Port, D5_GPIO_Port, D6_GPIO_Port, D7_GPIO_Port};
   Lcd_PinType  data_pins[] = {
		D4_Pin, D5_Pin, D6_Pin, D7_Pin};
#endif

//**************************************************************************************
//**************************************************************************************
//Time

uint32_t micros(){
  return (millis()&0x3FFFFF)*1000 + (SYSTICK_LOAD-SysTick->VAL)/SYS_CLOCK;
}

void delay_us(uint32_t us){
  uint32_t temp = micros();
  uint32_t comp = temp + us;
  uint8_t  flag = 0;
  while(comp > temp){
    uint32_t mil = millis();
    if(((mil&0x3FFFFF)==0)&&(flag==0)){
      flag = 1;
    }
    if(flag) {
      temp = micros() + 0x400000UL * 1000;
    }
    else  {
      temp = micros();
    }
  }
}

  // Create new Lcd_HandleTypeDef and initialize the hlcd
Lcd_HandleTypeDef hlcd;
static void command(uint8_t value);
static size_t write(uint8_t value);
static void send(uint8_t value, GPIO_PinState mode);
static void pulseEnable(void);
static void write4bits(uint8_t value);
void write8bits(uint8_t value);

void lcd_Init(uint8_t cols, uint8_t lines) {
  hlcd.data_ports = data_ports;  // LCD Data Ports , hlcd.data_ports
  hlcd.data_pins = data_pins;    // LCD Data Pins , hlcd.data_pins

#ifdef _LCD_4BITMODE_
    hlcd.displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
#endif
#ifdef _LCD_8BITMODE_
    hlcd.displayfunction = LCD_8BITMODE | LCD_1LINE | LCD_5x8DOTS;
#endif


if (lines > 1) {
    hlcd.displayfunction |= LCD_2LINE;
  }
  hlcd.numlines = lines;

  // for some 1 line displays you can select a 10 pixel high font
  if ((LCD_DOTSIZE != LCD_5x8DOTS) && (lines == 1)) {
    hlcd.displayfunction |= LCD_5x10DOTS;
  }

  // SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
  // according to datasheet, we need at least 40ms after power rises above 2.7V
  // before sending commands. Arduino can turn on way before 4.5V so we'll wait 50
  delay_ms(50);        //delayMicroseconds(50000); 
  // Now we pull both RS and R/W low to begin commands
  HAL_GPIO_WritePin(RS_GPIO_Port, RS_Pin, GPIO_PIN_RESET);  // digitalWrite(_rs_pin, LOW);
  HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_RESET);  // digitalWrite(_enable_pin, LOW);
  
  //put the LCD into 4 bit or 8 bit mode
#ifdef _LCD_4BITMODE_
    // this is according to the hitachi HD44780 datasheet
    // figure 24, pg 46

    // we start in 8bit mode, try to set 4 bit mode
    write4bits(0x03);
    delay_us(4500);    //delayMicroseconds(4500); // wait min 4.1ms

    // second try
    write4bits(0x03);
    delay_us(100);    //delayMicroseconds(4500); // wait min 4.1ms
    
    // third go!
    write4bits(0x03); 
    delay_us(100);    //delayMicroseconds(150);

    // finally, set to 4-bit interface
    write4bits(0x02);
    delay_us(100);
#endif
#ifdef _LCD_8BITMODE_
    // this is according to the hitachi HD44780 datasheet
    // page 45 figure 23

    // Send function set command sequence
    command(LCD_FUNCTIONSET | hlcd.displayfunction);
    delay_us(4500);     // delayMicroseconds(4500);  // wait more than 4.1ms

    // second try
    command(LCD_FUNCTIONSET | hlcd.displayfunction);
    delay_us(150);     // delayMicroseconds(150);

    // third go
    command(LCD_FUNCTIONSET | hlcd.displayfunction);
#endif

  // finally, set # lines, font size, etc.
  command(LCD_FUNCTIONSET | hlcd.displayfunction);  

  // turn the display on with no cursor or blinking default
  hlcd.displaycontrol = LCD_DISPLAYOFF | LCD_CURSOROFF | LCD_BLINKOFF;
  command(LCD_DISPLAYCONTROL | hlcd.displaycontrol);

  // clear it off
  lcd_clear();
  delay_ms(3);

  // Initialize to default text direction (for romance languages)
  hlcd.displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  // set the entry mode
  command(LCD_ENTRYMODESET | hlcd.displaymode);

  lcd_display();
}


/********** high level commands, for the user! */
void lcd_clear()
{
  command(LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
  delay_ms(2);                //  this command takes a long time!
}

void lcd_home()
{
  command(LCD_RETURNHOME);  // set cursor position to zero
  delay_ms(2);              // this command takes a long time!
}

void lcd_setCursor(uint8_t col, uint8_t row)
{
  uint8_t row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
  if ( row > hlcd.numlines ) {
    row = hlcd.numlines-1;    // we count rows starting w/0
  }
  
  command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void lcd_noDisplay() {
  hlcd.displaycontrol &= ~LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | hlcd.displaycontrol);
}

void lcd_display() {
  hlcd.displaycontrol |= LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | hlcd.displaycontrol);
}

// Turns the underline cursor on/off
void lcd_noCursor() {
  hlcd.displaycontrol &= ~LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | hlcd.displaycontrol);
}
void lcd_cursor() {
  hlcd.displaycontrol |= LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | hlcd.displaycontrol);
}

// Turn on and off the blinking cursor
void lcd_noBlink() {
  hlcd.displaycontrol &= ~LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | hlcd.displaycontrol);
}
void lcd_blink() {
  hlcd.displaycontrol |= LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | hlcd.displaycontrol);
}

// These commands scroll the display without changing the RAM
void lcd_scrollDisplayLeft(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void lcd_scrollDisplayRight(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void lcd_leftToRight(void) {
  hlcd.displaymode |= LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | hlcd.displaymode);
}

// This is for text that flows Right to Left
void lcd_rightToLeft(void) {
  hlcd.displaymode &= ~LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | hlcd.displaymode);
}

// This will 'right justify' text from the cursor
void lcd_autoscroll(void) {
  hlcd.displaymode |= LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | hlcd.displaymode);
}

// This will 'left justify' text from the cursor
void lcd_noAutoscroll(void) {
  hlcd.displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | hlcd.displaymode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void lcd_createChar(uint8_t location, uint8_t charmap[]) {
  location &= 0x7; // we only have 8 locations 0-7
  command(LCD_SETCGRAMADDR | (location << 3));
  for (int i=0; i<8; i++) {
    write(charmap[i]);
  }
}

//====================== ���� �� TEXT ó�� �Լ� ======================================
void lcd_putchar(char c) {
	write(c);
}

void lcd_string(char * str_data) {
   while(*str_data) {
	  write(*str_data++);
   }
}

void lcd_setCurStr(uint8_t col, uint8_t row,  char * str) {
	lcd_setCursor(col, row);
	lcd_string(str);
}

/*********** mid level commands, for sending data/cmds */

void command(uint8_t value) {
  send(value, GPIO_PIN_RESET);
}

size_t write(uint8_t value) {
  send(value, GPIO_PIN_SET);
  return 1; // assume sucess
}

/************ low level data pushing commands **********/

// write either command or data, with automatic 4/8-bit selection
void send(uint8_t value, GPIO_PinState mode) {
  HAL_GPIO_WritePin(RS_GPIO_Port, RS_Pin, mode);   // digitalWrite(_rs_pin, mode);

  #ifdef _LCD_8BITMODE_
    write8bits(value); 
  #endif
  #ifdef _LCD_4BITMODE_
    write4bits(value>>4);
    write4bits(value);
  #endif
}

void pulseEnable(void) {
  HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_RESET);
  delay_us(1);              //
  HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_SET);
  delay_us(1);   			   // enable pulse must be >450ns
  HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_RESET);
  delay_us(100);          // commands need > 37us to settle
}

#ifdef _LCD_4BITMODE_
void write4bits(uint8_t value) {
  for (uint8_t i = 0; i < 4; i++) {
    HAL_GPIO_WritePin(hlcd.data_ports[i], hlcd.data_pins[i],  (value >> i) & 0x01 );
  }

  pulseEnable();
}
#endif

#ifdef _LCD_8BITMODE_
void write8bits(uint8_t value) {
  for (uint8_t i = 0; i < 8; i++) {
	  HAL_GPIO_WritePin(hlcd.data_ports[i], hlcd.data_pins[i],  (value >> i) & 0x01 );
  }
  
  pulseEnable();
}
#endif
