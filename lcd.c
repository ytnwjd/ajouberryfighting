#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

// GPIO

// Define some device parameters
#define I2C_ADDR   0x27 // I2C device address
// Define some device constants
#define LCD_CHR  1 // Mode - Sending data
#define LCD_CMD  0 // Mode - Sending command
#define LINE1  0x80 // 1st line
#define LINE2  0xC0 // 2nd line

#define LCD_BACKLIGHT   0x08  // On
// LCD_BACKLIGHT = 0x00  # Off

#define ENABLE  0b00000100 // Enable bit

void lcd_init(int fd);
void lcd_byte(int bits, int mode, int fd);
void lcd_toggle_enable(int bits, int fd);
void lcdStart(int line, int fd, char* string);

// added by Lewis
void typeInt(int i, int fd);
void typeFloat(float myFloat, int fd);
void lcdLoc(int line, int fd); //move cursor
void ClrLcd(int fd); // clr LCD return home
void typeln(const char* s, int fd);
void typeChar(char val, int fd);



int main(){

    int fd;

    if (wiringPiSetup() == -1) exit(1);
    fd = wiringPiI2CSetup(I2C_ADDR);
    lcd_init(fd); // setup LCD

    lcdStart(LINE1, fd, "Hello"); //lcd화면에 표시


    return 0;
}

// this allows use of any size string
void typeln(const char* s,int fd) {
    while (*s) lcd_byte(*(s++), LCD_CHR,fd);

}

// int to string
void typeInt(int i, int fd) {
    char array1[20];
    sprintf(array1, "%d", i);
    typeln(array1,fd);
}

void typeFloat(float myFloat,int fd) {
    char buffer[20];
    sprintf(buffer, "%4.2f", myFloat);
    typeln(buffer,fd);
}

void lcd_byte(int bits, int mode,int fd) {

    //Send byte to data pins
    // bits = the data
    // mode = 1 for data, 0 for command
    int bits_high;
    int bits_low;
    // uses the two half byte writes to LCD
    bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    // High bits
    wiringPiI2CReadReg8(fd, bits_high);
    lcd_toggle_enable(bits_high,fd);

    // Low bits
    wiringPiI2CReadReg8(fd, bits_low);
    lcd_toggle_enable(bits_low,fd);
}

// clr lcd go home loc 0x80
void ClrLcd(int fd) {
    lcd_byte(0x01, LCD_CMD,fd);
    lcd_byte(0x02, LCD_CMD,fd);
}

// go to location on LCD
void lcdLoc(int line,int fd) {
    lcd_byte(line, LCD_CMD,fd);
}

// out char to LCD at current position
void typeChar(char val,int fd) {
    lcd_byte(val, LCD_CHR,fd);
}


void lcd_toggle_enable(int bits,int fd) {
    // Toggle enable pin on LCD display
    delayMicroseconds(500);
    wiringPiI2CReadReg8(fd, (bits | ENABLE));
    delayMicroseconds(500);
    wiringPiI2CReadReg8(fd, (bits & ~ENABLE));
    delayMicroseconds(500);
}


void lcd_init(int fd) {
    // Initialise display
    lcd_byte(0x33, LCD_CMD,fd); // Initialise
    lcd_byte(0x32, LCD_CMD,fd); // Initialise
    lcd_byte(0x06, LCD_CMD,fd); // Cursor move direction
    lcd_byte(0x0C, LCD_CMD,fd); // 0x0F On, Blink Off
    lcd_byte(0x28, LCD_CMD,fd); // Data length, number of lines, font size
    lcd_byte(0x01, LCD_CMD,fd); // Clear display
    delayMicroseconds(500);
}

void lcdStart(int line, int fd, char* string) {
    lcdLoc(line,fd);
    typeln(string,fd);
}
